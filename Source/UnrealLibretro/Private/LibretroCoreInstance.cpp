// Fill out your copyright notice in the Description page of Project Settings.


#include "LibretroCoreInstance.h"
#include "LibretroInputComponent.h"
#include "libretro/libretro.h"

#include "RHIResources.h"
#include "LambdaRunnable.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/MessageDialog.h"


#include "Interfaces/IPluginManager.h"

#define NOT_LAUNCHED_GUARD if (!CoreInstance.IsSet()) { return; }

template<typename T>
TRefCountPtr<T>&& Replace(TRefCountPtr<T> & a, TRefCountPtr<T> && b)
{
    a.Swap(b);
    return MoveTemp(b);
}

TMap<FString, FGraphEventRef> LastIOTask; // @dynamic
// This is a pretty nasty crutch just because I don't know how to reason about the lifetime of the background task running the libretro cores.
template <typename... TArgs>
FGraphEventRef AccessFileOrdered(FString FilePath, TFunction<void(FString, TArgs...)> IOOperation, TFunction<void(TUniqueFunction<void(TArgs...)>)> EnqueueTask)
{
    check(IsInGameThread());

    auto ThisIOOperation = TGraphTask<FNullGraphTask>::CreateTask().ConstructAndHold(TStatId(), ENamedThreads::AnyThread);

    EnqueueTask
    (
        [IOOperation, FilePath, ThisIOOperation, LastIOOperation = Replace(LastIOTask.FindOrAdd(FilePath), ThisIOOperation->GetCompletionEvent())]
        (auto Args)
        {
            if (LastIOOperation)
            {
                FTaskGraphInterface::Get().WaitUntilTaskCompletes(LastIOOperation);
            }
        	
            IOOperation(FilePath, Args);
            ThisIOOperation->Unlock(); // This uses _InterlockedCompareExchange which has a memory barrier so it should be thread-safe
        }
    );

    return LastIOTask[FilePath];
}
// todo merge the above function into this one
FGraphEventRef ULibretroCoreInstance::AccessFileOrderedOnCoreThread(FString FilePath, TFunction<void(FString, libretro_api_t&)> IOOperation)
{
    return AccessFileOrdered(FilePath,
                             IOOperation,
                    TFunction<void(TUniqueFunction<void(libretro_api_t&)>)>(
				       [this](auto IOOperation)
						     {
						         this->CoreInstance.GetValue()->EnqueueTask(MoveTemp(IOOperation));
						     }
                            )
                            );
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

    auto PlayerController = Controller[Port];

    if (PlayerController.IsValid())
    {
        PlayerController->PopInputComponent(InputMap[Port]);
    }

    Disconnected[Port].ExecuteIfBound(PlayerController.Get(), Port);
}

