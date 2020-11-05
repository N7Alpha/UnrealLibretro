// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

UNREALLIBRETRO_API DECLARE_LOG_CATEGORY_EXTERN(Libretro, Log, All);

class FUnrealLibretroModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	
private:
	void* SDLHandle;

#ifdef PLATFORM_WINDOWS
	FString RedistDirectory;
#endif
};
