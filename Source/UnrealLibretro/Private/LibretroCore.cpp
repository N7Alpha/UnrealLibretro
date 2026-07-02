#include "LibretroCore.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

THIRD_PARTY_INCLUDES_START
#include "ulnet.h"
#include "libretro/libretro.h"
THIRD_PARTY_INCLUDES_END

#include "Misc/FileHelper.h"
#include "UnrealLibretro.h"
#include "LibretroCoreInstance.h"
#include "LibretroContext.h"
#include "Async/TaskGraphInterfaces.h"

#define CORE_GONE_GUARD if (!Context) { UE_LOG(Libretro, Warning, TEXT("%hs called on a ULibretroCore whose core has shut down"), __FUNCTION__); return; }

void ULibretroCore::LoadState(const FString& FilePath)
{
    CORE_GONE_GUARD

    Context->EnqueueTask(
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

void ULibretroCore::SaveState(const FString& FilePath)
{
    CORE_GONE_GUARD

    Context->EnqueueTask
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

void ULibretroCore::Pause(bool ShouldPause)
{
    CORE_GONE_GUARD

    Context->Pause(ShouldPause);
}

TArray<FLibretroControllerDescription> ULibretroCore::GetControllerDescriptions(int Port)
{
    return Context ? Context->ControllerDescriptions[Port] : TArray<FLibretroControllerDescription>{};
}

void ULibretroCore::GetController(int Port, int64& ID, FString& Description)
{
    CORE_GONE_GUARD

    // This if statement guards against a datarace on FLibretroContext::DeviceIDs
    if (Context->CoreState.load(std::memory_order_acquire) != FLibretroContext::ECoreState::Starting)
    {
        ID = Context->DeviceIDs[Port];
        for (FLibretroControllerDescription& ControllerDescription : Context->ControllerDescriptions[Port])
        {
            if (ControllerDescription.ID == ID)
            {
                Description = ControllerDescription.Description;
                break;
            }
        }
    }
}

void ULibretroCore::SetController(int Port, int64 ID)
{
    CORE_GONE_GUARD

    // This if statement guards against a datarace on FLibretroContext::DeviceIDs
    if (Context->CoreState.load(std::memory_order_acquire) != FLibretroContext::ECoreState::Starting)
    {
        Context->DeviceIDs[Port] = ID;
        Context->EnqueueTask([Port, ID](libretro_api_t &libretro_api)
            {
                libretro_api.set_controller_port_device(Port, ID);
            });
    }
}

TArray<FLibretroOptionDescription> ULibretroCore::GetOptionDescriptions()
{
    return Context ? Context->OptionDescriptions : TArray<FLibretroOptionDescription>{};
}

void ULibretroCore::GetOption(const FString& Key, FString& Value, int& Index)
{
    CORE_GONE_GUARD

    for (int i = 0; i < Context->OptionDescriptions.Num(); i++)
    {
        if (Context->OptionDescriptions[i].Key == Key)
        {
            Index = Context->OptionSelectedIndex[i].load(std::memory_order_relaxed);
            Value = Context->OptionDescriptions[i].Values[Index];
        }
    }
}

void ULibretroCore::SetOption(const FString& Key, const FString& Value)
{
    CORE_GONE_GUARD

    for (int i = 0; i < Context->OptionDescriptions.Num(); i++)
    {
        if (Context->OptionDescriptions[i].Key == Key)
        {
            int32 Index = Context->OptionDescriptions[i].Values.IndexOfByKey(Value);
            Context->OptionSelectedIndex[i].store(Index, std::memory_order_relaxed);
            Context->OptionsHaveBeenModified.store(true, std::memory_order_release);
        }
    }
}

void ULibretroCore::SetInputDigital(int Port, bool Activated, ERetroDeviceID Input)
{
    CORE_GONE_GUARD

    Context->EnqueueTask([=, Context = this->Context](auto)
    {
        Context->NextInputState[Port][Input] = Activated;
    });
}

void ULibretroCore::SetInputAnalog(int Port, int _16BitSignedInteger, ERetroDeviceID Input)
{
    CORE_GONE_GUARD

    Context->EnqueueTask([=, Context = this->Context](auto)
    {
        Context->NextInputState[Port][Input] = _16BitSignedInteger;
    });
}

void ULibretroCore::ReadMemory(ERetroMemoryType MemoryType, int64 Address, int64 Size, const FOnReadMemoryComplete& OnReadMemoryComplete)
{
    CORE_GONE_GUARD

    if (Size <= 0)
    {
        UE_LOG(Libretro, Warning, TEXT("ReadMemory: Invalid Size (%lld)"), Size);
        return;
    }

    Context->EnqueueTask([=](libretro_api_t libretro_api)
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

void ULibretroCore::WriteMemory(ERetroMemoryType MemoryType, int64 Address, const TArray<uint8>& Data)
{
    CORE_GONE_GUARD

    if (Data.Num() <= 0)
    {
        UE_LOG(Libretro, Warning, TEXT("WriteMemory: Data array is empty"));
        return;
    }

    Context->EnqueueTask([=](libretro_api_t libretro_api)
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

void ULibretroCore::NetplayHost(int PeerId)
{
    CORE_GONE_GUARD

    if (PeerId <= SAM2_PORT_SENTINELS_MAX || PeerId > 65535)
    {
        SAM2_LOG_ERROR("Invalid peer id %05d. It should be between %05d and 65535 inclusive", PeerId, SAM2_PORT_SENTINELS_MAX + 1);
        return;
    }

    Context->NetplayTasks.Enqueue([Context = this->Context, PeerId, WeakOwner = OwningComponent](libretro_api_t& libretro_api)
        {
            if (!Context->connected_to_sam2)
            {
                SAM2_LOG_WARN("Ignoring NetplayHost: not connected to the sam2 signaling server");
                ULibretroCoreInstance::BroadcastNetplayErrorOnGameThread(WeakOwner, TEXT("Not connected to the netplay signaling server"), SAM2_RESPONSE_SERVER_ERROR);
                return;
            }

            Context->NetplayHost_CoreThread((uint16)PeerId);
        });
}

void ULibretroCore::NetplaySync(int PeerId)
{
    CORE_GONE_GUARD

    if (PeerId <= SAM2_PORT_SENTINELS_MAX || PeerId > 65535)
    {
        SAM2_LOG_ERROR("Invalid peer id %05d. It should be between %05d and 65535 inclusive", PeerId, SAM2_PORT_SENTINELS_MAX + 1);
        return;
    }

    Context->NetplayTasks.Enqueue([Context = this->Context, PeerId, WeakOwner = OwningComponent](libretro_api_t& libretro_api)
        {
            // Signaling through sam2 is required for the ICE handshake to ever complete
            if (!Context->connected_to_sam2)
            {
                SAM2_LOG_WARN("Ignoring NetplaySync: not connected to the sam2 signaling server");
                ULibretroCoreInstance::BroadcastNetplayErrorOnGameThread(WeakOwner, TEXT("Not connected to the netplay signaling server"), SAM2_RESPONSE_SERVER_ERROR);
                return;
            }

            Context->NetplaySync_CoreThread((uint16)PeerId);
        });
}

void ULibretroCore::NetplayLeave()
{
    CORE_GONE_GUARD

    Context->NetplayTasks.Enqueue([Context = this->Context](libretro_api_t& libretro_api)
        {
            // The sentinel check covers a join that is still waiting for the host's savestate --
            // the room flags only arrive with the savestate, so it isn't NETWORK_HOSTED yet
            if (   Context->netplay_session->room_we_are_in.flags & SAM2_FLAG_ROOM_IS_NETWORK_HOSTED
                || Context->netplay_session->frame_counter == ULNET_WAITING_FOR_SAVE_STATE_SENTINEL)
            {
                SAM2_LOG_INFO("Leaving the current netplay room");
                ulnet_session_tear_down(Context->netplay_session); // Best-effort EXIT to the host
                ulnet_session_init_defaulted(Context->netplay_session);
            }
        });
}
