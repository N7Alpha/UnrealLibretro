#include "UnrealLibretro.h"

#define SAM2_SERVER
THIRD_PARTY_INCLUDES_START
#include "sam2.h"
THIRD_PARTY_INCLUDES_END

#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"

#include "LibretroSettings.h"

DEFINE_LOG_CATEGORY(Libretro);

#define LOCTEXT_NAMESPACE "FUnrealLibretroModule"

#if PLATFORM_WINDOWS
void* OpenGLDLL;
PFN_wglCreateContext _wglCreateContext;
PFN_wglDeleteContext _wglDeleteContext;
PFN_wglMakeCurrent _wglMakeCurrent;
PFN_wglGetProcAddress _wglGetProcAddress;
PFN_wglShareLists _wglShareLists;
#endif

char UnrealLibretroVersionAnsi[256] = {0}; // IModuleInterface can give you this but it's GameThread only

void FUnrealLibretroModule::StartupModule()
{
    check(IsInGameThread()); // For IPluginManager
    auto UnrealLibretro = IPluginManager::Get().FindPlugin("UnrealLibretro");
    check(UnrealLibretro.IsValid());
    const FPluginDescriptor& Descriptor = UnrealLibretro->GetDescriptor();
    FString VersionName = Descriptor.VersionName;

    FCStringAnsi::Strncpy(UnrealLibretroVersionAnsi, TCHAR_TO_ANSI(*VersionName), sizeof(UnrealLibretroVersionAnsi) - 1);

    Sam2ServerInstance = (sam2_server_t*)malloc(sizeof(sam2_server_t));
    if (!Sam2ServerInstance)
    {
        UE_LOG(Libretro, Fatal, TEXT("Failed to allocate memory for sam2_server_t."));
        return;
    }

    int InitResult = sam2_server_init(Sam2ServerInstance, SAM2_SERVER_DEFAULT_PORT);
    if (InitResult != 0)
    {
        UE_LOG(Libretro, Warning, TEXT("Failed to initialize sam2_server. Error code: %d. Server will not run."), InitResult);
        free(Sam2ServerInstance);
        Sam2ServerInstance = nullptr;
    }
    else
    {
        UE_LOG(Libretro, Log, TEXT("sam2_server initialized successfully."));
    }

    if (Sam2ServerInstance)
    {
        TickDelegateHandle = FThreadSafeTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateRaw(this, &FUnrealLibretroModule::GameThread_Tick), 0.0f
        );
    }

#define LIBRETRO_NOTE " Note: disable UnrealLibretro or delete the UnrealLibretro plugin to make this error go away." \
                        " You can also post an issue to github."
#define LIBRETRO_MODULE_LOAD_ERROR(msg) FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("LibretroError", msg LIBRETRO_NOTE)); \
                                        UE_LOG(Libretro, Fatal, TEXT(msg LIBRETRO_NOTE));

#if PLATFORM_WINDOWS
    FString BaseDir = IPluginManager::Get().FindPlugin("UnrealLibretro")->GetBaseDir();
    RedistDirectory = FPaths::Combine(*BaseDir, TEXT("Binaries/Win64/ThirdParty/libretro/"));
    FPlatformProcess::AddDllDirectory(*RedistDirectory);

    OpenGLDLL = FPlatformProcess::GetDllHandle(TEXT("opengl32.dll"));
    if (!OpenGLDLL)
    {
        LIBRETRO_MODULE_LOAD_ERROR("Couldn't load opengl32.dll");
    }

    wglCreateContext = (decltype(wglCreateContext))FPlatformProcess::GetDllExport(OpenGLDLL, TEXT("wglCreateContext"));
    wglGetProcAddress = (decltype(wglGetProcAddress))FPlatformProcess::GetDllExport(OpenGLDLL, TEXT("wglGetProcAddress"));
    wglDeleteContext = (decltype(wglDeleteContext))FPlatformProcess::GetDllExport(OpenGLDLL, TEXT("wglDeleteContext"));
    wglMakeCurrent = (decltype(wglMakeCurrent))FPlatformProcess::GetDllExport(OpenGLDLL, TEXT("wglMakeCurrent"));
    wglShareLists = (decltype(wglShareLists))FPlatformProcess::GetDllExport(OpenGLDLL, TEXT("wglShareLists"));

    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = DefWindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = TEXT("my_opengl_class");
    RegisterClass(&wc);
#endif
}

void FUnrealLibretroModule::ShutdownModule()
{
    if (TickDelegateHandle.IsValid())
    {
        FThreadSafeTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);
        TickDelegateHandle.Reset();
    }

    if (Sam2ServerInstance)
    {
        UE_LOG(Libretro, Log, TEXT("Shutting down SAM2 Server"));
        sam2_server_destroy(Sam2ServerInstance);
        free(Sam2ServerInstance);
        Sam2ServerInstance = nullptr;
    }

    // @todo For now I skip resource cleanup for background thread shared handles. It could be added back if I added isReadyForFinishDestroy(bool) to ULibretroCoreInstance
    // in conjunction with waiting for the FLibretroContext to destruct since UE uses the outstanding UObjects from this module visible through
    // the reflection system (UProperty, etc)  to determine when it is safe to shutdown this module.
    // This is because FLibretroContext depends on the dlls and paths loaded by this module and is destructed asynchronously and is not a UObject.
    // I could also fix the shutdown_audio hack as well as remove the numerous weak pointers in FLibretroContext.
    // I'm nervous how much the engine will block the game thread on that condition though so that still might not be a solution.
#if 0

#if PLATFORM_WINDOWS
    if (OpenGLDLL)
    {
        FPlatformProcess::FreeDllHandle(OpenGLDLL);
        OpenGLDLL = nullptr;
    }
    UnregisterClass(TEXT("my_opengl_class"), GetModuleHandle(NULL));
#endif
#endif
    UE_LOG(Libretro, Log, TEXT("FUnrealLibretroModule Shutdown."));
}

bool FUnrealLibretroModule::GameThread_Tick(float DeltaTime)
{
    check(IsInGameThread());
    if (Sam2ServerInstance)
    {
        //DECLARE_SCOPE_CYCLE_COUNTER(TEXT("SAM2ServerTick"), STAT_LibretroSAM2ServerTick, STATGROUP_UnrealLibretro);
        sam2_server_poll(Sam2ServerInstance);
    }
    return true;
}

extern "C"
void sam2_log_write(int level, const char* file, int line, const char* fmt, ...) {
    char buffer[1536];

    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    FString formattedMessage = ANSI_TO_TCHAR(buffer);

    // Log the message
    switch (level) {
    default:
    case 4: UE_LOG(Libretro, Fatal  , TEXT("Netplay: %s"), *formattedMessage); break;
    case 0: UE_LOG(Libretro, Verbose, TEXT("Netplay: %s"), *formattedMessage); break;
    case 1: UE_LOG(Libretro, Log    , TEXT("Netplay: %s"), *formattedMessage); break;
    case 2: UE_LOG(Libretro, Warning, TEXT("Netplay: %s"), *formattedMessage); break;
    case 3: UE_LOG(Libretro, Error  , TEXT("Netplay: %s"), *formattedMessage); break;
    }
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUnrealLibretroModule, UnrealLibretro)
