#include "UnrealLibretro.h"

#define SAM2_SERVER
THIRD_PARTY_INCLUDES_START
#include "sam2.h"
THIRD_PARTY_INCLUDES_END

#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"

#include "LibretroSettings.h"

DEFINE_LOG_CATEGORY(Libretro)

#define LOCTEXT_NAMESPACE "FUnrealLibretroModule"

#if PLATFORM_WINDOWS
void* OpenGLDLL;
PFN_wglCreateContext _wglCreateContext;
PFN_wglDeleteContext _wglDeleteContext;
PFN_wglMakeCurrent _wglMakeCurrent;
PFN_wglGetProcAddress _wglGetProcAddress;
#endif

char UnrealLibretroVersionAnsi[256] = {0}; // IModuleInterface can give you this but it's GameThread only

class FSam2ServerRunnable : public FRunnable
{
public:
    FSam2ServerRunnable()
    {
        Sam2Server = (sam2_server_t*)malloc(sizeof(sam2_server_t));
        sam2_server_init(Sam2Server, SAM2_SERVER_DEFAULT_PORT);

        uv_async_init(&Sam2Server->loop, &StopAsync, OnStopAsync);
        StopAsync.data = this;
    }

    ~FSam2ServerRunnable()
    {
        uv_close((uv_handle_t*)&StopAsync, NULL);
        free(Sam2Server);
    }

    virtual uint32 Run() override
    {
        // Handle TCP requests. This call returns only after calling uv_stop()
        uv_run(&Sam2Server->loop, UV_RUN_DEFAULT);

        // Start asynchronous destruction
        sam2_server_begin_destroy(Sam2Server);

        // Do the needful
        uv_run(&Sam2Server->loop, UV_RUN_DEFAULT);
        uv_loop_close(&Sam2Server->loop);

        return 0;
    }

    static void OnStopAsync(uv_async_t* handle)
    {
        FSam2ServerRunnable* This = (FSam2ServerRunnable*)handle->data;
        uv_stop(&This->Sam2Server->loop);
    }

    virtual void Stop() override
    {
        uv_async_send(&StopAsync);
    }

    sam2_server_t* Sam2Server;
    uv_async_t StopAsync;
};

void FUnrealLibretroModule::StartupModule()
{
    check(IsInGameThread()); // For IPluginManager
    auto UnrealLibretro = IPluginManager::Get().FindPlugin("UnrealLibretro");
    check(UnrealLibretro.IsValid());
    const FPluginDescriptor& Descriptor = UnrealLibretro->GetDescriptor();
    FString VersionName = Descriptor.VersionName;

    FCStringAnsi::Strncpy(UnrealLibretroVersionAnsi, TCHAR_TO_ANSI(*VersionName), sizeof(UnrealLibretroVersionAnsi) - 1);

    Sam2ServerRunnable = new FSam2ServerRunnable();
    Sam2ServerThread = FRunnableThread::Create(Sam2ServerRunnable, TEXT("Sam2ServerThread"));

    // This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
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

    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = DefWindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = TEXT("my_opengl_class");
    RegisterClass(&wc);
#endif
}

void FUnrealLibretroModule::ShutdownModule()
{
    if (Sam2ServerThread)
    {
        Sam2ServerRunnable->Stop();
        Sam2ServerThread->WaitForCompletion();
        delete Sam2ServerThread;
        delete Sam2ServerRunnable;
        Sam2ServerRunnable = nullptr;
        Sam2ServerThread = nullptr;
    }

    // @todo For now I skip resource cleanup. It could be added back if I added isReadyForFinishDestroy(bool) to ULibretroCoreInstance
    // in conjunction with waiting for the FLibretroContext to destruct since UE uses the outstanding UObjects from this module visible through
    // the reflection system (UProperty, etc)  to determine when it is safe to shutdown this module.
    // This is because FLibretroContext depends on the dlls and paths loaded by this module and is destructed asynchronously and is not a UObject.
    // I could also fix the shutdown_audio hack as well as remove the numerous weak pointers in FLibretroContext.
    // I'm nervous how much the engine will block the game thread on that condition though so that still might not be a solution.
#if 0

#if PLATFORM_WINDOWS
    FPlatformProcess::FreeDllHandle(OpenGLDLL);
    OpenGLDLL = nullptr;
    UnregisterClass(TEXT("my_opengl_class"), GetModuleHandle(NULL));
#endif

    // @todo Remove RedistDirectory from Searchpath
#endif
}

#include "Logging/LogMacros.h"
extern "C"
void LibretroSam2LogWrite(int level, const char* file, int line, const char* fmt, ...) {
    char buffer[1536];

    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    FString formattedMessage = ANSI_TO_TCHAR(buffer);

    // Log the message
    switch (level) {
    default:
    case 4: UE_LOG(Libretro, Fatal  , TEXT("%s"), *formattedMessage); break;
    case 0: UE_LOG(Libretro, Verbose, TEXT("%s"), *formattedMessage); break;
    case 1: UE_LOG(Libretro, Log    , TEXT("%s"), *formattedMessage); break;
    case 2: UE_LOG(Libretro, Warning, TEXT("%s"), *formattedMessage); break;
    case 3: UE_LOG(Libretro, Error  , TEXT("%s"), *formattedMessage); break;
    }
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUnrealLibretroModule, UnrealLibretro)
