
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

#define NOT_LAUNCHED_GUARD if (!CoreInstance.IsSet()) return;

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
    NOT_LAUNCHED_GUARD

    if (PeerId <= SAM2_PORT_SENTINELS_MAX || PeerId > 65535)
    {
        SAM2_LOG_ERROR("Invalid peer id %05d. It should be between %05d and 65535 inclusive", PeerId, SAM2_PORT_SENTINELS_MAX + 1);
        return;
    }

    CoreInstance.GetValue()->NetplayTasks.Enqueue([CoreInstance = this->CoreInstance.GetValue(), PeerId](libretro_api_t& libretro_api)
        {
            if (CoreInstance->netplay_session->room_we_are_in.flags & SAM2_FLAG_ROOM_IS_NETWORK_HOSTED)
            {
                SAM2_LOG_INFO("Disconnecting from peer %05d room and connecting to peer %05d room", PeerId);
                ulnet_session_tear_down(CoreInstance->netplay_session);
            }

            // Directly signaling the authority just means spectate
            ulnet_session_init_defaulted(CoreInstance->netplay_session);
            CoreInstance->netplay_session->room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX] = PeerId;
            CoreInstance->netplay_session->frame_counter = ULNET_WAITING_FOR_SAVE_STATE_SENTINEL;
            ulnet_startup_ice_for_peer(
                CoreInstance->netplay_session,
                PeerId,
                SAM2_AUTHORITY_INDEX,
                NULL
            );
        });
}

void ULibretroCoreInstance::NetplayHost(int PeerId)
{
    NOT_LAUNCHED_GUARD

    if (PeerId) {
        // We want to change our peer id
        if (PeerId <= SAM2_PORT_SENTINELS_MAX || PeerId > 65535) {
            SAM2_LOG_ERROR("Invalid peer id %05d. It should be between %05d and 65535 inclusive", PeerId, SAM2_PORT_SENTINELS_MAX + 1);
            return;
        }
    }

    sam2_room_make_message_t HostRoomRequest = { SAM2_MAKE_HEADER };

    FString RoomName = GetOwner() ? GetOwner()->GetName() : GetName();
    FTCHARToUTF8 RoomNameUTF8{ *RoomName };

    int EndOfRoomNameIndex = SAM2_MIN(RoomNameUTF8.Length(), SAM2_ARRAY_LENGTH(HostRoomRequest.room.name)-1);
    FMemory::Memcpy(HostRoomRequest.room.name, RoomNameUTF8.Get(), EndOfRoomNameIndex);
    HostRoomRequest.room.name[EndOfRoomNameIndex] = '\0';
    HostRoomRequest.room.flags |= SAM2_FLAG_ROOM_IS_NETWORK_HOSTED;
    CoreInstance.GetValue()->NetplayTasks.Enqueue([CoreInstance = this->CoreInstance.GetValue(), HostRoomRequest, PeerId](libretro_api_t& libretro_api)
        mutable {
            HostRoomRequest.room.rom_hash_xxh64 = CoreInstance->rom_hash_xxh64;

            sam2_format_core_version(
                &HostRoomRequest.room,
                CoreInstance->system.library_name,
                CoreInstance->system.library_version
            );

            if (CoreInstance->netplay_session->room_we_are_in.flags & SAM2_FLAG_ROOM_IS_NETWORK_HOSTED)
            {
                if (ulnet_is_spectator(CoreInstance->netplay_session, CoreInstance->netplay_session->our_peer_id))
                {
                    ulnet_session_tear_down(CoreInstance->netplay_session);
                }
                else if (ulnet_is_authority(CoreInstance->netplay_session))
                {
                    SAM2_LOG_WARN("We're already hosting a room, we can't host a new room");
                    return;
                }
                else
                {
                    SAM2_LOG_ERROR("We're connected peer to peer we have to exit gracefully before we can host a new room");
                    // @todo Disconnect gracefully
                    return;
                }
            }

            if (PeerId)
            {
                sam2_connect_message_t ChangePeerIdRequest = { SAM2_CONN_HEADER };
                ChangePeerIdRequest.peer_id = PeerId;

                if (int ErrorCode = sam2_client_send(CoreInstance->sam_socket, (char*)&ChangePeerIdRequest))
                {
                    SAM2_LOG_ERROR("Failed to send message with header '%.8s' (error=%d)", (char*)&ChangePeerIdRequest, ErrorCode);
                }
            }

            if (int ErrorCode = sam2_client_send(CoreInstance->sam_socket, (char*)&HostRoomRequest))
            {
                SAM2_LOG_ERROR("Failed to send message with header '%.8s' (error=%d)", (char*)&HostRoomRequest, ErrorCode);
            }
        });
}