void ULibretroCoreInstance::Launch() 
{
    Shutdown();

    Rom = Rom.IsEmpty() ? "MAZE" : Rom;
    Core = Core.IsEmpty() ? "emux_chip8_libretro.dll" : Core;

    auto LibretroPluginRootPath = IPluginManager::Get().FindPlugin("UnrealLibretro")->GetBaseDir();
    auto CorePath = FPaths::Combine(LibretroPluginRootPath, TEXT("MyCores"), Core);
    auto RomPath  = FPaths::Combine(LibretroPluginRootPath, TEXT("MyROMs"), Rom );

    if (!IPlatformFile::GetPlatformPhysical().FileExists(*CorePath))
    {
        FMessageDialog::Open(EAppMsgType::Ok, FText::AsCultureInvariant("Failed to launch Libretro core. Couldn't find core at path " + IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*CorePath)));
        return;
    }
    else if (!IPlatformFile::GetPlatformPhysical().FileExists(*RomPath) && !IPlatformFile::GetPlatformPhysical().DirectoryExists(*RomPath))
    {
        FMessageDialog::Open(EAppMsgType::Ok, FText::AsCultureInvariant("Failed to launch Libretro core " + Core + ". Couldn't find ROM at path " + IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*RomPath)));
        return;
    }


    AudioBuffer = NewObject<URawAudioSoundWave>();

    if (!RenderTarget)
    {
        RenderTarget = NewObject<UTextureRenderTarget2D>();
    }

    RenderTarget->Filter = TF_Nearest;

    LibretroContext::Launch(CorePath, RomPath, RenderTarget, AudioBuffer, InputState,
        [weakThis = MakeWeakObjectPtr(this)](bool bottom_left_origin) 
            { // Core Loaded
            FGraphEventRef Task = FFunctionGraphTask::CreateAndDispatchWhenReady([=]()
                {
                    if (weakThis.IsValid())
                    {
                        weakThis->OnCoreIsReady.Broadcast(weakThis->RenderTarget, weakThis->AudioBuffer, bottom_left_origin);
                    }

                }, TStatId(), nullptr, ENamedThreads::GameThread);
            },
            [this](LibretroContext *JustConstructedContext)
            {
                this->CoreInstance = JustConstructedContext; // todo its kind of crazy to have this hook here
                AccessFileOrderedOnCoreThread(SRAMPath("Default"),
                    [](auto SRAMPath, auto libretro_api)
                    {
                        auto File = IPlatformFile::GetPlatformPhysical().OpenRead(*SRAMPath);
                        if (File && libretro_api.retro_get_memory_size(RETRO_MEMORY_SAVE_RAM))
                        {
                            File->Read((uint8*)libretro_api.retro_get_memory_data(RETRO_MEMORY_SAVE_RAM), libretro_api.retro_get_memory_size(RETRO_MEMORY_SAVE_RAM));
                            File->~IFileHandle(); // must be called explicitly
                        }
                    }
                );
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

// These functions are clusters. They will be rewritten... eventually.
void ULibretroCoreInstance::LoadState(const FString Identifier)
{
    NOT_LAUNCHED_GUARD

    AccessFileOrderedOnCoreThread(this->SaveStatePath(Identifier),
        [Core = this->Core](auto SavePath, auto libretro_api)
        {
            TArray<uint8> SaveStateBuffer;

            if (!FFileHelper::LoadFileToArray(SaveStateBuffer, *SavePath))
            {
                return; // We just assume failure means the file did not exist and we do nothing
            }

            if (SaveStateBuffer.Num() != libretro_api.retro_serialize_size()) // because of emulator versions these might not match up also some Libretro cores don't follow spec
            {
                UE_LOG(Libretro, Warning, TEXT("Save state file size specified by %s did not match the save state size in folder. File Size : %d Core Size: %zu. Going to try to load it anyway."), *Core, SaveStateBuffer.Num(), libretro_api.retro_serialize_size()) 
            }

            libretro_api.retro_unserialize(SaveStateBuffer.GetData(), SaveStateBuffer.Num());
        }
    );
}

void ULibretroCoreInstance::SaveState(const FString Identifier)
{
	NOT_LAUNCHED_GUARD

	TArray<uint8> *SaveStateBuffer = new TArray<uint8>(); // @dynamic

	FGraphEventArray Prerequisites{ LastIOTask.FindOrAdd(this->SaveStatePath(Identifier)) };
	
    // This async task is executed second
	auto SaveStateToFileTask = TGraphTask<FFunctionGraphTask>::CreateTask(Prerequisites[0].IsValid() ? &Prerequisites : nullptr).ConstructAndHold
	(
		[SaveStateBuffer, SaveStatePath = this->SaveStatePath(Identifier)]() // @dynamic The capture here does a copy on the heap probably
		{
			FFileHelper::SaveArrayToFile(*SaveStateBuffer, *SaveStatePath, &IFileManager::Get(), FILEWRITE_None);
			delete SaveStateBuffer;
		}
		, TStatId(), ENamedThreads::AnyThread
	);
	
	LastIOTask[this->SaveStatePath(Identifier)] = SaveStateToFileTask->GetCompletionEvent();
	
	// This async task is executed first
	this->CoreInstance.GetValue()->EnqueueTask
	(
		[SaveStateBuffer, SaveStateToFileTask](libretro_api_t& libretro_api)
		{
			SaveStateBuffer->Reserve(libretro_api.retro_serialize_size() + 2); // The plus two is a slight optimization based on how SaveArrayToFile works
			SaveStateBuffer->AddUninitialized(libretro_api.retro_serialize_size());
			libretro_api.retro_serialize(static_cast<void*>(SaveStateBuffer->GetData()), libretro_api.retro_serialize_size());

			SaveStateToFileTask->Unlock(); // This uses _InterlockedCompareExchange which has a memory barrier so it should be thread-safe
		}
	);
}

#include "Editor.h"
#include "Scalability.h"

void ULibretroCoreInstance::InitializeComponent() {
    ResumeEditor = FEditorDelegates::ResumePIE.AddLambda([this](const bool bIsSimulating)
        {
            // This could have weird behavior if CoreInstance is launched when the editor is paused. That really shouldn't ever happen though
            NOT_LAUNCHED_GUARD
            this->CoreInstance.GetValue()->Pause(Paused);
        });
    PauseEditor = FEditorDelegates::PausePIE.AddLambda([this](const bool bIsSimulating)
        {
            NOT_LAUNCHED_GUARD
            this->CoreInstance.GetValue()->Pause(true);
        });

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

// @todo: Using this function I think would be the proper way of keeping the UObjects I'm using in LibretroContext alive instead of the weak pointer shared pointer mess I'm currently using. However its kind of a low priority since the current system I'm using works fine.
//		  The thing I worry about though is if delaying destruction for a few seconds is simply too long Unreal AFAIK only uses this to delay destruction until render resources are deleted which is no more than a few frames
//bool ULibretroCoreInstance::IsReadyForFinishDestroy() { return false;  }

void ULibretroCoreInstance::BeginDestroy()
{
    for (int Port = 0; Port < PortCount; Port++)
    {
        DisconnectController(Port);
    }

    if (this->CoreInstance.IsSet())
    {
        // Save SRam
        AccessFileOrderedOnCoreThread(SRAMPath("Default"),
            [](auto SRAMPath, auto libretro_api)
            {
                FFileHelper::SaveArrayToFile
                (
                    TArrayView<const uint8>((uint8*)libretro_api.retro_get_memory_data(RETRO_MEMORY_SAVE_RAM), libretro_api.retro_get_memory_size(RETRO_MEMORY_SAVE_RAM)),
                    *SRAMPath
                );
            }
        );
    }
    
    Shutdown();

    FEditorDelegates::ResumePIE.Remove(ResumeEditor);
    FEditorDelegates::PausePIE .Remove(PauseEditor );

    Super::BeginDestroy();
}
