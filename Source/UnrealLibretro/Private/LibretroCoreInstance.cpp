
#include "LibretroCoreInstance.h"

#include "libretro/libretro.h"

#include "Misc/FileHelper.h"

#include "UnrealLibretro.h"
#include "LibretroInputComponent.h"
#include "RawAudioSoundWave.h"
#include "sdlarch.h"

#define NOT_LAUNCHED_GUARD if (!CoreInstance.IsSet()) { UE_LOG(Libretro, Warning, TEXT("Called function '%hs' before Libretro Core '%s' was launched. This has no effect"), __func__, *CorePath) return; }

template<typename T>
TRefCountPtr<T>&& Replace(TRefCountPtr<T> & a, TRefCountPtr<T> && b)
{
    a.Swap(b);
    return MoveTemp(b);
}

// The procedure below is a pretty nasty crutch. I need it because I don't know how to reason about the lifetime of the background threads running the Libretro Cores.
// If I could find some engine hook that can defer the loading of levels until all IO Operations I perform are finished I could get rid of this.
// If you didn't have this crutch potentially a race condition can occur if say you have a instance setup to be persistent (Saves state when its destroyed; loads state when its created)
// And you don't synchronize the lifetimes of the instances which asynchronously write to the save files to be mutually exclusive and ordered between levels then you
// can imagine a scenario where you reload a level and the loading state operation of the new instance happens before the saving sate of old instance and
// thus you load an old state and the user loses progress or even worse the save file is corrupted from a simultaneous read and write.
TMap<FString, FGraphEventRef> LastIOTask; // @dynamic

TUniqueFunction<void(TUniqueFunction<void(const FString&)>)> GameThread_PrepareBlockingOrderedOperation(FString Key) // @todo trigger assert if the operation is released before being called
{
    check(IsInGameThread());

    auto ThisOperation = TGraphTask<FNullGraphTask>::CreateTask().ConstructAndHold(TStatId(), ENamedThreads::AnyThread);

    return [Key = MoveTemp(Key), ThisOperation, LastOperation = Replace(LastIOTask.FindOrAdd(Key), ThisOperation->GetCompletionEvent())]
    (auto OrderedOperation)
    {
        if (LastOperation)
        {
            FTaskGraphInterface::Get().WaitUntilTaskCompletes(LastOperation);
        }

        OrderedOperation(Key);
        ThisOperation->Unlock(); // This uses _InterlockedCompareExchange which has a memory barrier so it should be thread-safe
    };
}

ULibretroCoreInstance::ULibretroCoreInstance()
    : InputState(MakeShared<TStaticArray<FLibretroInputState, PortCount>, ESPMode::ThreadSafe>())
{
    PrimaryComponentTick.bCanEverTick = false;
    bWantsInitializeComponent = true;
    InputMap.AddZeroed(PortCount);
}
 
void ULibretroCoreInstance::ConnectController(APlayerController* PlayerController, int Port, TMap<FKey, ERetroInput> ControllerBindings, FOnControllerDisconnected OnControllerDisconnected)
{
    check(Port >= 0 && Port < PortCount);

    DisconnectController(Port);

    Controller[Port] = MakeWeakObjectPtr(PlayerController);
    Disconnected[Port] = OnControllerDisconnected;

    InputMap[Port]->KeyBindings.Empty();
    InputMap[Port]->BindKeys(ControllerBindings);
    PlayerController->PushInputComponent(InputMap[Port]);
}

void ULibretroCoreInstance::DisconnectController(int Port)
{
    check(Port >= 0 && Port < PortCount);

    if (Controller[Port].IsValid())
    {
        for (auto &input : InputState.Get()[Port].digital)
        {
            input.store(0, std::memory_order_relaxed);
        }
    	
        InputState.Get()[Port].analog[0][0].store(0, std::memory_order_relaxed);
        InputState.Get()[Port].analog[0][1].store(0, std::memory_order_relaxed);
        InputState.Get()[Port].analog[1][0].store(0, std::memory_order_relaxed);
        InputState.Get()[Port].analog[1][1].store(0, std::memory_order_relaxed);
    	
        Controller[Port]->PopInputComponent(InputMap[Port]);
        Disconnected[Port].ExecuteIfBound(Controller[Port].Get(), Port);
    }
}

