
#include "LibretroCoreInstance.h"

#include "libretro/libretro.h"

#include "Misc/FileHelper.h"
#include "Components/AudioComponent.h"
#include "GameFramework/PlayerInput.h"

#include "UnrealLibretro.h"
#include "LibretroInputDefinitions.h"
#include "RawAudioSoundWave.h"
#include "sdlarch.h"

#define NOT_LAUNCHED_GUARD if (!CoreInstance.IsSet()) return;

ULibretroCoreInstance::ULibretroCoreInstance()
{
    PrimaryComponentTick.bCanEverTick = true;
}

 
void ULibretroCoreInstance::ConnectController(APlayerController*       PlayerController,
                                             int                       Port,
                                             TMap<FKey, ERetroInput>   ControllerBindings,
                                             FOnControllerDisconnected OnControllerDisconnected,
                                             bool                      ForwardAllKeyboardInputToCoreIfPossible)
{
    check(Port >= 0 && Port < PortCount);

    DisconnectController(Port);

    Bindings[Port] = ControllerBindings;
    Controller[Port] = MakeWeakObjectPtr(PlayerController);
    Disconnected[Port] = OnControllerDisconnected;

    ForwardKeyboardInput[Port] = ForwardAllKeyboardInputToCoreIfPossible;
}

void ULibretroCoreInstance::DisconnectController(int Port)
{
    check(Port >= 0 && Port < PortCount);

    if (Controller[Port].IsValid())
    {
        if (CoreInstance.IsSet())
        {
            CoreInstance.GetValue()->EnqueueTask([&InputStatePort = this->CoreInstance.GetValue()->InputState[Port]](libretro_api_t& libretro_api)
            {
                InputStatePort = { { 0 } };

                if (libretro_api.keyboard_event)
                {
                    for (int k = RETROK_FIRST; k < RETROK_LAST; k++)
                    {
                        libretro_api.keyboard_event(false, k, 0, RETROKMOD_NONE);
                    }
                }
            });
        }
    	
        Disconnected[Port].ExecuteIfBound(Controller[Port].Get(), Port);
        Controller[Port] = nullptr;
    }
}

