
#include "LibretroCoreInstance.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

THIRD_PARTY_INCLUDES_START
#include "ulnet.h"
#include "libretro/libretro.h"
THIRD_PARTY_INCLUDES_END

#include "Misc/FileHelper.h"
#include "Components/AudioComponent.h"
#include "GameFramework/PlayerInput.h"
#include "SocketSubsystem.h"
#include "Engine/NetDriver.h"
#include "Engine/NetConnection.h"
#include "Engine/World.h"

#include "UnrealLibretro.h"
#include "LibretroInputDefinitions.h"
#include "RawAudioSoundWave.h"
#include "LibretroContext.h"
#include "Async/TaskGraphInterfaces.h"

#define NOT_LAUNCHED_GUARD if (!CoreInstance.IsSet()) return;

void ULibretroCoreInstance::BroadcastNetplayErrorOnGameThread(TWeakObjectPtr<ULibretroCoreInstance> WeakThis, FString Message, int64 Code)
{
    FFunctionGraphTask::CreateAndDispatchWhenReady([WeakThis, Message = MoveTemp(Message), Code]
        {
            if (WeakThis.IsValid())
            {
                WeakThis->OnNetplayError.Broadcast(Message, Code);
            }
        }, TStatId(), nullptr, ENamedThreads::GameThread);
}

ULibretroCoreInstance::ULibretroCoreInstance()
{
    PrimaryComponentTick.bCanEverTick = true;
}

//bool ULibretroCoreInstance::IsReadyForFinishDestroy() { return true; };
FString ULibretroCoreInstance::GetAuthorityIP()
{
    check(IsInGameThread());

    if (UWorld* World = GetWorld())
    {
        // If we're a client
        if (World->GetNetMode() == NM_Client)
        {
            if (UNetDriver* NetDriver = World->GetNetDriver())
            {
                if (NetDriver->ServerConnection)
                {
                    auto RemoteAddr = NetDriver->ServerConnection->GetRemoteAddr();
                    return RemoteAddr->ToString(/* bAppendPort = */ false);
                }
            }
        }
        // If we're the defacto server/host
        else
        {
            return TEXT("127.0.0.1");
        }
    }

    return TEXT("0.0.0.0"); // Return invalid IP if something went wrong
}

void ULibretroCoreInstance::NetplaySync(int PeerId)
{
    if (Core) Core->NetplaySync(PeerId);
}

void ULibretroCoreInstance::NetplayLeave()
{
    if (Core) Core->NetplayLeave();
}

void ULibretroCoreInstance::NetplayHost(int PeerId)
{
    if (Core) Core->NetplayHost(PeerId);
}

void ULibretroCoreInstance::SetController(int Port, int64 ID)
{
    if (Core) Core->SetController(Port, ID);
}

void ULibretroCoreInstance::GetController(int Port, int64& ID, FString& Description)
{
    if (Core) Core->GetController(Port, ID, Description);
}

TArray<FLibretroOptionDescription> ULibretroCoreInstance::GetOptionDescriptions()
{
    return Core ? Core->GetOptionDescriptions() : TArray<FLibretroOptionDescription>{};
}

TArray<FLibretroControllerDescription> ULibretroCoreInstance::GetControllerDescriptions(int Port)
{
    return Core ? Core->GetControllerDescriptions(Port) : TArray<FLibretroControllerDescription>{};
}

void ULibretroCoreInstance::GetOption(const FString& Key, FString& Value, int& Index)
{
    if (Core) Core->GetOption(Key, Value, Index);
}

void ULibretroCoreInstance::SetOption(const FString& Key, const FString& Value)
{
    if (Core) Core->SetOption(Key, Value);
}

void ULibretroCoreInstance::Launch() 
{
    Shutdown();
    
    // Clear any previous error message
    LastErrorMessage.Empty();
    
    FString _CorePath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*FUnrealLibretroModule::ResolveCorePath(this->CorePath));
    FString _RomPath = "";

    // White-space only or empty strings are interpreted as not providing a ROM to the core
    if (!RomPath.TrimStart().IsEmpty())
    {
        _RomPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*FUnrealLibretroModule::ResolveROMPath(this->RomPath));
    }
    
#if PLATFORM_WINDOWS
    _RomPath.ReplaceCharInline('/', '\\');
