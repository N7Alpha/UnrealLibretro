#pragma once

#include "Runtime/Launch/Resources/Version.h"
#include "LibretroCoreInstance.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "UObject/NameTypes.h"

UNREALLIBRETRO_API DECLARE_LOG_CATEGORY_EXTERN(Libretro, Log, All);

extern char UnrealLibretroVersionAnsi[];

#if    ENGINE_MAJOR_VERSION < 5 \
    && ENGINE_MINOR_VERSION < 27
// The version of netImgui I'm using was tested on 4.27 at the earliest it does not build on 4.24
#define UNREALLIBRETRO_NETIMGUI 0
#elif PLATFORM_WINDOWS
#define UNREALLIBRETRO_NETIMGUI 1
#elif PLATFORM_MAC
#define UNREALLIBRETRO_NETIMGUI 1
#else
#define UNREALLIBRETRO_NETIMGUI 0
#endif

#if PLATFORM_WINDOWS && PLATFORM_64BITS
#   define PLATFORM_INDEX 0
#elif PLATFORM_ANDROID_ARM
#   define PLATFORM_INDEX 1
#elif PLATFORM_ANDROID_ARM64
#   define PLATFORM_INDEX 2
#endif

static const struct { FString DistributionPath; FString Extension; FString BuildbotPath; FName ImageName; } CoreLibMetadata[] =
{
    { TEXT("Win64/"),                  "_libretro.dll",           "https://buildbot.libretro.com/nightly/windows/x86_64/latest/",        "Launcher.Platform_Windows.Large" },
    { TEXT("Android/armeabi-v7a/"),    "_libretro_android.so",    "https://buildbot.libretro.com/nightly/android/latest/armeabi-v7a/",   "Launcher.Platform_Android.Large" },
    { TEXT("Android/arm64-v8a/"),      "_libretro_android.so",    "https://buildbot.libretro.com/nightly/android/latest/arm64-v8a/",     "Launcher.Platform_Android.Large" },
//  { TEXT("Linux/x86_64/"),           "_libretro.so",            "https://buildbot.libretro.com/nightly/linux/x86_64/latest/",          "Launcher.Platform_Linux.Large"   },
//  { TEXT("Mac/arm64-v8a/"),          "_libretro.dylib",         "https://buildbot.libretro.com/nightly/apple/osx/arm64/latest/",       "Launcher.Platform_Mac.Large"     },
//  { TEXT("iOS/universal/"),          "_libretro_ios.dylib",     "https://buildbot.libretro.com/nightly/apple/ios-arm64/latest/",       "Launcher.Platform_iOS.Large"     },
};

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"

typedef HGLRC(WINAPI* PFN_wglCreateContext)(HDC);
typedef BOOL(WINAPI* PFN_wglDeleteContext)(HGLRC);
typedef PROC(WINAPI* PFN_wglGetProcAddress)(LPCSTR);
typedef HDC(WINAPI* PFN_wglGetCurrentDC)(void);
typedef HGLRC(WINAPI* PFN_wglGetCurrentContext)(void);
typedef BOOL(WINAPI* PFN_wglMakeCurrent)(HDC, HGLRC);
typedef BOOL(WINAPI* PFN_wglShareLists)(HGLRC, HGLRC);

#define wglCreateContext _wglCreateContext
#define wglDeleteContext _wglDeleteContext
#define wglMakeCurrent _wglMakeCurrent
#define wglGetProcAddress _wglGetProcAddress

#define GL_GET_PROC_ADDRESS Win32GLGetProcAddress

extern void* OpenGLDLL;
extern PFN_wglCreateContext _wglCreateContext;
extern PFN_wglDeleteContext _wglDeleteContext;
extern PFN_wglMakeCurrent _wglMakeCurrent;
extern PFN_wglGetProcAddress _wglGetProcAddress;

static void* Win32GLGetProcAddress(const char* procname)
{
    void* proc = wglGetProcAddress(procname);

    if (!proc)
    {
        proc = FPlatformProcess::GetDllExport(OpenGLDLL, ANSI_TO_TCHAR(procname));
    }

    return proc;
}

#include "Windows/HideWindowsPlatformTypes.h"
#endif

class FUnrealLibretroModule : public IModuleInterface
{
public:

    /** IModuleInterface implementation */
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;


    /** Path Resolution */
    static bool IsCoreName(const FString &CorePath)
    {
        return FPaths::GetExtension(CorePath).IsEmpty() && !CorePath.IsEmpty() && FPaths::GetPath(CorePath).IsEmpty();
    }

