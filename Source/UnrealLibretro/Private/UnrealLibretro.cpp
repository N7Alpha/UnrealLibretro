#include "UnrealLibretro.h"

#include "Modules/ModuleManager.h"

#if PLATFORM_APPLE
#include <dispatch/dispatch.h>
#endif

#if PLATFORM_WINDOWS
#include "Windows/PreWindowsApi.h"
#include "Windows/SDL2/SDL.h"
#include "Windows/PostWindowsApi.h"
#endif

#include "LibretroSettings.h"

DEFINE_LOG_CATEGORY(Libretro)

#define LOCTEXT_NAMESPACE "FUnrealLibretroModule"

void FUnrealLibretroModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

	FString BaseDir = IPluginManager::Get().FindPlugin("UnrealLibretro")->GetBaseDir();

#define LIBRETRO_NOTE " Note: disable UnrealLibretro or delete the UnrealLibretro plugin to make this error go away." \
						" You can also post an issue to github."
#define LIBRETRO_MODULE_LOAD_ERROR(msg) FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("LibretroError", msg LIBRETRO_NOTE)); \
										UE_LOG(Libretro, Fatal, TEXT(msg LIBRETRO_NOTE));

#if !PLATFORM_ANDROID
#if PLATFORM_APPLE
	dispatch_sync(dispatch_get_main_queue(), ^ {
#endif
	SDL_SetMainReady(); // Hackery I had to patch SDL source so that setting this doesn't segfault. This is needed for Android
	int load_sdl_error = SDL_Init(SDL_INIT_VIDEO);
#if PLATFORM_APPLE
	});
#endif

	if (load_sdl_error)
	{
		UE_LOG(Libretro, Fatal, TEXT("Failed to initialize SDL2: (error code %d) %s"), load_sdl_error, ANSI_TO_TCHAR(SDL_GetError()));
	}
#endif

#if PLATFORM_WINDOWS
	FPlatformProcess::AddDllDirectory(*RedistDirectory);
#endif
}

void FUnrealLibretroModule::ShutdownModule()
{
	// @todo For now I skip resource cleanup. It could be added back if I added isReadyForFinishDestroy(bool) to ULibretroCoreInstance
	// in conjunction with waiting for the LibretroContext to destruct since UE uses the outstanding UObjects from this module visible through
	// the reflection system (UProperty, etc)  to determine when it is safe to shutdown this module.
	// This is because LibretroContext depends on the dlls and paths loaded by this module and is destructed asynchronously and is not a UObject.
	// I could also fix the shutdown_audio hack as well as remove the numerous weak pointers in LibretroContext.
	// I'm nervous how much the engine will block the game thread on that condition though so that still might not be a solution.
#if 0
#if PLATFORM_APPLE
	dispatch_sync(dispatch_get_main_queue(), ^ {
#endif
	SDL_Quit();
#if PLATFORM_APPLE
	});
#endif
	
	FPlatformProcess::FreeDllHandle(SDLHandle);
	SDLHandle = nullptr;

	// @todo Remove RedistDirectory from Searchpath
#endif
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FUnrealLibretroModule, UnrealLibretro)