void ULibretroCoreInstance::Launch() 
{
    Shutdown();
    
    auto _CorePath = FUnrealLibretroModule::ResolveCorePath(this->CorePath);
    auto _RomPath  = FUnrealLibretroModule::ResolveROMPath (this->RomPath);

    if (!IPlatformFile::GetPlatformPhysical().FileExists(*_CorePath))
    {
        UE_LOG(Libretro, Warning, TEXT("Failed to launch Libretro core '%s'. Couldn't find core at path '%s'"), *_CorePath, *_CorePath);
        return;
    }
    else if (!IPlatformFile::GetPlatformPhysical().FileExists(*_RomPath) && !IPlatformFile::GetPlatformPhysical().DirectoryExists(*_RomPath))
    {
        UE_LOG(Libretro, Warning, TEXT("Failed to launch Libretro core '%s'. Couldn't find ROM at path '%s'"), *_CorePath, *_RomPath);
        return;
    }

    typedef URawAudioSoundWave AudioBufferInternal;
    AudioBuffer = NewObject<AudioBufferInternal>();

    if (!RenderTarget)
    {
        RenderTarget = NewObject<UTextureRenderTarget2D>();
    }

    RenderTarget->Filter = TF_Nearest; // @todo remove this

    this->CoreInstance = LibretroContext::Launch(_CorePath, _RomPath, RenderTarget, static_cast<AudioBufferInternal*>(AudioBuffer), InputState,
	    [weakThis = MakeWeakObjectPtr(this), OrderedFileAccess = GameThread_PrepareBlockingOrderedOperation(FUnrealLibretroModule::ResolveSRAMPath(_RomPath, SRAMPath))]
        (libretro_api_t &libretro_api, bool bottom_left_origin) 
	    {   // Core has loaded
            
	        // Load save data into core
		    OrderedFileAccess( // @todo this is just a weird place to hook this in
                [&libretro_api](FString SRAMPath)
                {
                    auto File = IPlatformFile::GetPlatformPhysical().OpenRead(*SRAMPath);
                    if (File && libretro_api.get_memory_size(RETRO_MEMORY_SAVE_RAM))
                    {
                        File->Read((uint8*)libretro_api.get_memory_data(RETRO_MEMORY_SAVE_RAM), 
                                           libretro_api.get_memory_size(RETRO_MEMORY_SAVE_RAM));
                        File->~IFileHandle(); // must be called explicitly
                    }
                });
	        
	        // Notify delegate
	        FGraphEventRef Task = FFunctionGraphTask::CreateAndDispatchWhenReady(
                [=]()
	            {
	                if (weakThis.IsValid())
	                {
	                    weakThis->OnCoreIsReady.Broadcast(weakThis->RenderTarget, 
                                                          weakThis->AudioBuffer,
                                                          bottom_left_origin);
	                }
	            }, TStatId(), nullptr, ENamedThreads::GameThread);
	    });
}

void ULibretroCoreInstance::Pause(bool ShouldPause)
{
    NOT_LAUNCHED_GUARD

    CoreInstance.GetValue()->Pause(ShouldPause);
    Paused = ShouldPause;
}

void ULibretroCoreInstance::Shutdown() {
    
    NOT_LAUNCHED_GUARD

    LibretroContext::Shutdown(CoreInstance.GetValue());
    CoreInstance.Reset();
}

void ULibretroCoreInstance::LoadState(const FString& FilePath)
{
    NOT_LAUNCHED_GUARD

    CoreInstance.GetValue()->EnqueueTask(
        [CorePath = this->CorePath, OrderedSaveStateFileAccess = GameThread_PrepareBlockingOrderedOperation(FUnrealLibretroModule::ResolveSaveStatePath(RomPath, FilePath))]
        (auto libretro_api)
        {
            OrderedSaveStateFileAccess(
            [&libretro_api, &CorePath]
            (auto SaveStatePath)
            {
                TArray<uint8> SaveStateBuffer;

                if (!FFileHelper::LoadFileToArray(SaveStateBuffer, *SaveStatePath))
                {
                    UE_LOG(Libretro, Warning, TEXT("Couldn't load save state '%s' error code:%u"), *SaveStatePath, FPlatformMisc::GetLastError());
                    return; // We just assume failure means the file did not exist and we do nothing
                }

                if (SaveStateBuffer.Num() != libretro_api.serialize_size()) // because of emulator versions these might not match up also some Libretro cores don't follow spec so the size can change between calls to serialize_size
                {
                    UE_LOG(Libretro, Warning, TEXT("Save state file size specified by '%s' did not match the save state size in folder. File Size : %d Core Size: %zu. Going to try to load it anyway."), *CorePath, SaveStateBuffer.Num(), libretro_api.serialize_size())
                }

                libretro_api.unserialize(SaveStateBuffer.GetData(), SaveStateBuffer.Num());
            });
        });
}

