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

    /**
     * Core options applied on top of everything else (including per-instance options) when running
     * headless: dedicated servers, commandlets, and -nullrhi. Hardware rendering is refused in those
     * environments, so use this to steer cores onto their software/null renderers, e.g.
     *   mupen64plus-rdp-plugin = angrylion
     *   dolphin_renderer       = Null
     * These win over per-instance options because they express environment constraints, not preferences
     */
    UPROPERTY(config, EditAnywhere, Category = Libretro, meta = (DisplayName = "Server (Headless) Core Option Overrides"))
    TMap<FString, FString> ServerGlobalCoreOptions;

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