void ULibretroCoreInstance::Launch() 
{
    Shutdown();
    
    auto _CorePath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*FUnrealLibretroModule::ResolveCorePath(this->CorePath));
    auto _RomPath  = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*FUnrealLibretroModule::ResolveROMPath (this->RomPath));

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

    if (!RenderTarget)
    {
        RenderTarget = NewObject<UTextureRenderTarget2D>();
    }

    AudioBuffer = NewObject<URawAudioSoundWave>();

    RenderTarget->Filter = TF_Nearest; // @todo remove this

    this->CoreInstance = LibretroContext::Launch(_CorePath, _RomPath, RenderTarget, static_cast<URawAudioSoundWave*>(AudioBuffer),
	    [weakThis = MakeWeakObjectPtr(this), SRAMPath = FUnrealLibretroModule::ResolveSRAMPath(_RomPath, SRAMPath)]
        (LibretroContext *_CoreInstance, libretro_api_t &libretro_api) 
	    {   // Core has loaded
            
	        // Load save data into core @todo this is just a weird place to hook this in
            auto File = IPlatformFile::GetPlatformPhysical().OpenRead(*SRAMPath);
            if (File && libretro_api.get_memory_size(RETRO_MEMORY_SAVE_RAM))
            {
                File->Read((uint8*)libretro_api.get_memory_data(RETRO_MEMORY_SAVE_RAM), 
                                   libretro_api.get_memory_size(RETRO_MEMORY_SAVE_RAM));
                File->~IFileHandle(); // must be called explicitly
            }
	        
	        // Notify delegate
	        FGraphEventRef Task = FFunctionGraphTask::CreateAndDispatchWhenReady(
                [weakThis, 
                 bottom_left_origin = _CoreInstance->LibretroThread_bottom_left_origin,
                 geometry           = _CoreInstance->LibretroThread_geometry]()
	            {
	                if (weakThis.IsValid())
	                {
	                    weakThis->OnCoreIsReady.Broadcast(weakThis->RenderTarget, 
                                                          weakThis->AudioBuffer);

                        weakThis->bottom_left_origin = bottom_left_origin;
                        weakThis->base_width = geometry.base_width;
                        weakThis->base_height = geometry.base_height;
                        
                        weakThis->NotifyOnFramebufferResizeDelegate();
	                	
                        weakThis->AudioComponent->SetSound(weakThis->AudioBuffer);
                        weakThis->AudioComponent->Play();
	                }
	            }, TStatId(), nullptr, ENamedThreads::GameThread);
	    });
    
    // @todo theres a data race with how I assign this
    this->CoreInstance.GetValue()->CoreEnvironmentCallback = [weakThis = MakeWeakObjectPtr(this), CoreInstance = this->CoreInstance.GetValue()](unsigned cmd, void* data)->bool
    {
        switch (cmd)
        {
            case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO: {
                FFunctionGraphTask::CreateAndDispatchWhenReady(
                    [weakThis,
                     system_av_info = *(const struct retro_system_av_info*)data]()
                    {
                        if (weakThis.IsValid())
                        {
                            { // @hack to change audio playback sample-rate
                                auto AudioQueue = static_cast<URawAudioSoundWave*>(weakThis->AudioBuffer)->AudioQueue;
                                weakThis->AudioBuffer = NewObject<URawAudioSoundWave>();
                                weakThis->AudioBuffer->SetSampleRate(system_av_info.timing.sample_rate);
                                weakThis->AudioBuffer->NumChannels = 2;
                                static_cast<URawAudioSoundWave*>(weakThis->AudioBuffer)->AudioQueue = AudioQueue;
                                weakThis->AudioComponent->SetSound(weakThis->AudioBuffer);
                            }

                            weakThis->base_width  = system_av_info.geometry.base_width;
                            weakThis->base_height = system_av_info.geometry.base_height;
                            weakThis->NotifyOnFramebufferResizeDelegate();
                        }
                    }, TStatId(), nullptr, ENamedThreads::GameThread);

                return true;
            }
            case RETRO_ENVIRONMENT_SET_ROTATION: {
                FFunctionGraphTask::CreateAndDispatchWhenReady(
                    [weakThis,
                     rotation = *(const unsigned*)data]()
                {
                    if (weakThis.IsValid())
                    {
                        weakThis->rotation_hertz = rotation / 4.f;
                        weakThis->NotifyOnFramebufferResizeDelegate();
                    }
                }, TStatId(), nullptr, ENamedThreads::GameThread);
                
                return true;
            }
            case RETRO_ENVIRONMENT_SET_GEOMETRY: {
                auto geometry = (const struct retro_game_geometry*) data;
            		
                FFunctionGraphTask::CreateAndDispatchWhenReady(
                    [weakThis,
                     geometry = *(const struct retro_game_geometry*)data]()
                {
                    if (weakThis.IsValid())
                    {
                        weakThis->base_width  = geometry.base_width;
                        weakThis->base_height = geometry.base_height;
                        weakThis->NotifyOnFramebufferResizeDelegate();
                    }
                }, TStatId(), nullptr, ENamedThreads::GameThread);
                
                return true;
	        }
        }

        return false;
    };
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
        [CorePath = this->CorePath, SaveStatePath = FUnrealLibretroModule::ResolveSaveStatePath(RomPath, FilePath)]
        (auto libretro_api)
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
}

void ULibretroCoreInstance::SaveState(const FString& FilePath)
{
	NOT_LAUNCHED_GUARD
	
	this->CoreInstance.GetValue()->EnqueueTask
	(
		[SaveStatePath = FUnrealLibretroModule::ResolveSaveStatePath(RomPath, FilePath)](libretro_api_t& libretro_api)
		{
            TArray<uint8> SaveStateBuffer; // @dynamic
			SaveStateBuffer.Reserve(libretro_api.serialize_size() + 2); // The plus two is a slight optimization based on how SaveArrayToFile works
			SaveStateBuffer.AddUninitialized(libretro_api.serialize_size());
			libretro_api.serialize(static_cast<void*>(SaveStateBuffer.GetData()), libretro_api.serialize_size());
		}
	);
}

