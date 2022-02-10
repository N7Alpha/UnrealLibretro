#pragma once
#include "Engine/DeveloperSettings.h"

#include "LibretroSettings.generated.h"

UCLASS(Config = UnrealLibretro, meta=(DisplayName="Unreal Libretro"))
class UNREALLIBRETRO_API ULibretroSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Path to the 'system' directory passed to Libretro Cores */
	UPROPERTY(Config, EditAnywhere, Category = Libretro)
	FString CoreSaveDirectory;

	/** Path to the 'save' directory passed to Libretro Cores */
	UPROPERTY(Config, EditAnywhere, Category = Libretro)
	FString CoreSystemDirectory;

	FName GetCategoryName() const override
	{
		return TEXT("Plugins");
	}
};