void ULibretroCoreInstance::SetController(int Port, int64 ID)
{
    NOT_LAUNCHED_GUARD

    // This if statement guards against a datarace on FLibretroContext::DeviceIDs
    if (CoreInstance.GetValue()->CoreState.load(std::memory_order_acquire) != FLibretroContext::ECoreState::Starting)
    {
        CoreInstance.GetValue()->DeviceIDs[Port] = ID;
        CoreInstance.GetValue()->EnqueueTask([Port, ID](libretro_api_t &libretro_api)
            {
                libretro_api.set_controller_port_device(Port, ID);
            }); 
    }
}

void ULibretroCoreInstance::GetController(int Port, int64& ID, FString& Description)
{
    NOT_LAUNCHED_GUARD

    // This if statement guards against a datarace on FLibretroContext::DeviceIDs
    if (CoreInstance.GetValue()->CoreState.load(std::memory_order_acquire) != FLibretroContext::ECoreState::Starting)
    {
        ID = CoreInstance.GetValue()->DeviceIDs[Port];
        for (FLibretroControllerDescription& ControllerDescription : CoreInstance.GetValue()->ControllerDescriptions[Port])
        {
            if (ControllerDescription.ID == ID)
            {
                Description = ControllerDescription.Description;
                break;
            }
        }
    }
}

TArray<FLibretroOptionDescription> ULibretroCoreInstance::GetOptionDescriptions()
{
    return CoreInstance.IsSet() ? CoreInstance.GetValue()->OptionDescriptions           : TArray<FLibretroOptionDescription>{};
}

TArray<FLibretroControllerDescription> ULibretroCoreInstance::GetControllerDescriptions(int Port)
{
    return CoreInstance.IsSet() ? CoreInstance.GetValue()->ControllerDescriptions[Port] : TArray<FLibretroControllerDescription>{};
}

void ULibretroCoreInstance::GetOption(const FString& Key, FString& Value, int& Index)
{
    NOT_LAUNCHED_GUARD

    for (int i = 0; i < CoreInstance.GetValue()->OptionDescriptions.Num(); i++)
    {
        if (CoreInstance.GetValue()->OptionDescriptions[i].Key == Key)
        {
            Index = CoreInstance.GetValue()->OptionSelectedIndex[i].load(std::memory_order_relaxed);
            Value = CoreInstance.GetValue()->OptionDescriptions[i].Values[Index];
        }
    }
}

void ULibretroCoreInstance::SetOption(const FString& Key, const FString& Value)
{
    NOT_LAUNCHED_GUARD

    for (int i = 0; i < CoreInstance.GetValue()->OptionDescriptions.Num(); i++)
    {
        if (CoreInstance.GetValue()->OptionDescriptions[i].Key == Key)
        {
            int32 Index = CoreInstance.GetValue()->OptionDescriptions[i].Values.IndexOfByKey(Value);
            CoreInstance.GetValue()->OptionSelectedIndex[i].store(Index, std::memory_order_relaxed);
            CoreInstance.GetValue()->OptionsHaveBeenModified.store(true, std::memory_order_release);
        }
    }
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
    this->CoreInstance = FLibretroContext::Launch(this, _CorePath, _RomPath, RenderTarget, static_cast<URawAudioSoundWave*>(AudioBuffer),
        [weakThis = MakeWeakObjectPtr(this), SRAMPath = FUnrealLibretroModule::ResolveSRAMPath(_RomPath, SRAMPath)]
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
                    }

                    weakThis->OnLaunchComplete.Broadcast(weakThis->RenderTarget,
                        weakThis->AudioBuffer, bCoreLaunchSucceeded);
                }
            }, TStatId(), nullptr, ENamedThreads::GameThread);

            if (bCoreLaunchSucceeded)
            {
                // Core has loaded
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
                            weakThis->bFrameBottomLeftOrigin = bottom_left_origin;
                            weakThis->FrameWidth  = geometry.base_width;
                            weakThis->FrameHeight = geometry.base_height;
                        
                            weakThis->OnCoreFrameBufferResize.Broadcast();
                        
                            weakThis->AudioComponent->SetSound(weakThis->AudioBuffer);
                            weakThis->AudioComponent->Play();
                        }
                    }, TStatId(), nullptr, ENamedThreads::GameThread);
            }
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
    NOT_LAUNCHED_GUARD

    CoreInstance.GetValue()->Pause(ShouldPause);
    Paused = ShouldPause;
}