    template <typename... PathTypes>
    static FString IfRelativeResolvePathRelativeToThisPluginWithPathExtensions(const FString& Path, PathTypes&&... InPaths)
    {
        if (!FPaths::IsRelative(Path)) return Path;
        check(IsInGameThread()); // For IPluginManager
        auto UnrealLibretro = IPluginManager::Get().FindPlugin("UnrealLibretro");
        check(UnrealLibretro.IsValid());

        return FPaths::Combine(UnrealLibretro->GetBaseDir(), InPaths..., Path);
    }

    static FString ResolveCorePath(FString UnresolvedCorePath)
    {
        if (IsCoreName(UnresolvedCorePath)) 
        {
            UnresolvedCorePath = CoreLibMetadata[PLATFORM_INDEX].DistributionPath + *UnresolvedCorePath + CoreLibMetadata[PLATFORM_INDEX].Extension;
        }

        return IfRelativeResolvePathRelativeToThisPluginWithPathExtensions(UnresolvedCorePath, TEXT("MyCores"));
    }

    static FString ResolveROMPath(const FString& UnresolvedRomPath)
    {
        return IfRelativeResolvePathRelativeToThisPluginWithPathExtensions(UnresolvedRomPath, TEXT("MyROMs"));
    }

    static FString ResolveSaveStatePath(const FString& UnresolvedRomPath, const FString& UnresolvedSavePath)
    {
        return IfRelativeResolvePathRelativeToThisPluginWithPathExtensions(UnresolvedSavePath, TEXT("Saves"), TEXT("SaveStates"), FPaths::GetCleanFilename(UnresolvedRomPath));
    }

    static FString ResolveSRAMPath(const FString& UnresolvedRomPath, const FString& UnresolvedSavePath)
    {
        return IfRelativeResolvePathRelativeToThisPluginWithPathExtensions(UnresolvedSavePath, TEXT("Saves"), TEXT("SRAM"), FPaths::GetCleanFilename(UnresolvedRomPath));
    }

    static TStaticArray<TArray<FLibretroControllerDescription>, PortCount> EnvironmentParseControllerInfo(const retro_controller_info* controller_info)
    {
        TStaticArray<TArray<FLibretroControllerDescription>, PortCount> ControllerDescriptions;

        for (int port = 0; controller_info[port].types != NULL && port < PortCount; port++) {
            for (unsigned t = 0; t < controller_info[port].num_types; t++) {
                if (controller_info[port].types[t].desc == nullptr) break; // Not part of Libretro API but needed check for some cores

                retro_controller_description controller_description = controller_info[port].types[t];
                ControllerDescriptions[port].Add({ controller_description.desc,
                                                   controller_description.id });
            }

            ControllerDescriptions[port].Add({ "None", RETRO_DEVICE_NONE });
        }

        return ControllerDescriptions;
    }

    static TArray<FLibretroOptionDescription> EnvironmentParseOptions(const struct retro_variable* variable_array)
    {
        TArray<FLibretroOptionDescription> ParsedOptions;

        while (variable_array->key != nullptr)
        {
            // Example entry:
            // { .key = "foo_option", .value = "Speed hack coprocessor X; false|true" }
            const FString Value = variable_array->value;

            // Find the position of the semicolon that separates the description and values
            int32 SemicolonIndex;
            if (!Value.FindChar(';', SemicolonIndex))
            {
                UE_LOG(Libretro, Warning, TEXT("Failed to parse libretro core option '%s'. No semicolon"), *Value);
                continue; // Skip this Option
            }

            FLibretroOptionDescription Option;

            // Extract the description substring
            Option.Description = Value.Left(SemicolonIndex);

            // Extract the values substring and split it into individual values
            FString PipeDelimitedValueString = Value.Mid(SemicolonIndex + 1).TrimStart();

            PipeDelimitedValueString.ParseIntoArray(Option.Values, TEXT("|"), false);
            Option.Key = variable_array->key;

            ParsedOptions.Add(Option);
            variable_array++;
        }

        return ParsedOptions;
    }

private:
    class FSam2ServerRunnable* Sam2ServerRunnable = nullptr;
    FRunnableThread* Sam2ServerThread = nullptr;

#ifdef PLATFORM_WINDOWS
    FString RedistDirectory;
#endif
};

#undef PLATFORM_INDEX
