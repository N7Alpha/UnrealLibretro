#pragma once

#include "CoreMinimal.h"
#include "LibretroInputDefinitions.h"
#include "LibretroCore.generated.h"

DECLARE_DYNAMIC_DELEGATE_ThreeParams(FOnReadMemoryComplete, int64, TheFrameMemoryWasRead, int64, Address, const TArray<uint8>&, Memory);

/**
 * A handle to a launched, running Libretro core. Obtained from ULibretroCoreInstance::GetCore()
 * (null before launch) or the OnCoreLaunched event (fired with a valid handle).
 *
 * Unlike the equivalent functions on ULibretroCoreInstance -- which silently do nothing before
 * Launch() -- a ULibretroCore only exists while a core is running, so validity is an explicit,
 * branchable null check instead of an invisible no-op. New code should prefer this API; the
 * component functions remain for backwards compatibility and just forward here.
 */
UCLASS(BlueprintType)
class UNREALLIBRETRO_API ULibretroCore : public UObject
{
    GENERATED_BODY()

public:
    /** False once the core has been shut down (the owning component keeps no stale handles, but ones stored elsewhere can outlive it) */
    UFUNCTION(BlueprintPure, Category = "Libretro")
    bool IsRunning() const { return Context != nullptr; }

    /** @see ULibretroCoreInstance::LoadState */
    UFUNCTION(BlueprintCallable, Category = "Libretro")
    void LoadState(const FString& FilePath = "Default.sav");

    /** @see ULibretroCoreInstance::SaveState */
    UFUNCTION(BlueprintCallable, Category = "Libretro")
    void SaveState(const FString& FilePath = "Default.sav");

    /** @see ULibretroCoreInstance::Pause */
    UFUNCTION(BlueprintCallable, Category = "Libretro")
    void Pause(bool ShouldPause = true);

    UFUNCTION(BlueprintPure, Category = "Libretro")
    TArray<FLibretroControllerDescription> GetControllerDescriptions(int Port);

    UFUNCTION(BlueprintCallable, Category = "Libretro")
    void GetController(int Port, int64& ID, FString& Description);

    UFUNCTION(BlueprintCallable, Category = "Libretro")
    void SetController(int Port, int64 ID);

    UFUNCTION(BlueprintPure, Category = "Libretro")
    TArray<FLibretroOptionDescription> GetOptionDescriptions();

    UFUNCTION(BlueprintCallable, Category = "Libretro")
    void GetOption(const FString& Key, FString& Value, int& Index);

    UFUNCTION(BlueprintCallable, Category = "Libretro")
    void SetOption(const FString& Key, const FString& Value);

    /** @see ULibretroCoreInstance::SetInputDigital */
    UFUNCTION(BlueprintCallable, Category = "Libretro")
    void SetInputDigital(int Port, bool Activated, ERetroDeviceID Input);

    /** @see ULibretroCoreInstance::SetInputAnalog */
    UFUNCTION(BlueprintCallable, Category = "Libretro")
    void SetInputAnalog(int Port, int _16BitSignedInteger, ERetroDeviceID Input);

    UFUNCTION(BlueprintCallable, Category = "Libretro")
    void ReadMemory(ERetroMemoryType MemoryType, int64 Address, int64 Size, const FOnReadMemoryComplete& OnReadMemoryComplete);

    UFUNCTION(BlueprintCallable, Category = "Libretro")
    void WriteMemory(ERetroMemoryType MemoryType, int64 Address, const TArray<uint8>& Data);

    /** Host a netplay room with the given peer id (2-65535). @see ULibretroCoreInstance::NetplayHost */
    UFUNCTION(BlueprintCallable, Category = "Libretro|Netplay")
    void NetplayHost(int PeerId);

    /** Join the netplay room hosted by the given peer id. @see ULibretroCoreInstance::NetplaySync */
    UFUNCTION(BlueprintCallable, Category = "Libretro|Netplay")
    void NetplaySync(int PeerId);

    /** Leave the current netplay room and resume playing locally. @see ULibretroCoreInstance::NetplayLeave */
    UFUNCTION(BlueprintCallable, Category = "Libretro|Netplay")
    void NetplayLeave();

protected:
    friend class ULibretroCoreInstance;

    // Set at launch by the owning component; cleared (invalidating the handle) at shutdown
    struct FLibretroContext* Context = nullptr;

    // Captured launch context so the handle is self-sufficient
    FString CorePath;
    FString RomPath;

    // For surfacing netplay errors on the owning component's OnNetplayError delegate
    TWeakObjectPtr<class ULibretroCoreInstance> OwningComponent;
};