void ULibretroCoreInstance::Shutdown() 
{
    NOT_LAUNCHED_GUARD

    FLibretroContext::Shutdown(CoreInstance.GetValue());
    CoreInstance.Reset();
}

// @todo Reimplement these to load and save from buffers since right now there is a race condition
//       Where multiple cores access data from the file system at the same time
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
            FFileHelper::SaveArrayToFile(SaveStateBuffer, *SaveStatePath);
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

void ULibretroCoreInstance::SetInputDigital(int Port, bool Pressed, ERetroDeviceID Input)
{
    NOT_LAUNCHED_GUARD

    CoreInstance.GetValue()->EnqueueTask([=, CoreInstance = CoreInstance.GetValue()](auto)
    {
        CoreInstance->NextInputState[Port][Input] = Pressed;
    });
}

void ULibretroCoreInstance::SetInputAnalog(int Port, int _16BitSignedInteger, ERetroDeviceID Input)
{
    NOT_LAUNCHED_GUARD

    CoreInstance.GetValue()->EnqueueTask([=, CoreInstance = CoreInstance.GetValue()](auto)
    {
        CoreInstance->NextInputState[Port][Input] = _16BitSignedInteger;
    });
}

void ULibretroCoreInstance::ReadMemory(ERetroMemoryType MemoryType, int64 Address, int64 Size, const FOnReadMemoryComplete& OnReadMemoryComplete)
{
    NOT_LAUNCHED_GUARD

    if (Size <= 0)
    {
        UE_LOG(Libretro, Warning, TEXT("ReadMemory: Invalid Size (%lld)"), Size);
        return;
    }

    CoreInstance.GetValue()->EnqueueTask([=, weakThis = MakeWeakObjectPtr(this)](libretro_api_t libretro_api)
    {
        void*  memory_data = libretro_api.get_memory_data(MemoryType);
        size_t memory_size = libretro_api.get_memory_size(MemoryType);

        if (memory_data == nullptr)
        {
            UE_LOG(Libretro, Warning, TEXT("ReadMemory: Memory data is null for MemoryType %d"), MemoryType);
            return;
        }

        if (static_cast<uint64>(Address) + static_cast<uint64>(Size) > memory_size)
        {
            UE_LOG(Libretro, Warning, TEXT("ReadMemory: Address + Size (%lld + %lld) exceeds memory size (%zu) for MemoryType %d"), Address, Size, memory_size, MemoryType);
            return;
        }

        TArray<uint8> Data;
        Data.SetNumUninitialized(Size);
        FMemory::Memcpy(Data.GetData(), (uint8*)memory_data + static_cast<uint64>(Address), Size);

        FFunctionGraphTask::CreateAndDispatchWhenReady(
            [=]()
            {
                OnReadMemoryComplete.ExecuteIfBound(0, Address, Data);
            }, TStatId(), nullptr, ENamedThreads::GameThread);
    });
}

void ULibretroCoreInstance::WriteMemory(ERetroMemoryType MemoryType, int64 Address, const TArray<uint8>& Data)
{
    NOT_LAUNCHED_GUARD

    if (Data.Num() <= 0)
    {
        UE_LOG(Libretro, Warning, TEXT("WriteMemory: Data array is empty"));
        return;
    }

    CoreInstance.GetValue()->EnqueueTask([=, weakThis = MakeWeakObjectPtr(this)](libretro_api_t libretro_api)
    {
        void* memory_data = libretro_api.get_memory_data(MemoryType);
        size_t memory_size = libretro_api.get_memory_size(MemoryType);

        if (memory_data == nullptr)
        {
            UE_LOG(Libretro, Warning, TEXT("WriteMemory: Memory data is null for MemoryType %d"), MemoryType);
            return;
        }

        if (static_cast<uint64>(Address) + static_cast<uint64>(Data.Num()) > memory_size)
        {
            UE_LOG(Libretro, Warning, TEXT("WriteMemory: Address + Data.Num() (%lld + %d) exceeds memory size (%zu) for MemoryType %d"), Address, Data.Num(), memory_size, MemoryType);
            return;
        }

        FMemory::Memcpy(static_cast<uint8*>(memory_data) + static_cast<uint64>(Address), Data.GetData(), Data.Num());
    });
}


void ULibretroCoreInstance::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
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