#include "Scalability.h"

void ULibretroCoreInstance::BeginPlay()
{
    Super::BeginPlay();

    /*if (Scalability::GetQualityLevels().AntiAliasingQuality) {
        FMessageDialog::Open(EAppMsgType::Ok, FText::AsCultureInvariant("You have temporal anti-aliasing enabled. The emulated games will look will look blurry and laggy if you leave this enabled. If you happen to know how to fix this let me know. I tried enabling responsive AA on the material to prevent this, but that didn't work."));
    }*/
}

#include <type_traits>

template<typename E>
static constexpr auto to_integral(E e) -> typename std::underlying_type<E>::type
{
    return static_cast<typename std::underlying_type<E>::type>(e);
}

void ULibretroCoreInstance::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
#if ENGINE_MAJOR_VERSION <= 4 && ENGINE_MINOR_VERSION <= 25
    #define IsAxis1D IsFloatAxis
#endif

    if (CoreInstance.IsSet()) 
    {
        for (int Port = 0; Port < PortCount; Port++)
        {
            if (!Controller[Port].IsValid()) continue;

            FLibretroInputState NextInputState = {{0}};
            for (auto ControllerBinding : Bindings[Port])
            {
                if (ControllerBinding.Key.IsAxis1D()) 
                {
                    // Both Y-Axes should be inverted because of Libretro convention however Unreal has a quirk where the Y-Axis of the right stick is inverted by default for some reason
                    //                         LX  RX  LY  RY
                    float StickAxisInvert[] = { 1,  1, -1,  1 };
                    int StickAxisIndex = to_integral(ControllerBinding.Value) - to_integral(ERetroInput::LeftX);
                    // Some cores support 0x7FFF to -0x7FFF others to -0x8000. However I support only 0x7FFF to -0x7FFF
                    auto retro_value = (int16_t)FMath::RoundHalfToEven(StickAxisInvert[StickAxisIndex] * 0x7FFF * Controller[Port]->PlayerInput->GetKeyValue(ControllerBinding.Key)); 
                    reinterpret_cast<int16_t*>(NextInputState.analog)[StickAxisIndex] = int16_t(FMath::Clamp(reinterpret_cast<int16_t*>(NextInputState.analog)[StickAxisIndex] + retro_value, -0x7FFF, 0x7FFF));
                }
                else
                {
                    NextInputState.digital[to_integral(ControllerBinding.Value)] |= static_cast<unsigned>(Controller[Port]->PlayerInput->IsPressed(ControllerBinding.Key));
                }
            }

            CoreInstance.GetValue()->EnqueueTask([&InputStatePort = this->CoreInstance.GetValue()->InputState[Port], NextInputState](auto)
            {
                InputStatePort = NextInputState;
            });

            if (ForwardKeyboardInput[Port])
            {
                for (int i = 0; i < count_key_bindings; i++)
                {
                    if (   Controller[Port]->PlayerInput->WasJustPressed(key_bindings[i].Unreal)
                        || Controller[Port]->PlayerInput->WasJustReleased(key_bindings[i].Unreal))
                    {
                        this->CoreInstance.GetValue()->EnqueueTask(
                            [=, down = Controller[Port]->PlayerInput->WasJustPressed(key_bindings[i].Unreal)]
                        (libretro_api_t libretro_api)
                        {
                            if (libretro_api.keyboard_event)
                            {
                                libretro_api.keyboard_event(down, key_bindings[i].libretro, 0, RETROKMOD_NONE);
                            }
                        });
                    }
                }
            }
        }
    }
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
            [SRAMPath = FUnrealLibretroModule::ResolveSRAMPath(RomPath, SRAMPath)](auto libretro_api)
            {
                auto SRAMBuffer = TArrayView<const uint8>((uint8*)libretro_api.get_memory_data(RETRO_MEMORY_SAVE_RAM),
                                                                  libretro_api.get_memory_size(RETRO_MEMORY_SAVE_RAM));
                FFileHelper::SaveArrayToFile(SRAMBuffer, *SRAMPath);
            });

        Shutdown();
    }

    Super::BeginDestroy();
}