// This function is disgusting I'm sorry
void ULibretroCoreInstance::SaveState(const FString& FilePath)
{
	NOT_LAUNCHED_GUARD

	TArray<uint8> *SaveStateBuffer = new TArray<uint8>(); // @dynamic

	FGraphEventArray Prerequisites{ LastIOTask.FindOrAdd(FUnrealLibretroModule::ResolveSaveStatePath(RomPath, FilePath)) };
	
    // This async task is executed second
	auto SaveStateToFileTask = TGraphTask<FFunctionGraphTask>::CreateTask(Prerequisites[0].IsValid() ? &Prerequisites : nullptr).ConstructAndHold
	(
		[SaveStateBuffer, SaveStatePath = FUnrealLibretroModule::ResolveSaveStatePath(RomPath, FilePath)]() // @dynamic The capture here does a copy on the heap probably
		{
			FFileHelper::SaveArrayToFile(*SaveStateBuffer, 
                                         *SaveStatePath);
			delete SaveStateBuffer;
		}
		, TStatId(), ENamedThreads::AnyThread
	);
	
	LastIOTask[FUnrealLibretroModule::ResolveSaveStatePath(RomPath, FilePath)] = SaveStateToFileTask->GetCompletionEvent();
	
	// This async task is executed first
	this->CoreInstance.GetValue()->EnqueueTask
	(
		[SaveStateBuffer, SaveStateToFileTask](libretro_api_t& libretro_api)
		{
			SaveStateBuffer->Reserve(libretro_api.serialize_size() + 2); // The plus two is a slight optimization based on how SaveArrayToFile works
			SaveStateBuffer->AddUninitialized(libretro_api.serialize_size());
			libretro_api.serialize(static_cast<void*>(SaveStateBuffer->GetData()), libretro_api.serialize_size());

			SaveStateToFileTask->Unlock();
		}
	);
}

#include "Scalability.h"

void ULibretroCoreInstance::InitializeComponent() {

    for (int Port = 0; Port < PortCount; Port++)
    {
        InputMap[Port] = NewObject<ULibretroInputComponent>();
        InputMap[Port]->Initialize(&(*InputState)[Port], [Port, this]() { this->DisconnectController(Port); });
    }
}

void ULibretroCoreInstance::BeginPlay()
{
    Super::BeginPlay();

    /*if (Scalability::GetQualityLevels().AntiAliasingQuality) {
        FMessageDialog::Open(EAppMsgType::Ok, FText::AsCultureInvariant("You have temporal anti-aliasing enabled. The emulated games will look will look blurry and laggy if you leave this enabled. If you happen to know how to fix this let me know. I tried enabling responsive AA on the material to prevent this, but that didn't work."));
    }*/
}

void ULibretroCoreInstance::BeginDestroy()
{
    for (int Port = 0; Port < PortCount; Port++)
    {
        DisconnectController(Port);
    }

    if (this->CoreInstance.IsSet())
    {
        // Save SRam
        this->CoreInstance.GetValue()->EnqueueTask(
            [AccessSRAMFileOrdered = GameThread_PrepareBlockingOrderedOperation(FUnrealLibretroModule::ResolveSRAMPath(RomPath, SRAMPath))](auto libretro_api)
            {
	            AccessSRAMFileOrdered(
                    [&libretro_api](auto SRAMPath)
                    {
                        auto SRAMBuffer = TArrayView<const uint8>((uint8*)libretro_api.get_memory_data(RETRO_MEMORY_SAVE_RAM),
                                                                          libretro_api.get_memory_size(RETRO_MEMORY_SAVE_RAM));
                        FFileHelper::SaveArrayToFile(SRAMBuffer, *SRAMPath);
                    });
            });

        Shutdown();
    }

    Super::BeginDestroy();
}
