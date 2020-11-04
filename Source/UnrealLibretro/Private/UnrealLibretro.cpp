// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "UnrealLibretro.h"
#include "Core.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "GameFramework/InputSettings.h"

#if PLATFORM_WINDOWS
#include "Windows/PreWindowsApi.h"
#include "Windows/SDL2/SDL.h"
#include "Windows/PostWindowsApi.h"
#endif

#if PLATFORM_APPLE
#include <dispatch/dispatch.h>
#endif

DEFINE_LOG_CATEGORY(Libretro)

#define LOCTEXT_NAMESPACE "FUnrealLibretroModule"

void FUnrealLibretroModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

	// Get the base directory of this plugin
	FString BaseDir = IPluginManager::Get().FindPlugin("UnrealLibretro")->GetBaseDir();

	// Add on the relative location of the third party dll and load it
	FString LibraryPath;
#if PLATFORM_WINDOWS
	LibraryPath = FPaths::Combine(*BaseDir, TEXT("Binaries/Win64/ThirdParty/SDL2.dll"));
#elif PLATFORM_MAC
    LibraryPath = FPaths::Combine(*BaseDir, TEXT("Binaries/Mac/ThirdParty/libSDL2.dylib"));
#endif // PLATFORM_WINDOWS

#define LIBRETRO_NOTE " Note: disable UnrealLibretro or delete the UnrealLibretro plugin to make this error go away." \
                      " You can also post an issue to github."
#define LIBRETRO_MODULE_LOAD_ERROR(msg) FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("LibretroError", msg LIBRETRO_NOTE)); \
                                        UE_LOG(Libretro, Fatal, TEXT(msg LIBRETRO_NOTE));

	SDLHandle = !LibraryPath.IsEmpty() ? FPlatformProcess::GetDllHandle(*LibraryPath) : nullptr;

	if (!SDLHandle)
	{
		LIBRETRO_MODULE_LOAD_ERROR("Failed to load SDL2.dll");
	}

#if PLATFORM_APPLE
	dispatch_sync(dispatch_get_main_queue(), ^ {
#endif
	int load_sdl_error = SDL_Init(SDL_INIT_VIDEO);
#if PLATFORM_APPLE
	});
#endif
	if (load_sdl_error) 
	{
		LIBRETRO_MODULE_LOAD_ERROR("Failed to initialize SDL2");
	}
}

void FUnrealLibretroModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	// Free the dll handle
#if PLATFORM_APPLE
	dispatch_sync(dispatch_get_main_queue(), ^ {
#endif
	SDL_Quit();
#if PLATFORM_APPLE
	});
#endif
	
	FPlatformProcess::FreeDllHandle(SDLHandle);
	SDLHandle = nullptr;
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FUnrealLibretroModule, UnrealLibretro)
