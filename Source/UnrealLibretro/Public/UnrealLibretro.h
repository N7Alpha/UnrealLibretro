// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"

UNREALLIBRETRO_API DECLARE_LOG_CATEGORY_EXTERN(Libretro, Log, All);

class FUnrealLibretroModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;


	/** Path Resolution */
	static FString CorePath(const FString& Core)
	{
		auto UnrealLibretro = IPluginManager::Get().FindPlugin("UnrealLibretro");
		verify(UnrealLibretro.IsValid());

		return FPaths::Combine(UnrealLibretro->GetBaseDir(), TEXT("MyCores"), Core);
	}

	static FString ROMPath(const FString& Rom)
	{
		auto UnrealLibretro = IPluginManager::Get().FindPlugin("UnrealLibretro");
		verify(UnrealLibretro.IsValid());
		
		return FPaths::Combine(UnrealLibretro->GetBaseDir(), TEXT("MyROMs"), Rom);
	}

	static FString SaveStatePath(const FString& Rom, const FString& Identifier)
	{
		auto UnrealLibretro = IPluginManager::Get().FindPlugin("UnrealLibretro");
		verify(UnrealLibretro.IsValid());

		return FPaths::Combine(UnrealLibretro->GetBaseDir(), TEXT("Saves"), TEXT("SaveStates"), Rom, Identifier + ".sav");
	}

	static FString SRAMPath(const FString& Rom, const FString& Identifier)
	{
		auto UnrealLibretro = IPluginManager::Get().FindPlugin("UnrealLibretro");
		verify(UnrealLibretro.IsValid());

		return FPaths::Combine(UnrealLibretro->GetBaseDir(), TEXT("Saves"), TEXT("SRAM"), Rom, Identifier + ".srm");
	}

	// As a libretro frontend you own directory path data that you provide to the core
	static TStaticArray<char, 1024> retro_save_directory;
	static TStaticArray<char, 1024> retro_system_directory;

private:
	void* SDLHandle;

#ifdef PLATFORM_WINDOWS
	FString RedistDirectory;
#endif
};
