#pragma once
#include "Engine/DeveloperSettings.h"

#include "LibretroSettings.generated.h"

UCLASS(Config = UnrealLibretro, meta=(DisplayName="Unreal Libretro"))
class UNREALLIBRETRO_API ULibretroSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    /** Path to the 'save' directory passed to Libretro Cores */
    UPROPERTY(Config, EditAnywhere, Category = Libretro)
    FString CoreSaveDirectory;

    /** Path to the 'system' directory passed to Libretro Cores */
    UPROPERTY(Config, EditAnywhere, Category = Libretro)
    FString CoreSystemDirectory;

    /** Core options that are loaded for every core. They will be overwritten by options set for a specific ULibretroCoreInstance */
    UPROPERTY(config, EditAnywhere, Category = Libretro)
    TMap<FString, FString> GlobalCoreOptions;

    /* GetCategoryName and GetSectionName are used for linking to the settings details pane */
    FName GetCategoryName() const override
    {
        return TEXT("Plugins");
    }

    FName GetSectionName() const override
    {
        return TEXT("Unreal Libretro");
    }
};