#endif

    if (!IPlatformFile::GetPlatformPhysical().FileExists(*_CorePath))
    {
        LastErrorMessage = FString::Printf(TEXT("Failed to launch Libretro core '%s'. Couldn't find core at path '%s'"), *_CorePath, *_CorePath);
        UE_LOG(Libretro, Warning, TEXT("%s"), *LastErrorMessage);
        
        // Notify of failure
        FFunctionGraphTask::CreateAndDispatchWhenReady(
            [this]()
            {
                OnLaunchComplete.Broadcast(nullptr, nullptr, false);
            }, TStatId(), nullptr, ENamedThreads::GameThread);
        return;
    }
    else if (!_RomPath.IsEmpty() && !IPlatformFile::GetPlatformPhysical().FileExists(*_RomPath) && !IPlatformFile::GetPlatformPhysical().DirectoryExists(*_RomPath))
    {
        LastErrorMessage = FString::Printf(TEXT("Failed to launch Libretro core '%s'. Couldn't find ROM at path '%s'"), *_CorePath, *_RomPath);
        UE_LOG(Libretro, Warning, TEXT("%s"), *LastErrorMessage);
        
        // Notify of failure
        FFunctionGraphTask::CreateAndDispatchWhenReady(
            [this]()
            {
                OnLaunchComplete.Broadcast(nullptr, nullptr, false);
            }, TStatId(), nullptr, ENamedThreads::GameThread);
        return;
    }

    if (!RenderTarget)
    {
        RenderTarget = NewObject<UTextureRenderTarget2D>();
    }

    AudioBuffer = NewObject<URawAudioSoundWave>();

    RenderTarget->Filter = TF_Nearest; // @todo remove this

    // @todo Figure out if this is actually a problem then fix it maybe
    // Sometimes it can be practical to make the UV's oversized so we don't want it to wrap
    // however this might make debugging a little more confusing if you have a UV transformation issue because the texture might be rendered
    // as completely black and not reflected or tiled or something <-- I actually immediately ran into this issue because the logic here was
    // broken because I think all my UV's are negative
    //RenderTarget->AddressX = TA_Clamp;
    //RenderTarget->AddressY = TA_Clamp;

    Sam2ServerAddress = ULibretroCoreInstance::GetAuthorityIP();
    ResolvedSRAMPath = FUnrealLibretroModule::ResolveSRAMPath(_RomPath, SRAMPath);
    this->CoreInstance = FLibretroContext::Launch(this, _CorePath, _RomPath, RenderTarget, static_cast<URawAudioSoundWave*>(AudioBuffer),
        [weakThis = MakeWeakObjectPtr(this), SRAMPath = ResolvedSRAMPath]
        (FLibretroContext *_CoreInstance, libretro_api_t &libretro_api, const FString& ErrorMessage)
        {
            bool bCoreLaunchSucceeded = _CoreInstance->CoreState.load(std::memory_order_relaxed) != FLibretroContext::ECoreState::StartFailed;

            FFunctionGraphTask::CreateAndDispatchWhenReady(
                [weakThis, bCoreLaunchSucceeded, ErrorMessage]()
            {
                if (weakThis.IsValid())
                {
                    if (!bCoreLaunchSucceeded)
                    {
                        weakThis->LastErrorMessage = ErrorMessage;
                        weakThis->CoreInstance.Reset();
                        if (weakThis->Core)
                        {
                            weakThis->Core->Context = nullptr; // Invalidate any handle Blueprint already grabbed
                            weakThis->Core = nullptr;
                        }
                    }

                    weakThis->OnLaunchComplete.Broadcast(weakThis->RenderTarget,
                        weakThis->AudioBuffer, bCoreLaunchSucceeded);

                    if (bCoreLaunchSucceeded && weakThis->Core)
                    {
                        weakThis->OnCoreLaunched.Broadcast(weakThis->Core);
                    }
                }
            }, TStatId(), nullptr, ENamedThreads::GameThread);

            if (bCoreLaunchSucceeded)
            {
                // Core has loaded
                // Load save data into core @todo this is just a weird place to hook this in
                auto File = IPlatformFile::GetPlatformPhysical().OpenRead(*SRAMPath);
                if (File)
                {
                    void*  SRAMData = libretro_api.get_memory_data(RETRO_MEMORY_SAVE_RAM);
                    size_t SRAMSize = libretro_api.get_memory_size(RETRO_MEMORY_SAVE_RAM);
                    if (SRAMData && SRAMSize > 0)
                    {
                        // The core and the file on disk can disagree about the save size (different core
                        // version, or a stale zero-length file) so only read as many bytes as both have
                        File->Read((uint8*)SRAMData, FMath::Min((int64)SRAMSize, File->Size()));
                    }
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
                            weakThis->bFrameBottomLeftOrigin = bottom_left_origin;
                            weakThis->FrameWidth  = geometry.base_width;
                            weakThis->FrameHeight = geometry.base_height;
                        
                            weakThis->OnCoreFrameBufferResize.Broadcast();

                            if (weakThis->AudioComponent) // Not assigned in Blueprint, or running headless
                            {
                                weakThis->AudioComponent->SetSound(weakThis->AudioBuffer);
                                weakThis->AudioComponent->Play();
                            }
                        }
                    }, TStatId(), nullptr, ENamedThreads::GameThread);
            }
        });
    
    // The handle mirrors the legacy semantics: it exists as soon as the launch is kicked off (like
    // CoreInstance being Set) and is invalidated on failure/shutdown. New API surface lives on it.
    Core = NewObject<ULibretroCore>(this);
    Core->Context = CoreInstance.GetValue();
    Core->CorePath = this->CorePath;
    Core->RomPath = this->RomPath;
    Core->OwningComponent = this;

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
                                if (weakThis->AudioComponent) // Not assigned in Blueprint, or running headless
                                {
                                    weakThis->AudioComponent->SetSound(weakThis->AudioBuffer);
                                }
                            }

                            weakThis->FrameWidth  = system_av_info.geometry.base_width;
                            weakThis->FrameHeight = system_av_info.geometry.base_height;
                            weakThis->OnCoreFrameBufferResize.Broadcast();
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
                        weakThis->FrameRotation = rotation / 4.f;
                        weakThis->OnCoreFrameBufferResize.Broadcast();
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
                        weakThis->FrameWidth  = geometry.base_width;
                        weakThis->FrameHeight = geometry.base_height;
                        weakThis->OnCoreFrameBufferResize.Broadcast();
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
    if (Core)
    {
        Core->Pause(ShouldPause);
        Paused = ShouldPause;
    }
}

void ULibretroCoreInstance::Shutdown()
{
    NOT_LAUNCHED_GUARD

    // Persist SRAM before the core shuts down. Every teardown funnels through here (PIE end,
    // BeginDestroy, and relaunching with a different ROM) so saves aren't lost when the
    // component is torn down without being garbage collected first (issue #7)
    if (!ResolvedSRAMPath.IsEmpty())
    {
        this->CoreInstance.GetValue()->EnqueueTask(
            [SRAMPath = ResolvedSRAMPath](libretro_api_t& libretro_api)
            {
                void*  SRAMData = libretro_api.get_memory_data(RETRO_MEMORY_SAVE_RAM);
                size_t SRAMSize = libretro_api.get_memory_size(RETRO_MEMORY_SAVE_RAM);

                // Cores that report no save RAM must not truncate an existing save file to zero bytes
                if (SRAMData && SRAMSize > 0)
                {
                    FFileHelper::SaveArrayToFile(TArrayView<const uint8>((const uint8*)SRAMData, SRAMSize), *SRAMPath);
                }
            });
    }

    if (Core)
    {
        Core->Context = nullptr; // Handles Blueprint stashed elsewhere go inert rather than dangling
        Core = nullptr;
    }

    FLibretroContext::Shutdown(CoreInstance.GetValue());
    CoreInstance.Reset();
}

// @todo Reimplement these to load and save from buffers since right now there is a race condition
//       Where multiple cores access data from the file system at the same time
void ULibretroCoreInstance::LoadState(const FString& FilePath)
{
    if (Core) Core->LoadState(FilePath);
}

void ULibretroCoreInstance::SaveState(const FString& FilePath)
{
    if (Core) Core->SaveState(FilePath);
}

#include "Scalability.h"

void ULibretroCoreInstance::BeginPlay()
{
    Super::BeginPlay();

    /*if (Scalability::GetQualityLevels().AntiAliasingQuality) {
        FMessageDialog::Open(EAppMsgType::Ok, FText::AsCultureInvariant("You have temporal anti-aliasing enabled. The emulated games will look will look blurry and laggy if you leave this enabled. If you happen to know how to fix this let me know. I tried enabling responsive AA on the material to prevent this, but that didn't work."));
    }*/
}

void ULibretroCoreInstance::SetInputDigital(int Port, bool Pressed, ERetroDeviceID Input)
{
    if (Core) Core->SetInputDigital(Port, Pressed, Input);
}

void ULibretroCoreInstance::SetInputAnalog(int Port, int _16BitSignedInteger, ERetroDeviceID Input)
{
    if (Core) Core->SetInputAnalog(Port, _16BitSignedInteger, Input);
}

void ULibretroCoreInstance::ReadMemory(ERetroMemoryType MemoryType, int64 Address, int64 Size, const FOnReadMemoryComplete& OnReadMemoryComplete)
{
    if (Core) Core->ReadMemory(MemoryType, Address, Size, OnReadMemoryComplete);
}

void ULibretroCoreInstance::WriteMemory(ERetroMemoryType MemoryType, int64 Address, const TArray<uint8>& Data)
{
    if (Core) Core->WriteMemory(MemoryType, Address, Data);
}


void ULibretroCoreInstance::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    if (CoreInstance.IsSet())
    {
        CoreInstance.GetValue()->SetSynchronousTickMode(bSynchronousTickMode);
        if (bSynchronousTickMode)
        {
            // Blocks until the emulator has advanced (at most one frame, gated by its native pacing)
            // and its framebuffer upload is dispatched, so the emulated frame lands in the Unreal
            // frame we're currently building
            CoreInstance.GetValue()->RunFrameSynchronously();
        }
    }

    if (   CoreInstance.IsSet()
        && KeyboardInputSourcePlayerController)
    {
        for (int i = 0; i < count_key_bindings; i++)
        {
            if (   KeyboardInputSourcePlayerController->PlayerInput->WasJustPressed(key_bindings[i].Unreal)
                || KeyboardInputSourcePlayerController->PlayerInput->WasJustReleased(key_bindings[i].Unreal))
            {
                this->CoreInstance.GetValue()->EnqueueTask(
                    [=, down = KeyboardInputSourcePlayerController->PlayerInput->WasJustPressed(key_bindings[i].Unreal)]
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

void ULibretroCoreInstance::BeginDestroy()
{
    Shutdown(); // Also persists SRAM

    Super::BeginDestroy();
}
