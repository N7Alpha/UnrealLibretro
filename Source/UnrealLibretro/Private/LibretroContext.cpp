#define SAM2_IMPLEMENTATION
#define ULNET_IMPLEMENTATION

#include "LibretroContext.h"
extern "C"
{
#include "gfx/scaler/pixconv.h"
}

#include "UnrealLibretro.h" // For Libretro debug log category
#if UNREALLIBRETRO_NETIMGUI
#define ULNET_IMGUI
#endif

THIRD_PARTY_INCLUDES_START
#include "sam2.h"
#include "ulnet.h"
#if UNREALLIBRETRO_NETIMGUI
#include "NetImgui_Api.h"
#endif
THIRD_PARTY_INCLUDES_END

#include "Runtime/Launch/Resources/Version.h"

#include "Misc/FileHelper.h"
#include "Misc/App.h" // FApp::CanEverRender

#include "LibretroCoreInstance.h"
#include "LibretroSettings.h"
#include "LibretroInputDefinitions.h"
#include "LambdaRunnable.h"

#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "TextureResource.h"
#include "RenderingThread.h"
#include "Runtime/Launch/Resources/Version.h"

// For the GPU RGB565 conversion pass in core_video_refresh
#include "RHI.h"
#include "RHIStaticStates.h"
#include "PipelineStateCache.h"
#include "ScreenRendering.h"       // Engine's FScreenVS/FScreenPS screen copy shaders
#include "CommonRenderResources.h" // GFilterVertexDeclaration
#include "RendererInterface.h"     // IRendererModule::DrawRectangle
#include "Modules/ModuleManager.h"
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3)
#include "ShaderParameterStruct.h" // SetShaderParametersLegacyPS
#endif

#if PLATFORM_APPLE
#include <dispatch/dispatch.h>
#endif

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <d3d12.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

#if PLATFORM_ANDROID
#define GL_GET_PROC_ADDRESS eglGetProcAddress

#include <android/native_window.h> // requires ndk r5 or newer
#include <EGL/egl.h> // requires ndk r5 or newer
#endif

// Android errors trying to use debug context for some reason even with EGL 1.5
#if defined(DEBUG_OPENGL) && !PLATFORM_ANDROID
#define DEBUG_OPENGL_CALLBACK
#endif

// MY EYEEEEESSS.... Even though this looks heavily obfuscated what this actually accomplishes is relatively simple. It allows us to run multiple libretro cores at once.
// We have to do it this way because when libretro calls a callback we implemented there really isn't any suitable way to tell which core the call came from.
// So we just statically generate a bunch of callback functions with macros and write their function pointers into an array of libretro_callbacks_t's and issue them at runtime.
// These generated callbacks call std::functions which can capture arguments. So we capture this and now it calls our callbacks on a per instance basis.
#define REP10(P, M)  M(P##0) M(P##1) M(P##2) M(P##3) M(P##4) M(P##5) M(P##6) M(P##7) M(P##8) M(P##9)
#define REP100(M) REP10(,M) REP10(1,M) REP10(2,M) REP10(3,M) REP10(4,M) REP10(5,M) REP10(6,M) REP10(7,M) REP10(8,M) REP10(9,M)

extern struct libretro_callbacks_t {
    retro_audio_sample_batch_t                                                c_audio_write;
    retro_video_refresh_t                                                     c_video_refresh;
    retro_audio_sample_t                                                      c_audio_sample;
    retro_environment_t                                                       c_environment;
    retro_input_poll_t                                                        c_input_poll;
    retro_input_state_t                                                       c_input_state;
    retro_hw_get_current_framebuffer_t                                        c_get_current_framebuffer;
    TUniqueFunction<TRemovePointer<retro_audio_sample_batch_t        >::Type>   audio_write; // Changed these to TUniqueFunction because std::function throws exceptions even with -O3 and -fno-exceptions surprisingly https://godbolt.org/z/oqP3vT
    TUniqueFunction<TRemovePointer<retro_video_refresh_t             >::Type>   video_refresh;
    TUniqueFunction<TRemovePointer<retro_audio_sample_t              >::Type>   audio_sample;
    TUniqueFunction<TRemovePointer<retro_environment_t               >::Type>   environment;
    TUniqueFunction<TRemovePointer<retro_input_poll_t                >::Type>   input_poll;
    TUniqueFunction<TRemovePointer<retro_input_state_t               >::Type>   input_state;
    TUniqueFunction<TRemovePointer<retro_hw_get_current_framebuffer_t>::Type>   get_current_framebuffer;
} libretro_callbacks_table[];

#define FUNC_WRAP_INIT(M) {      func_wrap_audio_write##M, func_wrap_video_refresh##M, func_wrap_audio_sample##M, func_wrap_environment##M, func_wrap_input_poll##M, func_wrap_input_state##M, func_wrap_get_current_framebuffer##M },
#define FUNC_WRAP_DEF(M) size_t  func_wrap_audio_write##M(const int16_t *data, size_t frames) { return libretro_callbacks_table[M].audio_write(data, frames); } \
                         void    func_wrap_video_refresh##M(const void *data, unsigned width, unsigned height, size_t pitch) { return libretro_callbacks_table[M].video_refresh(data, width, height, pitch); } \
                         void    func_wrap_audio_sample##M(int16_t left, int16_t right) { return libretro_callbacks_table[M].audio_sample(left, right); } \
                         bool    func_wrap_environment##M(unsigned cmd, void *data) { return libretro_callbacks_table[M].environment(cmd, data); } \
                         void    func_wrap_input_poll##M() { return libretro_callbacks_table[M].input_poll(); } \
                         int16_t func_wrap_input_state##M(unsigned port, unsigned device, unsigned index, unsigned id) { return libretro_callbacks_table[M].input_state(port, device, index, id); } \
                         uintptr_t func_wrap_get_current_framebuffer##M() { return libretro_callbacks_table[M].get_current_framebuffer(); }


REP100(FUNC_WRAP_DEF)
libretro_callbacks_t libretro_callbacks_table[] = { REP100(FUNC_WRAP_INIT) };


void glDebugOutput(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam){

    switch (severity) {
    case GL_DEBUG_SEVERITY_HIGH:
        UE_LOG(Libretro, Error, TEXT("OpenGL Debug: %s"), ANSI_TO_TCHAR(message));
        break;
    case GL_DEBUG_SEVERITY_MEDIUM:
        UE_LOG(Libretro, Warning, TEXT("OpenGL Debug: %s"), ANSI_TO_TCHAR(message));
        break;
    case GL_DEBUG_SEVERITY_LOW:
        UE_LOG(Libretro, Log, TEXT("OpenGL Debug: %s"), ANSI_TO_TCHAR(message));
        break;
    case GL_DEBUG_SEVERITY_NOTIFICATION:
    default:
        UE_LOG(Libretro, Verbose, TEXT("OpenGL Debug: %s"), ANSI_TO_TCHAR(message));
        break;
    }

}

static PFNGLGETERRORPROC glGetError;

#if defined(DEBUG_OPENGL) && !defined(DEBUG_OPENGL_CALLBACK)
#define LogGLErrors(x) GLClearErrors();\
    x;\
    GLLogCall(#x, __FILE__, __LINE__)
#else
#define LogGLErrors(x) x
#endif

static void GLClearErrors()
{
    /* loop while there are errors and until GL_NO_ERROR is returned */
    while (glGetError() != GL_NO_ERROR);
}

static bool GLLogCall(const char* function, const char* file, int line)
{
    while (GLenum error = glGetError())
    {
        UE_LOG(Libretro, Error, TEXT("OpenGL: %s:%s:%d: GLenum (%d)"), ANSI_TO_TCHAR(function), ANSI_TO_TCHAR(file), line, error);
        return false;
    }

    return true;
}

#define UNREALLIBRETRO_FRONTEND_CONTEXT 0x0001
#define UNREALLIBRETRO_SHARED_CONTEXT   0x0002
int FLibretroContext::SwitchOpenGLContext(int context_type) {
    switch (context_type) {
    case UNREALLIBRETRO_FRONTEND_CONTEXT: {
#if PLATFORM_WINDOWS
        if (!wglMakeCurrent(core.gl.hdc, core.gl.context)) {
            UE_LOG(Libretro, Error, TEXT("Failed to make frontend context current"));
            return -1;
        }
#elif PLATFORM_ANDROID
        if (!eglMakeCurrent(core.gl.egl_display,
                          EGL_NO_SURFACE, EGL_NO_SURFACE,
                          core.gl.egl_context)) {
            UE_LOG(Libretro, Error, TEXT("Failed to make frontend context current"));
            return -1;
        }
#endif
        return 0;
    }
    case UNREALLIBRETRO_SHARED_CONTEXT: {
#if PLATFORM_WINDOWS
        if (!wglMakeCurrent(core.gl.shared_hdc, core.gl.shared_context)) {
            UE_LOG(Libretro, Error, TEXT("Failed to make shared context current"));
            return -1;
        }
#elif PLATFORM_ANDROID
        if (!eglMakeCurrent(core.gl.egl_display,
                          EGL_NO_SURFACE, EGL_NO_SURFACE,
                          core.gl.shared_context)) {
            UE_LOG(Libretro, Error, TEXT("Failed to make shared context current"));
            return -1;
        }
#endif
        return 0;
    }
    default:
        UE_LOG(Libretro, Error, TEXT("Invalid context type"));
        return -1;
    }
}

int FLibretroContext::create_window() {
#if PLATFORM_ANDROID
    // Get an OpenGL context via EGL
    const EGLint attribs[] = {
    EGL_BLUE_SIZE, 8,
    EGL_GREEN_SIZE, 8,
    EGL_RED_SIZE, 8,
    EGL_NONE
    };

    EGLConfig config;
    EGLint numConfigs;

    if ((core.gl.egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY)) == EGL_NO_DISPLAY) {
        UE_LOG(Libretro, Fatal, TEXT("eglGetDisplay() returned error %d"), eglGetError());
    }

    if (!eglInitialize(core.gl.egl_display, 0, 0)) {
        UE_LOG(Libretro, Fatal, TEXT("eglInitialize() returned error %d"), eglGetError());
    }

    if (!eglChooseConfig(core.gl.egl_display, attribs, &config, 1, &numConfigs)) {
        UE_LOG(Libretro, Fatal, TEXT("eglChooseConfig() returned error %d"), eglGetError());
    }

    const EGLint attrib_list[] = {
        EGL_CONTEXT_MAJOR_VERSION, (EGLint) core.hw.version_major,
        EGL_CONTEXT_MINOR_VERSION, (EGLint) core.hw.version_minor,
#if defined(DEBUG_OPENGL_CALLBACK)
        EGL_CONTEXT_OPENGL_DEBUG, EGL_TRUE,
#endif
        EGL_NONE
    };

    UE_LOG(Libretro, Log, TEXT("EGL: Trying to load OpenGL ES version %d.%d"), core.hw.version_major, core.hw.version_minor);
    if (!(core.gl.egl_context = eglCreateContext(core.gl.egl_display, config, 0, attrib_list))) {
        UE_LOG(Libretro, Fatal, TEXT("eglCreateContext() returned error %d"), eglGetError());
    }

    //static_assert(EGL_KHR_surfaceless_context, TEXT("This check may break as a false positive. ndk r21 defines this as a macro might need to be queried at runtime on other platforms"));
    if (!eglMakeCurrent(core.gl.egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, core.gl.egl_context)) {
        UE_LOG(Libretro, Fatal, TEXT("eglMakeCurrent() returned error %d"), eglGetError());
    }

    // Shared context creation for Android
    if(core.gl.use_shared_context) {
        const EGLint shared_attribs[] = {
            EGL_CONTEXT_MAJOR_VERSION, core.hw.version_major,
            EGL_CONTEXT_MINOR_VERSION, core.hw.version_minor,
            EGL_NONE
        };
        core.gl.shared_context = eglCreateContext(core.gl.egl_display,
                                                  config,
                                                  core.gl.egl_context,
                                                  shared_attribs);
        if(core.gl.shared_context == EGL_NO_CONTEXT)
            UE_LOG(Libretro, Fatal, TEXT("Failed to create shared EGL context: %d"), eglGetError());
    }
#elif PLATFORM_WINDOWS
    // Use wgl to create a hidden window to get an OpenGL context
    core.gl.window = CreateWindowEx(
        0, TEXT("my_opengl_class"), TEXT("OpenGL Window"),
        WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_POPUP,
        0, 0, 1, 1,
        NULL, NULL, GetModuleHandle(NULL), NULL
    );

    if (!core.gl.window) {
        ErrorMessage = TEXT("Failed to create window");
        UE_LOG(Libretro, Error, TEXT("%s"), *ErrorMessage);
        return 1;
    }

    core.gl.hdc = GetDC(core.gl.window);

    PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR), 1,
        PFD_SUPPORT_OPENGL,
        PFD_TYPE_RGBA, 24, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        PFD_MAIN_PLANE, 0, 0, 0, 0
    };

    int pixel_format = ChoosePixelFormat(core.gl.hdc, &pfd);
    if (!pixel_format) {
        ErrorMessage = TEXT("Failed to choose pixel format");
        UE_LOG(Libretro, Error, TEXT("%s"), *ErrorMessage);
        return 1;
    }

    if (!SetPixelFormat(core.gl.hdc, pixel_format, &pfd)) {
        ErrorMessage = TEXT("Failed to set pixel format");
        UE_LOG(Libretro, Error, TEXT("%s"), *ErrorMessage);
        return 1;
    }

    core.gl.context = wglCreateContext(core.gl.hdc);
    if (!core.gl.context) {
        ErrorMessage = TEXT("Failed to create OpenGL context");
        UE_LOG(Libretro, Error, TEXT("%s"), *ErrorMessage);
        return 1;
    }

    if (!wglMakeCurrent(core.gl.hdc, core.gl.context)) {
        ErrorMessage = TEXT("Failed to activate OpenGL context");
        UE_LOG(Libretro, Error, TEXT("%s"), *ErrorMessage);
        return 1;
    }

    if (core.gl.use_shared_context) {
        // Create a separate window for the shared context
        core.gl.shared_window = CreateWindowEx(
            0, TEXT("my_opengl_class"), TEXT("Shared OpenGL Context"),
            WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_POPUP,
            0, 0, 1, 1,
            NULL, NULL, GetModuleHandle(NULL), NULL
        );

        if (!core.gl.shared_window) {
            UE_LOG(Libretro, Fatal, TEXT("Failed to create shared window"));
        }

        // Get a device context for the shared window
        core.gl.shared_hdc = GetDC(core.gl.shared_window);

        // Set the same pixel format for the shared window
        if (!SetPixelFormat(core.gl.shared_hdc, pixel_format, &pfd)) {
            UE_LOG(Libretro, Fatal, TEXT("Failed to set pixel format for shared context"));
        }

        // Create the shared context
        core.gl.shared_context = wglCreateContext(core.gl.shared_hdc);
        if (!core.gl.shared_context) {
            UE_LOG(Libretro, Fatal, TEXT("Failed to create shared GL context"));
        }

        // Share lists between contexts
        if (!wglShareLists(core.gl.context, core.gl.shared_context)) {
            UE_LOG(Libretro, Fatal, TEXT("Failed to share OpenGL context lists"));
        }
    }
#else
    // @todo Other platforms don't have routines to get OpenGL contexts currently
    // GLFW's interface has an oversight that windows and contexts are created together (You could expose some of it's internals to work around this)
    // and SDL is kind of bloated and wasn't flexible enough for Android to work with it
    // My current thoughts are use Google ANGLE since then the EGL code used above for PLATFORM_ANDROID should work without modification
    // ANGLE potentially could allow for easier interop with Unreal's RHI since you can run ANGLE on top of it if its DX12 or Vulkan
    // However the main issue with ANGLE is that it seems the Libretro Cores need to be built for it in mind and I don't know if that feature is being actively developed right now
    ErrorMessage = TEXT("OpenGL context creation not implemented for this platform");
    UE_LOG(Libretro, Error, TEXT("%s"), *ErrorMessage);
    return 1;
#endif
    if (core.gl.use_shared_context) {
        verify(0 == SwitchOpenGLContext(UNREALLIBRETRO_FRONTEND_CONTEXT));
    }

    #pragma warning(push)
    #pragma warning(disable:4191)

    // Initialize all entry points.
    #define GET_GL_PROCEDURES(Type,Func) Func = (Type)GL_GET_PROC_ADDRESS(#Func);
    ENUM_GL_PROCEDURES(GET_GL_PROCEDURES);
    ENUM_GL_WIN32_INTEROP_PROCEDURES(GET_GL_PROCEDURES);

    // Check that all of the entry points have been initialized.
    bool bFoundAllEntryPoints = true;
    #define CHECK_GL_PROCEDURES(Type,Func) if (Func == NULL) { bFoundAllEntryPoints = false; UE_LOG(Libretro, Warning, TEXT("Failed to find entry point for %s"), TEXT(#Func)); }
    ENUM_GL_PROCEDURES(CHECK_GL_PROCEDURES);
    if (!bFoundAllEntryPoints) {
        ErrorMessage = TEXT("Failed to find all OpenGL entry points.");
        UE_LOG(Libretro, Error, TEXT("%s"), *ErrorMessage);
        return 1;
    }

    ENUM_GL_WIN32_INTEROP_PROCEDURES(CHECK_GL_PROCEDURES);
    // Re-enabled now that the shared D3D12 fence handshake gives the interop path real
    // synchronization (it was disabled for having none). Set false to fall back to PBO readback
    this->gl_win32_interop_supported_by_driver = bFoundAllEntryPoints;

    glGetError = (PFNGLGETERRORPROC)GL_GET_PROC_ADDRESS("glGetError");
    if (!glGetError) {
        ErrorMessage = TEXT("Failed to get glGetError function pointer");
        UE_LOG(Libretro, Error, TEXT("%s"), *ErrorMessage);
        return 1;
    }

    // Restore warning C4191.
    #pragma warning(pop)
#if defined(DEBUG_OPENGL_CALLBACK)
    GLint opengl_flags;
    GLint major_version;
    GLint minor_version;
    bool is_OpenGL_ES = strncmp("OpenGL ES", (char *)glGetString(GL_SHADING_LANGUAGE_VERSION), strlen("OpenGL ES")) == 0;
    glGetIntegerv(GL_CONTEXT_FLAGS, &opengl_flags);
    glGetIntegerv(GL_MAJOR_VERSION, &major_version);
    glGetIntegerv(GL_MINOR_VERSION, &minor_version);
    if (   major_version >= 4 && minor_version >= 3 && !is_OpenGL_ES
        || major_version >= 3 && minor_version >= 2 &&  is_OpenGL_ES
        && (opengl_flags & GL_CONTEXT_FLAG_DEBUG_BIT))
    {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(glDebugOutput, nullptr);
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
    }
#endif

    UE_LOG(Libretro, Log, TEXT("GL_SHADING_LANGUAGE_VERSION: %s\n"), ANSI_TO_TCHAR((char*)glGetString(GL_SHADING_LANGUAGE_VERSION)));
    UE_LOG(Libretro, Log, TEXT("GL_VERSION: %s\n"), ANSI_TO_TCHAR((char*)glGetString(GL_VERSION)));

    return 0;
}

int FLibretroContext::video_configure(const struct retro_game_geometry *geom) {
    if (!core.gl.pixel_format) {
        auto data = RETRO_PIXEL_FORMAT_0RGB1555;
        this->core_environment(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &data);
    }

    // Unreal Resource init
    void *SharedHandle = nullptr;
    void *FenceSharedHandle = nullptr;

    uint64_t SizeInBytes, MipLevels;
    GLenum handleType;
    FTaskGraphInterface::Get().WaitUntilTaskCompletes(
        FFunctionGraphTask::CreateAndDispatchWhenReady([&]
            {
                const unsigned CapacityMilliseconds = 50;
                const unsigned CapacityFrames = CapacityMilliseconds * (core.av.timing.sample_rate / 1000.0);
                Unreal.AudioQueue = MakeShared<TCircularQueue<int32>, ESPMode::ThreadSafe>(CapacityFrames); // @todo move to audio init when the hack below is removed

                if (!FApp::CanEverRender())
                {
                    // Dedicated servers, commandlets, and -nullrhi processes have no render thread or
                    // usable RHI resources. Leave Unreal.TextureRHI null (core_video_refresh drops
                    // frames) and skip audio wiring; the core still runs input, netplay, and savestates
                }
                // Make sure the game objects haven't been GCed
                else if (!UnrealSoundBuffer.IsValid() || !UnrealRenderTarget.IsValid())
                {   // @hack until we acquire our own resources and don't rely on getting them by proxy through a UObject
                    ENQUEUE_RENDER_COMMAND(LibretroInitDummyRHIFramebuffer)
                        ([this](FRHICommandListImmediate& RHICmdList)
                            {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2
                                FRHITextureCreateDesc TextureDesc;
                                TextureDesc.Extent = FIntPoint(core.av.geometry.max_width, core.av.geometry.max_height);
                                TextureDesc.Format = UnrealPixelFormat;
                                TextureDesc.NumMips = 1;
                                TextureDesc.NumSamples = 1;
                                TextureDesc.Flags = ETextureCreateFlags::CPUWritable | ETextureCreateFlags::Dynamic;
                                TextureDesc.Dimension = ETextureDimension::Texture2D;

                                TextureDesc.DebugName = TEXT("Dummy Texture for now");

                                this->Unreal.TextureRHI = RHICreateTexture(TextureDesc);
#else
                                FRHIResourceCreateInfo Info{ TEXT("Dummy Texture for now") };

                                this->Unreal.TextureRHI =
                                    RHICreateTexture2D(core.av.geometry.max_width,
                                        core.av.geometry.max_height,
                                        UnrealPixelFormat,
                                        1,
                                        1,
                                        TexCreate_CPUWritable | TexCreate_Dynamic,
                                        Info);
#endif
                            });
                }
                else
                {
                    // Video Init
                    UnrealRenderTarget->bGPUSharedFlag = true; // Allows us to share this rendertarget with other applications and APIs in this case OpenGL
                    UnrealRenderTarget->InitCustomFormat(core.av.geometry.max_width,
                                                         core.av.geometry.max_height,
                                                         UnrealPixelFormat,
                                                         false);
                    ENQUEUE_RENDER_COMMAND(LibretroInitRHIFramebuffer)
                        ([&](FRHICommandListImmediate& RHICmdList)
                            {
                                const auto Resource = static_cast<FTextureRenderTarget2DResource*>(UnrealRenderTarget->GetRenderTargetResource());
                                this->Unreal.TextureRHI = Resource->GetTextureRHI();

                                if (gl_win32_interop_supported_by_driver)
                                {
#if PLATFORM_WINDOWS
                                    if ((FString)GDynamicRHI->GetName() == TEXT("D3D12"))
                                    {
                                        auto UE4D3DDevice = static_cast<ID3D12Device*>(GDynamicRHI->RHIGetNativeDevice());
                                        static FThreadSafeCounter NamingIdx;
                                        ID3D12Resource* ResolvedTexture = (ID3D12Resource*)this->Unreal.TextureRHI->GetTexture2D()->GetNativeResource();
                                        D3D12_RESOURCE_DESC TextureAttributes = ResolvedTexture->GetDesc();
                                        D3D12_RESOURCE_ALLOCATION_INFO TextureMemoryUsage = UE4D3DDevice->GetResourceAllocationInfo(0b0, 1, &TextureAttributes);
                                        verify(!FAILED(UE4D3DDevice->CreateSharedHandle(ResolvedTexture, NULL, GENERIC_ALL, *FString::Printf(TEXT("OpenGLSharedFramebuffer_UnrealLibretro_%u"), NamingIdx.Increment()), &SharedHandle)));
                                        check(SharedHandle);

                                        MipLevels = TextureAttributes.MipLevels;
                                        SizeInBytes = TextureMemoryUsage.SizeInBytes;
                                        handleType = GL_HANDLE_TYPE_D3D12_RESOURCE_EXT;

#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1)
                                        // Shared timeline fence paired with the shared memory above; GL signals it when a
                                        // frame's rendering completes and our graphics queue waits on it GPU-side.
                                        // Engines before RHIGetNativeGraphicsQueue (5.1) fall back to glFinish in sync mode
                                        ID3D12Fence* InteropFence = nullptr;
                                        if (!FAILED(UE4D3DDevice->CreateFence(0, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&InteropFence))))
                                        {
                                            if (FAILED(UE4D3DDevice->CreateSharedHandle(InteropFence, NULL, GENERIC_ALL, NULL, &FenceSharedHandle)))
                                            {
                                                InteropFence->Release();
                                                InteropFence = nullptr;
                                                FenceSharedHandle = nullptr;
                                            }
                                        }
                                        this->core.gl.d3d12_interop_fence = InteropFence;
#endif
                                    }
#endif
                                }
                            });
                    FlushRenderingCommands();

                    // Audio init
                    UnrealSoundBuffer->SetSampleRate(core.av.timing.sample_rate);
                    UnrealSoundBuffer->NumChannels = 2;
                    UnrealSoundBuffer->AudioQueue = Unreal.AudioQueue;
    }

            }, TStatId(), nullptr, ENamedThreads::GameThread)
    ); // mfence

    // Libretro Core resource init
    if (core.using_opengl) {
        if (gl_win32_interop_supported_by_driver && SharedHandle != nullptr) {
#if PLATFORM_WINDOWS
            UE_LOG(Libretro, Log, TEXT("Sharing RHI RenderTarget memory with OpenGL"))
            glCreateMemoryObjectsEXT(1, &core.gl.rhi_interop_memory);
            glImportMemoryWin32HandleEXT(core.gl.rhi_interop_memory, SizeInBytes, handleType, SharedHandle);
            glCreateTextures(GL_TEXTURE_2D, 1, &core.gl.texture);
            glTextureStorageMem2DEXT(core.gl.texture, MipLevels, GL_RGBA8, core.av.geometry.max_width,
                                                                           core.av.geometry.max_height,
                                                                           core.gl.rhi_interop_memory, 0);

            // NOTE: ID3D12Device::CreateSharedHandle gives as an NT Handle, and so we need to call CloseHandle on it
            if ((FString)GDynamicRHI->GetName() == TEXT("D3D12"))
            {
                verify(CloseHandle(SharedHandle));
            }

            if (FenceSharedHandle != nullptr)
            {
                glGenSemaphoresEXT(1, &core.gl.interop_semaphore);
                // Per EXT_external_objects_win32 a successful import transfers ownership of the NT
                // handle to the GL implementation, so unlike the memory handle above no CloseHandle
                glImportSemaphoreWin32HandleEXT(core.gl.interop_semaphore, GL_HANDLE_TYPE_D3D12_FENCE_EXT, FenceSharedHandle);
            }
#endif
        } else { // RHI Interop not supported fallback to creating OpenGL framebuffer
            glGenTextures(1, &core.gl.texture);

            glBindTexture(GL_TEXTURE_2D, core.gl.texture);
            LogGLErrors(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, geom->max_width,
                                                                 geom->max_height,
                                                                 0,
                                                                 core.gl.pixel_format,
                                                                 core.gl.pixel_type,
                                                                 NULL));
            glBindTexture(GL_TEXTURE_2D, 0);
        }


        glGenFramebuffers(1, &core.gl.framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, core.gl.framebuffer);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, core.gl.texture, 0);

        if (   core.hw.depth
            && core.hw.stencil) {
            glGenRenderbuffers(1, &core.gl.renderbuffer);
            glBindRenderbuffer(GL_RENDERBUFFER, core.gl.renderbuffer);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, geom->max_width,
                                                                        geom->max_height);

            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, core.gl.renderbuffer);
        }
        else if (core.hw.depth) {
            glGenRenderbuffers(1, &core.gl.renderbuffer);
            glBindRenderbuffer(GL_RENDERBUFFER, core.gl.renderbuffer);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, geom->max_width,
                                                                         geom->max_height);

            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, core.gl.renderbuffer);
        }

        glBindRenderbuffer(GL_RENDERBUFFER, 0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            ErrorMessage = TEXT("OpenGL framebuffer is incomplete");
            UE_LOG(Libretro, Error, TEXT("%s"), *ErrorMessage);
            return 1;
        }

        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        {
            glGenBuffers(2, core.gl.pixel_buffer_objects);
            for (int i = 0; i < sizeof(core.gl.pixel_buffer_objects) / sizeof(GLuint); i++)
            {
                glBindBuffer(GL_PIXEL_PACK_BUFFER, core.gl.pixel_buffer_objects[i]);
                glBufferData(GL_PIXEL_PACK_BUFFER, 4 * geom->max_width * geom->max_height, 0, GL_DYNAMIC_READ);
            }
            glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        }
    }

    if (FApp::CanEverRender()) {
        for (int i = 0; i < Unreal.FrameUpload.BUFFER_COUNT; i++) {
            Unreal.FrameUpload.Buffers[i] = FMemory::Malloc(4 * core.av.geometry.max_width
                                                              * core.av.geometry.max_height, PLATFORM_CACHE_LINE_SIZE);
        }
        Unreal.FrameUpload.BufferFreedEvent = FPlatformProcess::GetSynchEventFromPool();
    }

    core.hw.context_reset();
    return 0;
}

#include "Async/TaskGraphInterfaces.h"
// Stripped down code for profiling purposes https://godbolt.org/z/c57esx
 void FLibretroContext::core_video_refresh(const void *data, unsigned width, unsigned height, unsigned pitch) {
    DECLARE_SCOPE_CYCLE_COUNTER(TEXT("PrepareFrameBufferForRenderThread"), STAT_LibretroPrepareFrameBufferForRenderThread, STATGROUP_UnrealLibretro);

    if (!FApp::CanEverRender()) {
        return; // Headless (dedicated server/commandlet/nullrhi): no upload buffers, RHI textures, or render thread exist
    }

    // When the render thread doesn't exist IsInRenderingThread() is true on EVERY thread, so
    // ENQUEUE_RENDER_COMMAND executes its lambda inline right here on the libretro thread and the
    // RHI trips thread-affinity asserts (early startup on Android issue #12, PIE teardown issue #15,
    // and -onethread). Drop the frame instead until threaded rendering is back up
    if (!GIsThreadedRendering) {
        return;
    }

    // Unreal's render queue can be more than one frame deep, so a single pending-frame slot drops
    // frames when the render thread falls behind. Instead each frame gets its own ringbuffer entry
    // and its own unconditionally dispatched upload; when every buffer is in flight we block here
    // until the RHI thread retires the oldest one (issue #35)
    auto acquire_frame_upload_buffer = [&]() -> void*
    {
        while (Unreal.FrameUpload.InFlightCount.GetValue() >= Unreal.FrameUpload.BUFFER_COUNT)
        {
            Unreal.FrameUpload.BufferFreedEvent->Wait();
        }

        // Uploads retire in FIFO order, so with fewer than BUFFER_COUNT in flight the next ring slot is free
        void* buffer = Unreal.FrameUpload.Buffers[Unreal.FrameUpload.NextBufferIndex];
        Unreal.FrameUpload.NextBufferIndex = (Unreal.FrameUpload.NextBufferIndex + 1) % Unreal.FrameUpload.BUFFER_COUNT;
        return buffer;
    };

    // Uploads are sized to the frame the core actually produced rather than the max potential
    // geometry so we don't waste memory and PCIe bandwidth moving rows and columns of stale pixels.
    // RGB565 frames are uploaded raw (2 instead of 4 bytes per pixel) and expanded to the render
    // target format on the GPU with the engine's screen copy shaders
    auto dispatch_frame_upload_to_unreal_RHI = [&](void* const buffer, unsigned upload_width, unsigned upload_height,
                                                   unsigned upload_pitch, bool upload_is_rgb565)
    {
        Unreal.FrameUpload.InFlightCount.Increment();
        ENQUEUE_RENDER_COMMAND(CopyToUnrealFramebufferTask)( // @todo this triggers an assert on MacOS you can get around it by enqueuing through the TaskGraph instead no idea why this is the case
            [this,
            buffer,
            upload_pitch,
            upload_is_rgb565,
            MipIndex = 0,
            Region = FUpdateTextureRegion2D(0, 0, 0, 0, upload_width, upload_height)]
        (FRHICommandListImmediate& RHICmdList)
        {
            if (upload_is_rgb565 && !this->Unreal.SourceRGB565TextureRHI.GetReference())
            {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2
                FRHITextureCreateDesc TextureDesc;
                TextureDesc.Extent = FIntPoint(core.av.geometry.max_width, core.av.geometry.max_height);
                TextureDesc.Format = PF_R5G6B5_UNORM;
                TextureDesc.NumMips = 1;
                TextureDesc.NumSamples = 1;
                TextureDesc.Flags = ETextureCreateFlags::ShaderResource | ETextureCreateFlags::Dynamic;
                TextureDesc.Dimension = ETextureDimension::Texture2D;

                TextureDesc.DebugName = TEXT("LibretroSourceRGB565");

                this->Unreal.SourceRGB565TextureRHI = RHICreateTexture(TextureDesc);
#else
                FRHIResourceCreateInfo Info{ TEXT("LibretroSourceRGB565") };

                this->Unreal.SourceRGB565TextureRHI =
                    RHICreateTexture2D(core.av.geometry.max_width,
                        core.av.geometry.max_height,
                        PF_R5G6B5_UNORM,
                        1,
                        1,
                        TexCreate_ShaderResource | TexCreate_Dynamic,
                        Info);
#endif
            }

            RHICmdList.EnqueueLambda([=, this](FRHICommandList& RHICmdList)
                {
                    auto* UpdateTexture = upload_is_rgb565
                        ? this->Unreal.SourceRGB565TextureRHI.GetReference()
                        : this->Unreal.TextureRHI.GetReference();
                    if (UpdateTexture) {
                        GDynamicRHI->RHIUpdateTexture2D(
#if    ENGINE_MAJOR_VERSION == 5 \
    && ENGINE_MINOR_VERSION >= 2
                            RHICmdList,
#endif
                            UpdateTexture,
                            MipIndex,
                            Region,
                            upload_pitch,
                            (uint8*)buffer);
                    } else {
                        ErrorMessage = TEXT("Texture RHI reference is null");
                        UE_LOG(Libretro, Error, TEXT("%s"), *ErrorMessage);
                    }

                    // Retire the buffer even when the upload is skipped or the libretro thread blocks forever
                    this->Unreal.FrameUpload.InFlightCount.Decrement();
                    this->Unreal.FrameUpload.BufferFreedEvent->Trigger();
                }
            );

            if (upload_is_rgb565)
            {
                // Sample the RGB565 texture into the render target. This is recorded after the
                // EnqueueLambda above and the immediate command list executes FIFO, so the
                // intermediate texture holds this frame's pixels by the time the draw samples it
                auto* RenderTarget = this->Unreal.TextureRHI.GetReference();
                auto* SourceTexture = this->Unreal.SourceRGB565TextureRHI.GetReference();
#if ENGINE_MAJOR_VERSION > 4 || ENGINE_MINOR_VERSION >= 26
                bool render_target_drawable = RenderTarget && EnumHasAnyFlags(RenderTarget->GetFlags(), TexCreate_RenderTargetable);
#else
                bool render_target_drawable = RenderTarget && (RenderTarget->GetFlags() & TexCreate_RenderTargetable) != 0;
#endif
                if (!SourceTexture || !render_target_drawable) {
                    UE_LOG(Libretro, Verbose, TEXT("Skipping RGB565 conversion pass render target isn't drawable"));
                    return;
                }

                FIntPoint TargetSize(RenderTarget->GetSizeX(), RenderTarget->GetSizeY());

#if ENGINE_MAJOR_VERSION > 4 || ENGINE_MINOR_VERSION >= 26
                RHICmdList.Transition(FRHITransitionInfo(RenderTarget, ERHIAccess::Unknown, ERHIAccess::RTV));
#else
                RHICmdList.TransitionResource(EResourceTransitionAccess::EWritable, RenderTarget);
#endif

                FRHIRenderPassInfo RPInfo(RenderTarget, ERenderTargetActions::Load_Store);
                RHICmdList.BeginRenderPass(RPInfo, TEXT("LibretroRGB565Convert"));
                {
                    RHICmdList.SetViewport(0, 0, 0.0f, TargetSize.X, TargetSize.Y, 1.0f);

                    FGraphicsPipelineStateInitializer GraphicsPSOInit;
                    RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
                    GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
                    GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
                    GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
                    GraphicsPSOInit.PrimitiveType = PT_TriangleList;

                    auto ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
                    TShaderMapRef<FScreenVS> VertexShader(ShaderMap);
                    TShaderMapRef<FScreenPS> PixelShader(ShaderMap);

                    GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
#if ENGINE_MAJOR_VERSION > 4 || ENGINE_MINOR_VERSION >= 25
                    GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
                    GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
#else
                    GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
                    GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
#endif

#if ENGINE_MAJOR_VERSION >= 5
                    SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);
#else
                    SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
#endif

#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3)
                    SetShaderParametersLegacyPS(RHICmdList, PixelShader, TStaticSamplerState<SF_Point>::GetRHI(), SourceTexture);
#else
                    PixelShader->SetParameters(RHICmdList, TStaticSamplerState<SF_Point>::GetRHI(), SourceTexture);
#endif

                    IRendererModule& RendererModule = FModuleManager::GetModuleChecked<IRendererModule>("Renderer");
                    RendererModule.DrawRectangle(
                        RHICmdList,
                        0, 0,                        // Dest X, Y
                        Region.Width, Region.Height, // Dest Width, Height
                        0, 0,                        // Source U, V
                        Region.Width, Region.Height, // Source USize, VSize
                        TargetSize,                  // Target buffer size
                        FIntPoint(SourceTexture->GetSizeX(), SourceTexture->GetSizeY()),
#if ENGINE_MAJOR_VERSION > 4 || ENGINE_MINOR_VERSION >= 25
                        VertexShader,                // 4.25 rewrote shaders as handle types DrawRectangle accepts directly
#else
                        *VertexShader,               // 4.24 wants the raw FShader*
#endif
                        EDRF_Default);
                }
                RHICmdList.EndRenderPass();

#if ENGINE_MAJOR_VERSION > 4 || ENGINE_MINOR_VERSION >= 26
                RHICmdList.Transition(FRHITransitionInfo(RenderTarget, ERHIAccess::RTV, ERHIAccess::SRVMask));
#endif
            }
        }
        );
    };

    if (data && data != RETRO_HW_FRAME_BUFFER_VALID) {
        DECLARE_SCOPE_CYCLE_COUNTER(TEXT("CPUConvertAndCopyFramebuffer"), STAT_LibretroCPUConvertAndCopyFramebuffer, STATGROUP_UnrealLibretro);

        if (core.gl.pixel_type == GL_UNSIGNED_SHORT_5_6_5 && GPixelFormats[PF_R5G6B5_UNORM].Supported) {
            // Skip the CPU conversion and hand the core's raw RGB565 rows to the GPU conversion
            // pass: half the copy and PCIe traffic of expanding to 32-bit here
            unsigned dst_pitch = 2 * width;
            uint8* rgb565_buffer = (uint8*)acquire_frame_upload_buffer();
            for (unsigned y = 0; y < height; y++) {
                FMemory::Memcpy(rgb565_buffer + y * dst_pitch, (const uint8*)data + y * pitch, dst_pitch);
            }

            dispatch_frame_upload_to_unreal_RHI(rgb565_buffer, width, height, dst_pitch, true);
        } else {
            unsigned dst_pitch = 4 * width; // Convert into tightly packed rows; a max_width stride uploads stale pixels past the frame's edge
            auto bgra_buffer = acquire_frame_upload_buffer();

            if (core.gl.pixel_format == GL_BGRA) {
            switch (core.gl.pixel_type) {
                case GL_UNSIGNED_SHORT_5_6_5: {
                    conv_rgb565_argb8888(bgra_buffer, data,
                        width, height,
                        dst_pitch, pitch);
                }
                break;
                case GL_UNSIGNED_SHORT_5_5_5_1: {
                    checkNoEntry();
                }
                break;
                case GL_UNSIGNED_BYTE: {
                    conv_copy(bgra_buffer, data,
                        width, height,
                        dst_pitch, pitch);
                }
                break;
                default:
                    checkNoEntry();
                }
            } else {
                switch (core.gl.pixel_type) {
                case GL_UNSIGNED_SHORT_5_6_5: {
                    conv_rgb565_abgr8888(bgra_buffer, data,
                        width, height,
                        dst_pitch, pitch);
                }
                break;
                case GL_UNSIGNED_BYTE: {
                    conv_argb8888_abgr8888(bgra_buffer, data,
                        width, height,
                        dst_pitch, pitch);
                }
                break;
                default:
                    checkNoEntry();
                }
            }

            dispatch_frame_upload_to_unreal_RHI(bgra_buffer, width, height, dst_pitch, false);
        }
    }
    else if (data == RETRO_HW_FRAME_BUFFER_VALID) {
        check(core.using_opengl && core.gl.pixel_type == GL_UNSIGNED_BYTE);

        if (core.gl.rhi_interop_memory) {
#if PLATFORM_WINDOWS && (ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1))
            if (core.gl.interop_semaphore) {
                // GL signals the shared D3D12 fence from the GPU when this frame's rendering
                // completes; UE's graphics queue waits on it GPU-side before executing subsequently
                // submitted work (which includes the scene passes sampling the rendertarget).
                // Neither CPU blocks, so unlike glFinish this can't stall the libretro thread
                // behind Unreal's GPU work or vice versa
                core.gl.interop_fence_value++;
                GLuint64 fence_value = core.gl.interop_fence_value;
                glSemaphoreParameterui64vEXT(core.gl.interop_semaphore, GL_D3D12_FENCE_VALUE_EXT, &fence_value);
                GLenum dst_layout = GL_LAYOUT_SHADER_READ_ONLY_EXT;
                LogGLErrors(glSignalSemaphoreEXT(core.gl.interop_semaphore, 0, nullptr, 1, &core.gl.texture, &dst_layout));
                glFlush(); // The signal has to actually reach the GPU or the D3D12 queue waits forever

                ENQUEUE_RENDER_COMMAND(LibretroInteropFenceWait)([this, fence_value](FRHICommandListImmediate& RHICmdList)
                {
                    RHICmdList.EnqueueLambda([this, fence_value](FRHICommandList&)
                    {
                        ID3D12CommandQueue* Queue = (ID3D12CommandQueue*)GDynamicRHI->RHIGetNativeGraphicsQueue();
                        if (Queue && core.gl.d3d12_interop_fence)
                        {
                            Queue->Wait(core.gl.d3d12_interop_fence, fence_value);
                        }
                    });
                });
            } else
#endif
            if (bSynchronousTickMode.load(std::memory_order_relaxed)) {
                // No shared fence available (UE4 or import failed); brute force. Note glFinish only
                // waits for this GL context's commands, but drivers may schedule them behind other
                // work on the same hardware queue so under GPU contention this can inherit Unreal's
                // GPU frame time
                LogGLErrors(glFinish());
            }
        } else if (bSynchronousTickMode.load(std::memory_order_relaxed)) {
            // Hard GPU sync (RetroArch's name for this trade): stall until the core's GL work
            // completes and read the frame straight back to client memory. Costs GPU/CPU
            // parallelism, buys the lowest latency; by the time this tick returns to
            // RunFrameSynchronously the frame's pixels are dispatched to the render thread.
            // The async PBO ping-pong state below is left parked, so toggling modes mid-session
            // only costs one stale frame when switching back
            DECLARE_SCOPE_CYCLE_COUNTER(TEXT("HardGPUSyncReadback"), STAT_LibretroHardGPUSyncReadback, STATGROUP_UnrealLibretro);

            void* upload_buffer = acquire_frame_upload_buffer();
            LogGLErrors(glBindFramebuffer(GL_READ_FRAMEBUFFER, core.gl.framebuffer));
            LogGLErrors(glBindBuffer(GL_PIXEL_PACK_BUFFER, 0)); // Read back to client memory, not a PBO, which forces the synchronous path
            LogGLErrors(glReadBuffer(GL_COLOR_ATTACHMENT0));
            LogGLErrors(glReadPixels(0, 0,
                width,
                height,
                core.gl.pixel_format,
                core.gl.pixel_type,
                upload_buffer));
            LogGLErrors(glBindFramebuffer(GL_FRAMEBUFFER, 0));

            dispatch_frame_upload_to_unreal_RHI(upload_buffer, width, height, 4 * width, false);
        } else {
        // OpenGL is asynchronous and because of GPU driver reasons (work is executed FIFO for some drivers)
        // if we try reading the framebuffer we'll block here and consequently the framerate will be capped by Unreal Engines framerate
        // which will cause stuttering if its too low since most emulated games logic is tied to the framerate so we async copy the framebuffer and check a fence later

            switch (glClientWaitSync(core.gl.fence, GL_SYNC_FLUSH_COMMANDS_BIT, 0)) {
                case GL_TIMEOUT_EXPIRED:
                    UE_LOG(Libretro, Verbose, TEXT("Frame didn't render in time will try copying next time..."))
                        break;
                case GL_ALREADY_SIGNALED:
                case GL_CONDITION_SATISFIED:
                {
                    DECLARE_SCOPE_CYCLE_COUNTER(TEXT("GPUAsyncCopy"), STAT_LibretroGPUAsyncCopy, STATGROUP_UnrealLibretro);
                    { // Hand off previously copied frame to Unreal
                        glBindBuffer(GL_PIXEL_PACK_BUFFER, core.gl.pixel_buffer_objects[!core.free_framebuffer_index]);

                        void* frame_buffer = glMapBufferRange(GL_PIXEL_PACK_BUFFER,
                                                              0, // Offset
                                                              4 * core.av.geometry.max_width * core.av.geometry.max_height,
                                                              GL_MAP_READ_BIT);
                        if (!frame_buffer) {
                            ErrorMessage = TEXT("Failed to map OpenGL buffer");
                            UE_LOG(Libretro, Error, TEXT("%s"), *ErrorMessage);
                            return;
                        }
                        // The PBO holds the readback issued last frame so it's sized/uploaded with the
                        // dimensions recorded then, not this frame's. Zero means no readback has
                        // completed into it yet (the very first frame maps the primed empty PBO)
                        if (core.gl.pbo_readback_width > 0) {
                            unsigned readback_pitch = 4 * core.gl.pbo_readback_width;
                            // Copy out of the mapped PBO so it can be unmapped and reused for the next
                            // readback while this frame's upload is still queued behind the RHI thread
                            void* upload_buffer = acquire_frame_upload_buffer();
                            FMemory::Memcpy(upload_buffer, frame_buffer, (SIZE_T)readback_pitch * core.gl.pbo_readback_height);
                            dispatch_frame_upload_to_unreal_RHI(upload_buffer,
                                core.gl.pbo_readback_width, core.gl.pbo_readback_height, readback_pitch, false);
                        }
                    }

                    { // Download Libretro Core frame from OpenGL asynchronously
                        LogGLErrors(glBindFramebuffer(GL_READ_FRAMEBUFFER, core.gl.framebuffer));
                        LogGLErrors(glBindBuffer(GL_PIXEL_PACK_BUFFER, core.gl.pixel_buffer_objects[core.free_framebuffer_index]));
                        LogGLErrors(glReadBuffer(GL_COLOR_ATTACHMENT0));
                        if (glUnmapBuffer(GL_PIXEL_PACK_BUFFER) != GL_TRUE) {
                            ErrorMessage = TEXT("Failed to unmap OpenGL buffer");
                            UE_LOG(Libretro, Error, TEXT("%s"), *ErrorMessage);
                            return;
                        }
                        { // Async copy bound framebuffer color component into bound pbo
                            GLint mip_level = 0;
                            void* offset_into_pbo_where_data_is_written = 0x0;
                            // This call is async always and a DMA transfer on most platforms
                            // Only the region the core actually rendered is read back; the rest of the
                            // max-geometry framebuffer is stale pixels not worth the bandwidth
                            LogGLErrors(glReadPixels(0, 0,
                                width,
                                height,
                                core.gl.pixel_format,
                                core.gl.pixel_type,
                                offset_into_pbo_where_data_is_written));
                            core.gl.pbo_readback_width = width;
                            core.gl.pbo_readback_height = height;
                        }
                        glDeleteSync(core.gl.fence);
                        core.gl.fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
                    }

                    core.free_framebuffer_index = !core.free_framebuffer_index;

                    LogGLErrors(glBindFramebuffer(GL_FRAMEBUFFER, 0));
                    LogGLErrors(glBindBuffer(GL_PIXEL_PACK_BUFFER, 0));
                    break;
                }
                case GL_WAIT_FAILED:
                default:
                    ErrorMessage = TEXT("OpenGL fence sync wait failed");
                    UE_LOG(Libretro, Error, TEXT("%s"), *ErrorMessage);
            }
        }
    }
    else {
        // *Duplicate frame*
        return;
    }
}

size_t FLibretroContext::core_audio_write(const int16_t *buf, size_t frames) {
    if (Unreal.AudioQueue == nullptr) {
        UE_LOG(Libretro, Warning, TEXT("Call to core_audio_write when AudioQueue was null this is a programming error or the Libretro Core is doing something weird"));
        return 0;
    }

    unsigned FramesEnqueued = 0;
    while (FramesEnqueued < frames && Unreal.AudioQueue->Enqueue(((int32*)buf)[FramesEnqueued])) {
        FramesEnqueued++;
    }

    if (FramesEnqueued != frames) {
        UE_LOG(Libretro, Verbose, TEXT("Buffer underrun: %u"), frames - FramesEnqueued);
    }

    return frames;

}


static void core_log(enum retro_log_level level, const char *fmt, ...) {
    char buffer[4096] = {0};
    static const char * levelstr[] = { "dbg", "inf", "wrn", "err" };
    va_list va;

    va_start(va, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, va);
    va_end(va);

    switch (level) {
    case RETRO_LOG_DEBUG:
        UE_LOG(Libretro, Log, TEXT("%s"), ANSI_TO_TCHAR(buffer));
        break;
    case RETRO_LOG_INFO:
        UE_LOG(Libretro, Log, TEXT("%s"), ANSI_TO_TCHAR(buffer));
        break;
    case RETRO_LOG_WARN:
        UE_LOG(Libretro, Warning, TEXT("%s"), ANSI_TO_TCHAR(buffer));
        break;
    case RETRO_LOG_ERROR:
        UE_LOG(Libretro, Warning, TEXT("%s"), ANSI_TO_TCHAR(buffer));
        break;
    }

}

bool FLibretroContext::core_environment(unsigned cmd, void *data) {
    bool delegate_status{false};
    if (CoreEnvironmentCallback)
    {
        delegate_status = CoreEnvironmentCallback(cmd, data);
    }

    switch (cmd) {
    case RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE: {
        auto rumble = (retro_rumble_interface*)data;
        rumble->set_rumble_state = [](unsigned, retro_rumble_effect, uint16_t){ return false; };
        return true;
    }
    case RETRO_ENVIRONMENT_GET_VARIABLE: {
        retro_variable* var = (struct retro_variable*)data;

        int i = 0;
        for (FString TargetKey = FString(var->key); i < OptionDescriptions.Num(); i++) {
            if (OptionDescriptions[i].Key == TargetKey) break;
        }

        if (i == OptionDescriptions.Num()) {
            UE_LOG(Libretro, Warning, TEXT ("Core '%s' violated libretro spec asked for unknown option '%s'"), UTF8_TO_TCHAR(system.library_name), UTF8_TO_TCHAR(var->key));
            return false;
        }

        FString TargetValue = OptionDescriptions[i].Values[OptionSelectedIndex[i].load(std::memory_order_relaxed)];

        int32 Utf8Length = FTCHARToUTF8_Convert::ConvertedLength(*TargetValue, TargetValue.Len());
        TArray<char>& TargetValueCString = OptionsCache.FindOrAdd(var->key);
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 4
        TargetValueCString.SetNumZeroed(Utf8Length + 1, EAllowShrinking::No);
#else
        TargetValueCString.SetNumZeroed(Utf8Length + 1, false);
#endif

        FTCHARToUTF8_Convert::Convert(TargetValueCString.GetData(), Utf8Length, *TargetValue, TargetValue.Len());
        var->value = TargetValueCString.GetData();

        return true;
    }
    case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE: {
        // @todo Integrate with netarch
        bool* core_should_query_for_options = (bool*)data;

        // atomic<>::exchange is necessary otherwise we could miss newly set options because of a race condition
        *core_should_query_for_options = OptionsHaveBeenModified.exchange(false, std::memory_order_acquire);

        return true;
    }
    case RETRO_ENVIRONMENT_SET_VARIABLES: {
        // Queuing this and synchronizing on the game thread prevents a data race
        FTaskGraphInterface::Get().WaitUntilTaskCompletes(FFunctionGraphTask::CreateAndDispatchWhenReady([this, data]() 
            { 
                OptionDescriptions = FUnrealLibretroModule::EnvironmentParseOptions((const struct retro_variable*)data);

                if (OptionSelectedIndex.Num() > 0 && OptionSelectedIndex.Num() != OptionDescriptions.Num()) {
                    UE_LOG(Libretro, Warning, TEXT("Core violated libretro spec size of Options changed"));
                }

                // By libretro spec the 0 index setting is the default one
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 4
                OptionSelectedIndex.SetNumZeroed(OptionDescriptions.Num(), EAllowShrinking::No);
#else
                OptionSelectedIndex.SetNumZeroed(OptionDescriptions.Num(), false);
#endif
            }, 
            TStatId(), nullptr, ENamedThreads::GameThread));

        for (int i = 0; i < OptionDescriptions.Num(); i++)
        {
            if (FString* TargetValue = StartingOptions.Find(OptionDescriptions[i].Key))
            {
                auto TargetIndex = OptionDescriptions[i].Values.IndexOfByKey(*TargetValue);

                if (TargetIndex == INDEX_NONE)
                {
                    UE_LOG(Libretro, Warning, TEXT("Value '%s' does not exist for option '%s'"), **TargetValue, *OptionDescriptions[i].Key);
                    continue;
                }

                OptionSelectedIndex[i].store(TargetIndex, std::memory_order_relaxed);
            }
        }

        return true;
    }
    case RETRO_ENVIRONMENT_GET_LOG_INTERFACE: {
        struct retro_log_callback *cb = (struct retro_log_callback *)data;
        cb->log = core_log;
        return true;
    }
    case RETRO_ENVIRONMENT_GET_CAN_DUPE: {
        bool *bval = (bool *)data;
        *bval = true;
        return true;
    }
    case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME: {
        libretro_api.supports_no_game = *(bool*)data;
        return true;
    }
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: {
        const enum retro_pixel_format *format = (enum retro_pixel_format *)data;

        if (*format > RETRO_PIXEL_FORMAT_RGB565) {
            return false;
        }

        if (core.gl.texture) {
            ErrorMessage = TEXT("Tried to change pixel format after initialization.");
            UE_LOG(Libretro, Error, TEXT("%s"), *ErrorMessage);
            return false;
        }

        switch (*format) {
            case RETRO_PIXEL_FORMAT_0RGB1555:
                core.gl.pixel_type = GL_UNSIGNED_SHORT_5_5_5_1;
                core.gl.pixel_format = GL_BGRA;
                core.gl.bits_per_pixel = sizeof(uint16_t);
                break;
            case RETRO_PIXEL_FORMAT_XRGB8888:

                core.gl.pixel_type = GL_UNSIGNED_BYTE;
                core.gl.pixel_format = GL_BGRA;
                core.gl.bits_per_pixel = sizeof(uint32_t);

                break;
            case RETRO_PIXEL_FORMAT_RGB565:
                core.gl.pixel_type = GL_UNSIGNED_SHORT_5_6_5;
                core.gl.pixel_format = GL_BGRA;
                core.gl.bits_per_pixel = sizeof(uint16_t);
                break;
            default:
                ErrorMessage = FString::Printf(TEXT("Unknown pixel type %u"), *format);
                UE_LOG(Libretro, Error, TEXT("%s"), *ErrorMessage);
                return false;
        }

        // @todo The OpenGL RHI backend is supposed to swizzle the red and blue components of textures created with render targets
        // so in effect it should be a BGRA however that doesn't seem to be the case and it always behaves like rgba
        // I'll probably clean all this stuff up when I do a big optimization of the framebuffer copying stuff
        if ((FString)GDynamicRHI->GetName() == TEXT("OpenGL")) {
            UnrealPixelFormat = PF_R8G8B8A8;
            core.gl.pixel_format = GL_RGBA;
        }

        return true;
    }
    case RETRO_ENVIRONMENT_SET_HW_RENDER: {
        struct retro_hw_render_callback *hw = (struct retro_hw_render_callback*)data;

        // Refusing here is the libretro-sanctioned negotiation path, and multi-renderer cores
        // handle it: Dolphin falls back to its Null video backend when every SET_HW_RENDER
        // attempt is refused, and mupen64plus-next never asks at all with the angrylion
        // software RDP. Accepting on a platform that can't produce a GL context would only
        // fail later in video context init, taking the whole core launch down with it.
        bool gl_context_creation_implemented = PLATFORM_WINDOWS || PLATFORM_ANDROID;
        if (!FApp::CanEverRender() || !gl_context_creation_implemented) {
            return false;
        }

        if (hw->context_type >= RETRO_HW_CONTEXT_VULKAN) {
            UE_LOG(Libretro, Warning, TEXT("Core requested non-OpenGL hardware context %d which is unsupported; refusing so it can fall back"), (int)hw->context_type);
            return false;
        }
        hw->get_current_framebuffer = libretro_callbacks->c_get_current_framebuffer;
#pragma warning(push)
#pragma warning(disable:4191)
        hw->get_proc_address = (retro_hw_get_proc_address_t)GL_GET_PROC_ADDRESS;
#pragma warning(pop)
        core.hw = *hw;
        core.using_opengl = true;

        if (core.hw.context_type == RETRO_HW_CONTEXT_OPENGLES2) { core.hw.version_major = 2; core.hw.version_minor = 0; }
        if (core.hw.context_type == RETRO_HW_CONTEXT_OPENGLES3) { core.hw.version_major = 3; core.hw.version_minor = 0; }

        return true;
    }
    case RETRO_ENVIRONMENT_SET_HW_SHARED_CONTEXT: {
        core.gl.use_shared_context = true;
        return true;
    }
    case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO: {
        auto system_av_info = *(const struct retro_system_av_info*)data;

        check(   system_av_info.geometry.max_height <= core.av.geometry.max_height  // @todo buffer reallocation. The libretro core can basically request to reallocate framebuffers atm this requires writing code to potentially reallocate 4 buffers
              && system_av_info.geometry.max_width  <= core.av.geometry.max_width); // in the case of software rendering the client buffer in opengl the pbo and texture buffer and in both cases the unreal engine rhi buffer on top of this there are
                                                                                    // annoying synchonization issues to deal with. We just assert here so we don't actually have to do that for now

        this->core.av.timing = system_av_info.timing;

        return true;
    }
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY: {
        const char** retro_path = (const char**)data;
        auto RAII = StringCast<TCHAR>(this->core.save_directory.GetData());
        verify(IFileManager::Get().MakeDirectory(RAII.Get(), true));
        *retro_path = core.save_directory.GetData();

        return true;
    }
    case RETRO_ENVIRONMENT_GET_CORE_ASSETS_DIRECTORY:
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY: {
        const char** retro_path = (const char**)data;
        auto RAII = StringCast<TCHAR>(this->core.system_directory.GetData());
        verify(IFileManager::Get().MakeDirectory(RAII.Get(), true));
        *retro_path = core.system_directory.GetData();

        return true;
    }
    case RETRO_ENVIRONMENT_GET_LANGUAGE: {
        unsigned* language = (unsigned*)data;
        *language = RETRO_LANGUAGE_ENGLISH;

        return true;
    }
    case RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE: {
        // This could potentially be useful if the object in unreal engine displaying the video and audio is either out of sight or earshot

        return false;
    }
    case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS: {
        auto input_descriptor = (const struct retro_input_descriptor*)data;

        do {
            UE_LOG(Libretro, Verbose, TEXT("Button Found: %s"), ANSI_TO_TCHAR(input_descriptor->description));
        } while ((++input_descriptor)->description);

        return true;
    }
    case RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK: {
        auto keyboard_callback = (const struct retro_keyboard_callback*)data;

        libretro_api.keyboard_event = keyboard_callback->callback;

        return true;
    }
    case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO: {
        FTaskGraphInterface::Get().WaitUntilTaskCompletes(FFunctionGraphTask::CreateAndDispatchWhenReady([this, data]()
            { ControllerDescriptions = FUnrealLibretroModule::EnvironmentParseControllerInfo((const struct retro_controller_info*)data);  },
            TStatId(), nullptr, ENamedThreads::GameThread));

        return true;
    }
    case RETRO_ENVIRONMENT_GET_PREFERRED_HW_RENDER: {
        unsigned* library = (unsigned*)data;
        if (!FApp::CanEverRender() || !(PLATFORM_WINDOWS || PLATFORM_ANDROID)) {
            // Steer cores toward their software/null renderer up front; any SET_HW_RENDER
            // they try anyway will be refused (see above)
            *library = RETRO_HW_CONTEXT_NONE;
            return true;
        }
#if PLATFORM_ANDROID | PLATFORM_IOS
        *library = RETRO_HW_CONTEXT_OPENGLES3; // Unreal Engine minimum spec requires OpenGL ES 3.1
#else
        *library = RETRO_HW_CONTEXT_OPENGL_CORE;
#endif
        return true;
    }
    case RETRO_ENVIRONMENT_SET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE: {
        const struct retro_hw_render_context_negotiation_interface* iface =
            (const struct retro_hw_render_context_negotiation_interface*)data;

        core.hw_render_context_negotiation = iface;
        return false;
    }
    case RETRO_ENVIRONMENT_GET_LED_INTERFACE: {
        auto led_interface = (struct retro_led_interface *) data;
        // UAE expects this to not be null even if you don't implement it
        led_interface->set_led_state = [](int led, int state) {
            // noop
        };

        return false;
    }
    case RETRO_ENVIRONMENT_GET_FASTFORWARDING: {
        auto is_fast_forwarding = (bool*)data;
        *is_fast_forwarding = false;
        return true;
    }
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY: {
        return false;
    }
    default:
        if (!delegate_status) {
        core_log(RETRO_LOG_WARN, "Unhandled env #%u", cmd);
        }

        return delegate_status;
    }

    return delegate_status;
}

// Unfinished experiment with less branchy version of this function https://godbolt.org/z/hYeYxr95r
int16_t FLibretroContext::core_input_state(unsigned port, unsigned device, unsigned index, unsigned id) {
    // To get the core to poll for certain types of input sometimes requires setting particular controllers for compatible ports
    // or changing specific options related to the input you're trying to poll for. If it's not obvious your main resources are
    // experimenting in Retroarch, forums, the libretro documentation, or looking through the core's code itself.
    // Regarding searching the repo of the core you're having trouble with on github you can search for symbols from libretro.h directly
    // in the repo you're browsing with the search bar. You can even change the url from e.g. github.com/libretro/[CORE] to github1s.com/libretro/[CORE]
    // to get a web based IDE with syntax highlighting and code navigation
    //
    // Also here are some pitfalls I've encountered:
    // - The core/ROM might need to poll an input as active for at least two frames for it to register properly. I know the Zapper in nestopia requires this
    // - Some cores will not poll for any input by default (I fix this by always binding the RETRO_DEVICE_JOYPAD)
    // - The RETRO_DEVICE_POINTER interface is generally preferred over the lightgun and mouse even for things like lightguns and mice although you still use some parts of the lightgun interface for handling lightgun input probably same goes for mouse

    switch (device & RETRO_DEVICE_MASK) {
    case RETRO_DEVICE_JOYPAD:   return InputState[port][to_integral(ERetroDeviceID::JoypadB)     + id];
    case RETRO_DEVICE_LIGHTGUN: return InputState[port][to_integral(ERetroDeviceID::LightgunX)   + id];
    case RETRO_DEVICE_ANALOG:   return InputState[port][to_integral(ERetroDeviceID::AnalogLeftX) + 2 * index + (id % RETRO_DEVICE_ID_JOYPAD_L2)]; // The indexing logic is broken and might OOBs if we're queried for something that isn't an analog trigger or stick
    case RETRO_DEVICE_POINTER:  return InputState[port][to_integral(ERetroDeviceID::PointerX)    + 4 * index + id];
    case RETRO_DEVICE_MOUSE:
    case RETRO_DEVICE_KEYBOARD:
    default:                    return 0;
    }
}


void FLibretroContext::core_audio_sample(int16_t left, int16_t right) {
    int16_t buf[2] = {left, right};
    core_audio_write(buf, (size_t)1);
}

int FLibretroContext::load(const char *sofile) {
    void (*set_environment)(retro_environment_t) = NULL;
    void (*set_video_refresh)(retro_video_refresh_t) = NULL;
    void (*set_input_poll)(retro_input_poll_t) = NULL;
    void (*set_input_state)(retro_input_state_t) = NULL;
    void (*set_audio_sample)(retro_audio_sample_t) = NULL;
    void (*set_audio_sample_batch)(retro_audio_sample_batch_t) = NULL;

    libretro_api.handle = FPlatformProcess::GetDllHandle(ANSI_TO_TCHAR(sofile));

    if (!libretro_api.handle) {
        ErrorMessage = FString::Printf(TEXT("Failed to load core: %s"), ANSI_TO_TCHAR(sofile));
        UE_LOG(LogTemp, Warning, TEXT("%s"), *ErrorMessage);
        return 1;
    }

    // Define a macro to handle symbol loading errors
    #define load_sym(V, S) do { \
        if (0 == ((*(void**)&V) = FPlatformProcess::GetDllExport(libretro_api.handle, TEXT(#S)))) { \
            ErrorMessage = FString::Printf(TEXT("Failed to load symbol '" #S "': %u"), FPlatformMisc::GetLastError()); \
            UE_LOG(Libretro, Warning, TEXT("%s"), *ErrorMessage); \
            return 1; \
        } \
    } while (0)

    #define load_retro_sym(S) load_sym(libretro_api.S, retro_##S)

    // Load all required symbols
    load_retro_sym(init);
    load_retro_sym(deinit);
    load_retro_sym(api_version);
    load_retro_sym(get_system_info);
    load_retro_sym(get_system_av_info);
    load_retro_sym(set_controller_port_device);
    load_retro_sym(reset);
    load_retro_sym(run);
    load_retro_sym(load_game);
    load_retro_sym(unload_game);
    load_retro_sym(get_memory_data);
    load_retro_sym(get_memory_size);
    load_retro_sym(serialize_size);
    load_retro_sym(serialize);
    load_retro_sym(unserialize);

    load_sym(set_environment, retro_set_environment);
    load_sym(set_video_refresh, retro_set_video_refresh);
    load_sym(set_input_poll, retro_set_input_poll);
    load_sym(set_input_state, retro_set_input_state);
    load_sym(set_audio_sample, retro_set_audio_sample);
    load_sym(set_audio_sample_batch, retro_set_audio_sample_batch);

    #undef LOAD_SYM_SAFE
    #undef LOAD_RETRO_SYM_SAFE

    libretro_callbacks->video_refresh = [this](const void* data, unsigned width, unsigned height, size_t pitch) { return core_video_refresh(data, width, height, pitch); };
    libretro_callbacks->audio_write = [this](const int16_t *data, size_t frames) { return core_audio_write(data, frames); };
    libretro_callbacks->audio_sample = [this](int16_t left, int16_t right) { return core_audio_sample(left, right); };
    libretro_callbacks->input_state = [this](unsigned port, unsigned device, unsigned index, unsigned id) { return core_input_state(port, device, index, id); };
    libretro_callbacks->input_poll = [this]() {
        memset(InputState, 0, sizeof(InputState)); // @todo To query the input for sparse packets rather than this laborious subroutine
        ulnet_input_poll(netplay_session, (ulnet_input_state_t (*)[ULNET_PORT_COUNT]) &InputState); // @todo Cast violates strict-aliasing
    };
    libretro_callbacks->environment = [this](unsigned cmd, void* data) { return core_environment(cmd, data); };
    libretro_callbacks->get_current_framebuffer = [this]() { return core.gl.framebuffer; };

    set_environment(libretro_callbacks->c_environment);
    set_video_refresh(libretro_callbacks->c_video_refresh);
    set_input_poll(libretro_callbacks->c_input_poll);
    set_input_state(libretro_callbacks->c_input_state);
    set_audio_sample(libretro_callbacks->c_audio_sample);
    set_audio_sample_batch(libretro_callbacks->c_audio_write);

    libretro_api.init();
    libretro_api.initialized = true;

    return 0;
}


int FLibretroContext::load_game(const char* filename) {
    struct retro_game_info info = { filename , nullptr, (size_t)0, "" };
    TArray<uint8> gameBinary;

    if (!FFileHelper::LoadFileToArray(gameBinary, UTF8_TO_TCHAR(filename))) {
        ErrorMessage = FString::Printf(TEXT("Failed to load game file: %s"), UTF8_TO_TCHAR(filename));
        UE_LOG(Libretro, Warning, TEXT("%s"), *ErrorMessage);
        return 1;
    }

    rom_hash = ulnet_xxh32(gameBinary.GetData(), gameBinary.Num(), 0);

    if (filename && !system.need_fullpath) {
        info.data = gameBinary.GetData();
        info.size = gameBinary.Num();
    }

    libretro_api.game_loaded = libretro_api.load_game(&info);
    if (!libretro_api.game_loaded) {
        UE_LOG(Libretro, Error, TEXT("The core failed to load the content."));
        return 1;
    }

    libretro_api.get_system_av_info(&core.av);

    if (core.using_opengl) {
        // SDL State isn't threadlocal like OpenGL so we have to synchronize here when we create a window @todo Since I'm no longer using SDL it could be looking into if this is still required
#if PLATFORM_APPLE
        __block int ErrorCode = 0;
        // Apple OS's impose an additional requirement that 'all' rendering operations are done on the main thread
        dispatch_sync(dispatch_get_main_queue(),
            ^{
                ErrorCode = create_window();
             });
#else
        int ErrorCode;
        static FCriticalSection WindowLock; // Threadsafe initializatation as of c++11
        FScopeLock scoped_lock(&WindowLock);
        ErrorCode = create_window();
#endif

        if (ErrorCode) {
            return ErrorCode;
        }
    }

    if (core.gl.use_shared_context) {
        verify(0 == SwitchOpenGLContext(UNREALLIBRETRO_SHARED_CONTEXT));
    }
    int ErrorCode = video_configure(&core.av.geometry);
    if (core.gl.use_shared_context) {
        verify(0 == SwitchOpenGLContext(UNREALLIBRETRO_FRONTEND_CONTEXT));
    }

    return ErrorCode;
}

void memor(void *dst, const void *src, size_t n) {
    int16_t *d = (int16_t *)dst;
    const int16_t *s = (const int16_t *)src;
    size_t words = n / sizeof(int16_t);

    for (size_t i = 0; i < words; i++) {
        d[i] |= s[i];
    }
}

#if PLATFORM_MAC
// dyld coalesces C++ weak-definition symbols across every loaded image, even under two-level
// namespaces and RTLD_LOCAL, matching purely by symbol name. Two byte-identical copies of a core
// therefore share the winner's weak definitions -- and through them its internal statics. Fix:
// in the per-instance copy, clear MH_WEAK_DEFINES|MH_BINDS_TO_WEAK (so the image neither offers
// nor seeks coalescing) and zero the LC_DYLD_INFO weak_bind table (whose whole purpose is
// re-pointing this image's references at the coalescing winner). Build-time bindings all target
// the image's own definitions, so nothing is left dangling. The edit invalidates the code
// signature, which arm64 requires, so the copy is re-signed ad hoc afterward.
static bool MacIsolateWeakBindsForInstancedCore(const FString& DylibPath)
{
    TArray<uint8> Binary;
    if (!FFileHelper::LoadFileToArray(Binary, *DylibPath))
    {
        return false;
    }

    auto PatchMachO = [](uint8* MachO, int64 Size) -> bool
    {
        constexpr uint32 MH_MAGIC_64 = 0xfeedfacf;
        constexpr uint32 MH_WEAK_DEFINES_AND_BINDS_TO_WEAK = 0x8000 | 0x10000;
        constexpr uint32 LC_DYLD_INFO_ANY = 0x22; // LC_DYLD_INFO; LC_DYLD_INFO_ONLY is 0x22 | LC_REQ_DYLD
        constexpr int64 HeaderSize = 32;

        if (Size < HeaderSize || *(uint32*)MachO != MH_MAGIC_64)
        {
            return false; // 32-bit and big-endian slices don't occur for the platforms we load
        }

        uint32 CommandCount = *(uint32*)(MachO + 16);
        *(uint32*)(MachO + 24) &= ~MH_WEAK_DEFINES_AND_BINDS_TO_WEAK;

        int64 Offset = HeaderSize;
        for (uint32 i = 0; i < CommandCount && Offset + 8 <= Size; i++)
        {
            uint32 Command = *(uint32*)(MachO + Offset);
            uint32 CommandSize = *(uint32*)(MachO + Offset + 4);
            if ((Command & 0x7fffffff) == LC_DYLD_INFO_ANY && Offset + 32 <= Size)
            {
                *(uint32*)(MachO + Offset + 24) = 0; // weak_bind_off
                *(uint32*)(MachO + Offset + 28) = 0; // weak_bind_size
            }
            if (CommandSize == 0)
            {
                break;
            }
            Offset += CommandSize;
        }
        return true;
    };

    constexpr uint32 FAT_CIGAM = 0xbebafeca; // FAT_MAGIC as read on little-endian
    bool bPatched = false;
    if (Binary.Num() >= 8 && *(uint32*)Binary.GetData() == FAT_CIGAM)
    {
        auto SwapBE = [](uint32 v) { return (v >> 24) | ((v >> 8) & 0xff00) | ((v << 8) & 0xff0000) | (v << 24); };
        uint32 SliceCount = SwapBE(*(uint32*)(Binary.GetData() + 4));
        for (uint32 i = 0; i < SliceCount; i++)
        {
            int64 ArchOffset = 8 + (int64)i * 20; // struct fat_arch
            if (ArchOffset + 20 > Binary.Num()) break;
            uint32 SliceOffset = SwapBE(*(uint32*)(Binary.GetData() + ArchOffset + 8));
            uint32 SliceSize   = SwapBE(*(uint32*)(Binary.GetData() + ArchOffset + 12));
            if ((int64)SliceOffset + SliceSize <= Binary.Num())
            {
                bPatched |= PatchMachO(Binary.GetData() + SliceOffset, SliceSize);
            }
        }
    }
    else
    {
        bPatched = PatchMachO(Binary.GetData(), Binary.Num());
    }

    if (!bPatched || !FFileHelper::SaveArrayToFile(Binary, *DylibPath))
    {
        return false;
    }

    // The byte edits invalidated the code signature; arm64 refuses to load unsigned images
    int32 ReturnCode = -1;
    FString StdOut, StdErr;
    FPlatformProcess::ExecProcess(TEXT("/usr/bin/codesign"),
        *FString::Printf(TEXT("-f -s - \"%s\""), *DylibPath), &ReturnCode, &StdOut, &StdErr);
    if (ReturnCode != 0)
    {
        UE_LOG(Libretro, Warning, TEXT("codesign failed (%d): %s"), ReturnCode, *StdErr);
        return false;
    }

    return true;
}
#endif

FLibretroContext* FLibretroContext::Launch(ULibretroCoreInstance* LibretroCoreInstance, FString core, FString game, UTextureRenderTarget2D* RenderTarget, URawAudioSoundWave* SoundBuffer, TUniqueFunction<void(FLibretroContext*, libretro_api_t&, const FString&)> LoadedCallback)
{

    check(IsInGameThread()); // So static initialization is safe + UObject access

    static constexpr uint32 max_instances = sizeof(libretro_callbacks_table) / sizeof(libretro_callbacks_table[0]);
    static TBitArray<TInlineAllocator<(max_instances / 8) + 1>> AllocatedInstances(false, max_instances);

    FLibretroContext *l = new FLibretroContext();
    l->TickRequestedEvent = FPlatformProcess::GetSynchEventFromPool();
    l->TickCompletedEvent = FPlatformProcess::GetSynchEventFromPool();

    // Grab a statically generated callback structure
    int32 InstanceNumber;
    {
        InstanceNumber = AllocatedInstances.FindAndSetFirstZeroBit();
        check(InstanceNumber != INDEX_NONE);
        l->libretro_callbacks = libretro_callbacks_table + InstanceNumber;
    }

    auto ConvertPath = [](auto &core_directory, const FString& CoreDirectory)
    {
        FString AbsoluteCoreDirectory = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*FUnrealLibretroModule::IfRelativeResolvePathRelativeToThisPluginWithPathExtensions(CoreDirectory));
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 4
        core_directory.SetNumZeroed(TStringConvert<TCHAR, char>::ConvertedLength(*AbsoluteCoreDirectory, AbsoluteCoreDirectory.Len()) + 1, EAllowShrinking::No);
#else
        core_directory.SetNumZeroed(TStringConvert<TCHAR, char>::ConvertedLength(*AbsoluteCoreDirectory, AbsoluteCoreDirectory.Len()) + 1);
#endif
        TStringConvert<TCHAR, char>::Convert(core_directory.GetData(), // has internal assertion if fails
                                             core_directory.Num(),
                                            *AbsoluteCoreDirectory,
                                             AbsoluteCoreDirectory.Len());
    };

#if UNREALLIBRETRO_NETIMGUI
    static ImFontAtlas* GlobalFontAtlas = nullptr;
    if (GlobalFontAtlas == nullptr) {
        GlobalFontAtlas = new ImFontAtlas();
        ImFontConfig fontConfig;
        GlobalFontAtlas->AddFontDefault(&fontConfig);
        GlobalFontAtlas->Build();
    }
#endif

    l->netplay_session = (ulnet_session_t *) ULNET_MALLOC(sizeof(ulnet_session_t));
    memset(l->netplay_session, 0, sizeof(ulnet_session_t));
    ulnet_session_init_defaulted(l->netplay_session);
    l->netplay_session->delay_frames = FMath::Clamp(LibretroCoreInstance->NetplayDelayFrames, 0, ULNET_DELAY_FRAMES_MAX);

    // Capture the declarative netplay intent before the core thread starts
    ELibretroNetplayRole ResolvedNetplayRole = LibretroCoreInstance->NetplayRole;
    if (ResolvedNetplayRole == ELibretroNetplayRole::FromUnrealNetRole)
    {
        // Authority (dedicated/listen server or standalone) hosts; clients join. With the same
        // NetplayPeerId configured everywhere, the room finds itself with no further wiring
        AActor* NetOwner = LibretroCoreInstance->GetOwner();
        ResolvedNetplayRole = !NetOwner || NetOwner->HasAuthority()
            ? ELibretroNetplayRole::Host
            : ELibretroNetplayRole::Join;
    }
    l->PendingNetplayRole = (int)ResolvedNetplayRole;
    l->PendingNetplayPeerId = LibretroCoreInstance->NetplayPeerId;
    // Join must target a concrete id, and FromUnrealNetRole only works when every side shares one
    // (the authority claims it, clients look for it) -- so both require a valid NetplayPeerId
    bool bRequiresConcretePeerId = ResolvedNetplayRole == ELibretroNetplayRole::Join
        || LibretroCoreInstance->NetplayRole == ELibretroNetplayRole::FromUnrealNetRole;
    if (   bRequiresConcretePeerId
        && (l->PendingNetplayPeerId <= SAM2_PORT_SENTINELS_MAX || l->PendingNetplayPeerId > 65535))
    {
        UE_LOG(Libretro, Warning, TEXT("NetplayRole requires a concrete NetplayPeerId but %d is invalid (must be %d-65535); netplay disabled for this launch"),
            LibretroCoreInstance->NetplayPeerId, SAM2_PORT_SENTINELS_MAX + 1);
        l->PendingNetplayRole = 0;
    }
    {
        AActor* Owner = LibretroCoreInstance->GetOwner();
        FTCHARToUTF8 RoomNameUtf8(Owner ? *Owner->GetName() : *LibretroCoreInstance->GetName());
        FCStringAnsi::Strncpy(l->netplay_room_name, RoomNameUtf8.Get(), sizeof(l->netplay_room_name));
    }

    auto LibretroSettings = GetDefault<ULibretroSettings>();

    ConvertPath(l->core.save_directory,   LibretroSettings->CoreSaveDirectory);
    ConvertPath(l->core.system_directory, LibretroSettings->CoreSystemDirectory);

    l->StartingOptions = LibretroSettings->GlobalCoreOptions;
    l->StartingOptions.Append(LibretroCoreInstance->EditorPresetOptions); // Potentially overrides global options
    if (!FApp::CanEverRender())
    {
        // Environment constraints beat per-instance preferences: headless processes refuse hardware
        // rendering (see RETRO_ENVIRONMENT_SET_HW_RENDER), so e.g. a per-instance GLideN64 preset
        // must yield to the server's angrylion override or the core won't produce frames at all
        l->StartingOptions.Append(LibretroSettings->ServerGlobalCoreOptions);
    }

    l->UnrealRenderTarget = MakeWeakObjectPtr(RenderTarget);
    l->UnrealSoundBuffer  = MakeWeakObjectPtr(SoundBuffer);

    // Kick the initialization process off to another thread. It shouldn't be added to the Unreal task pool because those are too slow and my code relies on OpenGL state being thread local.
    // The Runnable system is the standard way for spawning and managing threads in Unreal. FThread looks enticing, but they removed any way to detach threads since "it doesn't work as expected"
    l->LambdaRunnable = FLambdaRunnable::RunLambdaOnBackGroundThread(FPaths::GetCleanFilename(core) + FPaths::GetCleanFilename(game),
        [=, LoadedCallback = MoveTemp(LoadedCallback), EditorPresetControllers = LibretroCoreInstance->EditorPresetControllers, WeakLibretroCoreInstance = MakeWeakObjectPtr(LibretroCoreInstance),
        Sam2ServerAddress = LibretroCoreInstance->Sam2ServerAddress]() {
            sam2_room_t NetplayRoomOld = {};
            int64_t DesyncedFrameOld = 0;
            int ErrorCode;
            // Here I load a copy of the dll instead of the original. If you load the same dll multiple times you won't obtain a new instance of the dll loaded into memory,
            // instead all variables and function pointers will point to the original loaded dll
            // WARNING: Don't ever even try to load the original dll since the editor needs to load it to query core settings (This can happen when you pause in PIE!)
            const FString InstancedCorePath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(
#if PLATFORM_ANDROID
                // On Android .so's have to be copied to your private application directory before they are loaded
                // @enhancement Use dlmopen and package files in private application directory then you wouldn't have to copy the dll every time https://man.archlinux.org/man/dlmopen.3.en#:~:text=LM_ID_NEWLM,is%20initially%0A%20%20%20%20%20%20empty
                //              Although this solution would be limited to 16 'namespaces' which is probably enough for practical purposes but not great
                *FString::Printf(TEXT("/data/data/%s/files/%s.%s"), *FAndroidPlatformProcess::GetGameBundleId(), *FGuid::NewGuid().ToString(), *FPaths::GetExtension(*core))
#else
                *FString::Printf(TEXT("%s.%s"), *FGuid::NewGuid().ToString(), *FPaths::GetExtension(*core))
#endif
            );

            if (!IPlatformFile::GetPlatformPhysical().CopyFile(*InstancedCorePath, *core)) {
                l->ErrorMessage = FString::Printf(TEXT("Failed to copy core file from %s to %s"), *core, *InstancedCorePath);
                UE_LOG(Libretro, Warning, TEXT("%s"), *l->ErrorMessage);
                l->CoreState.store(ECoreState::StartFailed, std::memory_order_release);
                goto cleanup;
            }

#if PLATFORM_MAC
            // Loading per-instance copies is not enough for isolation on macOS: dyld coalesces C++
            // weak-definition symbols ACROSS images, so the second copy's weak-bound function tables
            // (e.g. snes9x's PPU renderer tables) end up pointing into the first copy's code and
            // statics -- visually a "console VRAM corruption" when cores run side by side. Neutering
            // the copy's weak-bind participation keeps every reference bound within its own image.
            if (!MacIsolateWeakBindsForInstancedCore(InstancedCorePath))
            {
                UE_LOG(Libretro, Warning, TEXT("Could not isolate weak-bound symbols in '%s'; running multiple"
                    " instances of this core in one process may corrupt each other's state"), *InstancedCorePath);
            }
#endif

            l->core.hw.version_major = 4;
            l->core.hw.version_minor = 5;
            l->core.hw.context_type = RETRO_HW_CONTEXT_OPENGL_CORE;
            l->core.hw.context_reset = []() {};
            l->core.hw.context_destroy = []() {};

            // Loads the dll and its function pointers into libretro_api
            ErrorCode = l->load(TCHAR_TO_UTF8(*InstancedCorePath));
            if (ErrorCode)
            {
                l->CoreState.store(ECoreState::StartFailed, std::memory_order_release);
                goto cleanup;
            }

            l->libretro_api.get_system_info(&l->system);

            for (int Port = 0; Port < PortCount; Port++)
            {
                unsigned DeviceID = RETRO_DEVICE_DEFAULT;
                if (const FLibretroControllerDescriptions* CorePresetControllers = EditorPresetControllers.Find(l->system.library_name))
                {
                    DeviceID = (*CorePresetControllers)[Port].ID;
                }

                l->DeviceIDs[Port] = DeviceID;
                l->libretro_api.set_controller_port_device(Port, DeviceID);
            }

            if (!l->libretro_api.supports_no_game && game.IsEmpty())
            {
                l->ErrorMessage = FString::Printf(TEXT("Failed to launch Libretro core '%s'. Path given for ROM was empty"), *core);
                UE_LOG(Libretro, Warning, TEXT("%s"), *l->ErrorMessage);
                l->CoreState.store(ECoreState::StartFailed, std::memory_order_release);
                goto cleanup;
            }

            // This does load the game but does many other things as well. If hardware rendering is needed it loads OpenGL resources from the OS and this also initializes the unreal engine resources for audio and video.
            ErrorCode = l->load_game(game.IsEmpty() ? nullptr : TCHAR_TO_UTF8(*game));
            if (ErrorCode)
            {
                UE_LOG(Libretro, Error, TEXT("Error loading %s"), *game);
                l->CoreState.store(ECoreState::StartFailed, std::memory_order_release);
                goto cleanup;
            }

            l->CoreState.store(ECoreState::Running, std::memory_order_release);
            LoadedCallback(l, l->libretro_api, l->ErrorMessage);

            // This simplifies the logic in core_video_refresh, It stops us from erroring when we try to unmap this pixel buffer in core_video_refresh
            // You could just move this to the beginning of core_video_refresh and surround it with an if statement that does this the first time through
            if (l->core.using_opengl)
            {
                l->glBindBuffer(GL_PIXEL_PACK_BUFFER, l->core.gl.pixel_buffer_objects[l->core.free_framebuffer_index]);
                l->glMapBufferRange(GL_PIXEL_PACK_BUFFER,
                                    0, // Offset
                                    1, // Size...
                                    GL_MAP_READ_BIT);
                l->core.gl.fence = l->glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
                l->glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
            }
            
            sam2_client_connect(&l->sam_socket, TCHAR_TO_ANSI(*Sam2ServerAddress), SAM2_SERVER_DEFAULT_PORT);
            ulnet_set_stun_server(l->netplay_session, TCHAR_TO_ANSI(*Sam2ServerAddress), SAM2_SERVER_DEFAULT_PORT);

            l->netplay_session->user_ptr = (void*)l;
            l->netplay_session->sam2_send_callback = [](void* user_ptr, char* message) {
                FLibretroContext* LibretroContext = (FLibretroContext*)user_ptr;
                return sam2_client_send(LibretroContext->sam_socket, message);
            };
            // Fires on the core thread from inside ulnet_poll_session/ulnet_service_network;
            // stashed and broadcast to the game thread further down this loop
            l->netplay_session->session_event_callback = [](void* user_ptr, int event) {
                ((FLibretroContext*)user_ptr)->netplay_session_event_pending = event;
            };

#if UNREALLIBRETRO_NETIMGUI
            { // Setup ImGui
                IMGUI_CHECKVERSION();
                ImGui::CreateContext(GlobalFontAtlas);
                ImPlot::CreateContext();
                ImGuiIO& io = ImGui::GetIO(); (void)io;
                io.IniFilename = NULL; // Don't write an ini file that caches window positions
                io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
                ImGui::StyleColorsDark();

                if (!NetImgui::Startup()) {
                    UE_LOG(Libretro, Error, TEXT("Failed to initialize NetImgui. NetImgui will not be available"));
                } else {
                    for (l->netimgui_port = 8889; l->netimgui_port < 65535; l->netimgui_port++) {
                        char TitleAnsi[256];
                        FCStringAnsi::Snprintf(TitleAnsi, sizeof(TitleAnsi), "%s (UnrealLibretro %s)",
                            TCHAR_TO_ANSI(*FPaths::GetCleanFilename(game.IsEmpty() ? core : game)), UnrealLibretroVersionAnsi);
                        NetImgui::ConnectFromApp(TitleAnsi, l->netimgui_port);
                        if (NetImgui::IsConnectionPending()) {
                            UE_LOG(Libretro, Log, TEXT("NetImgui " NETIMGUI_VERSION " is listening on port %i"), l->netimgui_port);
                            break;
                        }
                    }
                }
            }
#endif

            while (l->CoreState.load(std::memory_order_relaxed) != ECoreState::Shutdown)
            {
                DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Frame"), STAT_LibretroFrame, STATGROUP_UnrealLibretro);
                {
                    DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Work"), STAT_LibretroWork, STATGROUP_UnrealLibretro);

                    if (l->CoreState.load(std::memory_order_relaxed) == ECoreState::Running)
                    {
                        // In synchronous tick mode the core only advances inside a RunFrameSynchronously
                        // handshake so ticks are deterministic with respect to the game thread's frame.
                        // The short wait keeps request latency low while letting this loop keep pumping
                        // tasks, sam2 messages, and network service between requests
                        bool synchronous_tick = l->bSynchronousTickMode.load(std::memory_order_acquire);
                        bool tick_requested = !synchronous_tick || l->TickRequestedEvent->Wait(FTimespan::FromMilliseconds(1));

                        if (tick_requested)
                        {
#if UNREALLIBRETRO_NETIMGUI
                        NetImgui::NewFrame();
#endif
                        static_assert(sizeof(l->netplay_session->next_input_state) == sizeof(l->NextInputState),
                            "Unreal and ULNET input state layouts must match");
                        memcpy(l->netplay_session->next_input_state, l->NextInputState, sizeof(l->NextInputState));
                        l->netplay_session->retro_run = [](void* user_ptr) {
                            FLibretroContext* l = (FLibretroContext*)user_ptr;
                            if (l->core.gl.use_shared_context) {
                                verify(0 == l->SwitchOpenGLContext(UNREALLIBRETRO_SHARED_CONTEXT));
                            }
                            l->libretro_api.run();
                            if (l->core.gl.use_shared_context) {
                                verify(0 == l->SwitchOpenGLContext(UNREALLIBRETRO_FRONTEND_CONTEXT));
                            }
                        };
                        l->netplay_session->retro_serialize_size = [](void* user_ptr) {
                            FLibretroContext* l = (FLibretroContext*)user_ptr;
                            return l->libretro_api.serialize_size();
                        };
                        l->netplay_session->retro_serialize = [](void* user_ptr, void* data, size_t size) {
                            FLibretroContext* l = (FLibretroContext*)user_ptr;
                            return l->libretro_api.serialize(data, size);
                        };
                        l->netplay_session->retro_unserialize = [](void* user_ptr, const void* data, size_t size) {
                            FLibretroContext* l = (FLibretroContext*)user_ptr;
                            return l->libretro_api.unserialize(data, size);
                        };
                        // In synchronous mode don't sleep waiting on the network; the game thread is
                        // blocked on this pass. The core's native frame pacing still gates the tick,
                        // so a caller at a higher rate than the core just gets unticked passes
                        int SynchronousTickStatus = ulnet_poll_session(l->netplay_session, false, NULL, 0, l->core.av.timing.fps,
                            synchronous_tick ? 0.0 : 1.0);

                        if (synchronous_tick)
                        {
                            l->SynchronousTickStatus.store(SynchronousTickStatus, std::memory_order_release);
                            l->TickCompletedEvent->Trigger();
                        }
#if UNREALLIBRETRO_NETIMGUI
                        NetImgui::EndFrame();
#endif
                        }
                        else
                        {
                            // No tick was requested this iteration; keep the reliable/ACK machinery
                            // alive so netplay peers aren't stalled by a hitching game thread
                            ulnet_service_network(l->netplay_session, 0);
                        }

                        if (!l->connected_to_sam2 && l->sam_socket != SAM2_SOCKET_INVALID) {
                            l->connected_to_sam2 = static_cast<bool>(sam2_client_poll_connection(l->sam_socket, 0));
                        }

                        if (memcmp(&NetplayRoomOld, &l->netplay_session->room_we_are_in, sizeof(sam2_room_t)) != 0)
                        {
                            NetplayRoomOld = l->netplay_session->room_we_are_in;
                            FFunctionGraphTask::CreateAndDispatchWhenReady([NetplayRoomNew = l->netplay_session->room_we_are_in, WeakLibretroCoreInstance]
                            {
                                if (WeakLibretroCoreInstance.IsValid())
                                {
                                    ULibretroCoreInstance* LibretroCoreInstance = WeakLibretroCoreInstance.Get();

                                    LibretroCoreInstance->NetplayRoomName = FString{ UTF8_TO_TCHAR(NetplayRoomNew.name) };

                                    LibretroCoreInstance->NetplayRoomPeerIds.SetNumUninitialized(SAM2_TOTAL_PEERS);
                                    for (int i = 0; i < SAM2_TOTAL_PEERS; i++) {
                                        LibretroCoreInstance->NetplayRoomPeerIds[i] = NetplayRoomNew.peer_ids[i];
                                    }

                                    LibretroCoreInstance->OnNetplayRoomModified.Broadcast(LibretroCoreInstance->NetplayRoomName, LibretroCoreInstance->NetplayRoomPeerIds);
                                }

                            }, TStatId(), nullptr, ENamedThreads::GameThread);
                        }

                        // A lifecycle event (host lost, savestate timeout) was recorded by ulnet; relay it to Blueprint
                        if (int SessionEvent = l->netplay_session_event_pending) {
                            l->netplay_session_event_pending = 0;
                            if (l->PendingNetplayRole == (int)ELibretroNetplayRole::Join) {
                                // A declarative join is still retrying; the event already reset the session
                                // to solo, which is exactly the state the next attempt needs. Only the final
                                // deadline failure is surfaced
                                SAM2_LOG_WARN("Ignoring netplay session event %d while a declarative join is pending", SessionEvent);
                                SessionEvent = 0;
                            }
                            if (SessionEvent)
                            FFunctionGraphTask::CreateAndDispatchWhenReady([WeakLibretroCoreInstance, SessionEvent]
                                {
                                    if (WeakLibretroCoreInstance.IsValid())
                                    {
                                        WeakLibretroCoreInstance->OnNetplayError.Broadcast(
                                            SessionEvent == ULNET_SESSION_EVENT_AUTHORITY_LOST
                                                ? TEXT("Disconnected from the netplay host")
                                                : TEXT("Timed out waiting for the savestate from the netplay host"),
                                            SessionEvent);
                                    }
                                }, TStatId(), nullptr, ENamedThreads::GameThread);
                        }

                        {
                            int OurPort = sam2_get_port_of_peer(&l->netplay_session->room_we_are_in, l->netplay_session->our_peer_id);
                            int64_t DesyncedFrame = OurPort != -1 && l->netplay_session->peer[OurPort]
                                ? l->netplay_session->peer[OurPort]->desynced_frame : 0;
                            if (DesyncedFrame != 0 && DesyncedFrameOld == 0) {
                                FFunctionGraphTask::CreateAndDispatchWhenReady([WeakLibretroCoreInstance, DesyncedFrame]
                                    {
                                        if (WeakLibretroCoreInstance.IsValid())
                                        {
                                            WeakLibretroCoreInstance->OnNetplayDesync.Broadcast(DesyncedFrame);
                                        }
                                    }, TStatId(), nullptr, ENamedThreads::GameThread);
                            }
                            DesyncedFrameOld = DesyncedFrame;
                        }

                        // Run queued netplay requests even without sam2 so they can report why they can't proceed
                        TUniqueFunction<void(libretro_api_t&)> NetplayTask;
                        while (l->NetplayTasks.Dequeue(NetplayTask))
                        {
                            NetplayTask(l->libretro_api);
                        }

                        if (l->connected_to_sam2) {
                            for (int _prevent_infinite_loop_counter = 0; _prevent_infinite_loop_counter < 64; _prevent_infinite_loop_counter++) {

                                int status = sam2_client_poll(l->sam_socket, &l->latest_sam2_message);

                                if (status < 0) {
                                    SAM2_LOG_ERROR("Error polling sam2 server: %d", status);
                                    l->connected_to_sam2 = false;
                                    sam2_client_disconnect(l->sam_socket);
                                    l->sam_socket = SAM2_SOCKET_INVALID;
                                    break;
                                }
                                else if (status == 0) {
                                    break;
                                }
                                else {
                                    status = ulnet_process_message(
                                        l->netplay_session,
                                        (char *)&l->latest_sam2_message,
                                        0 // Messages from the coordinator are never sam2_join_header (peer-relayed only); sender is unused.
                                    );

                                    if (sam2_header_matches((const char*)&l->latest_sam2_message, sam2_fail_header)) {
                                        if (l->PendingNetplayRole == (int)ELibretroNetplayRole::Join) {
                                            // Transient while a declarative join is retrying (e.g. the host's room
                                            // doesn't exist yet because both instances launched the same frame).
                                            // Reset to solo so the next attempt can proceed; only the final
                                            // deadline failure is surfaced to the user
                                            SAM2_LOG_WARN("Ignoring sam2 error while a declarative netplay join is pending: %s",
                                                l->latest_sam2_message.error_message.description);
                                            ulnet_session_tear_down(l->netplay_session);
                                            ulnet_session_init_defaulted(l->netplay_session);
                                        } else {
                                            FFunctionGraphTask::CreateAndDispatchWhenReady([WeakLibretroCoreInstance, ErrorMessage = l->latest_sam2_message.error_message]
                                                {
                                                    if (WeakLibretroCoreInstance.IsValid())
                                                    {
                                                        WeakLibretroCoreInstance->OnNetplayError.Broadcast(FString(ErrorMessage.description), ErrorMessage.code);
                                                    }
                                                }, TStatId(), nullptr, ENamedThreads::GameThread);
                                        }
                                    }
                                }
                            }
                        }

                        // Declarative netplay (NetplayRole/NetplayPeerId properties): consumed once the
                        // sam2 connection is up. Host is fire-and-forget; Join retries until the deadline
                        // so two instances launched the same frame order themselves without user timers
                        if (l->PendingNetplayRole != 0 && l->connected_to_sam2)
                        {
                            int64 now_usec = ulnet__get_unix_time_microseconds();
                            if (l->PendingNetplayDeadlineUnixUsec == 0)
                            {
                                l->PendingNetplayDeadlineUnixUsec = now_usec + 15 * 1000000LL;
                            }

                            if (l->PendingNetplayRole == (int)ELibretroNetplayRole::Host)
                            {
                                l->NetplayHost_CoreThread((uint16)l->PendingNetplayPeerId);
                                l->PendingNetplayRole = 0;
                            }
                            else // Join
                            {
                                bool joined = l->netplay_session->room_we_are_in.flags & SAM2_FLAG_ROOM_IS_NETWORK_HOSTED;
                                bool attempt_in_flight = l->netplay_session->frame_counter == ULNET_WAITING_FOR_SAVE_STATE_SENTINEL;
                                if (joined)
                                {
                                    l->PendingNetplayRole = 0;
                                }
                                else if (!attempt_in_flight)
                                {
                                    if (now_usec > l->PendingNetplayDeadlineUnixUsec)
                                    {
                                        l->PendingNetplayRole = 0;
                                        FFunctionGraphTask::CreateAndDispatchWhenReady([WeakLibretroCoreInstance, PeerId = l->PendingNetplayPeerId]
                                            {
                                                if (WeakLibretroCoreInstance.IsValid())
                                                {
                                                    WeakLibretroCoreInstance->OnNetplayError.Broadcast(
                                                        FString::Printf(TEXT("Timed out looking for netplay host %05d"), PeerId),
                                                        SAM2_RESPONSE_PEER_DOES_NOT_EXIST);
                                                }
                                            }, TStatId(), nullptr, ENamedThreads::GameThread);
                                    }
                                    else if (now_usec >= l->PendingNetplayNextAttemptUnixUsec)
                                    {
                                        l->NetplaySync_CoreThread((uint16)l->PendingNetplayPeerId);
                                        l->PendingNetplayNextAttemptUnixUsec = now_usec + 2 * 1000000LL;
                                    }
                                }
                            }
                        }
                    }

                    if (l->ErrorMessage.Len() > 0)
                    {
                        goto cleanup;
                    }

                    // Execute tasks from command queue  Note: It's semantically significant that this is here. Since I hook in save state
                    //                                         operations here it must necessarily come after run is called on the core
                    TUniqueFunction<void(libretro_api_t&)> Task;
                    while (l->LibretroAPITasks.Dequeue(Task))
                    {
                        Task(l->libretro_api);
                    }
                }
            }

cleanup:
            sam2_client_disconnect(l->sam_socket);
            // These ImGui routines all cleanup thread_local objects if they aren't NULL
#if UNREALLIBRETRO_NETIMGUI
            NetImgui::Shutdown();
            ImPlot::DestroyContext();
            ImGui::DestroyContext();
#endif

           // @todo Make state transitions better so StartFailed can't be overwritten
            // Note when a core dies mid-run (ErrorMessage set after a successful launch) this
            // deliberately re-invokes LoadedCallback as a failure: that's what resets the game
            // thread's CoreInstance handle so it doesn't dangle, at the cost of a second
            // OnLaunchComplete(false) broadcast
            if (   l->ErrorMessage.Len() > 0
                || l->CoreState.load(std::memory_order_relaxed) == ECoreState::StartFailed)
            {
                l->CoreState.store(ECoreState::StartFailed, std::memory_order_release);
                LoadedCallback(l, l->libretro_api, l->ErrorMessage);
            }

            // Check for unexecuted tasks and warn if any exist
            int32 UnexecutedTaskCount = 0;
            TUniqueFunction<void(libretro_api_t&)> Task;
            while (l->LibretroAPITasks.Dequeue(Task))
            {
                UnexecutedTaskCount++;
            }

            if (UnexecutedTaskCount > 0)
            {
                UE_LOG(Libretro, Warning, TEXT("There were %d unexecuted tasks during cleanup. This could lead to resource leaks."), UnexecutedTaskCount);
            }

            if (l->core.gl.use_shared_context)
            {
                // We need to switch back to the shared context we unload the game and deinit the core
                verify(0 == l->SwitchOpenGLContext(UNREALLIBRETRO_SHARED_CONTEXT));
            }

            if (l->libretro_api.game_loaded)
            {
                l->libretro_api.unload_game();
            }

            if (l->libretro_api.initialized)
            {
                l->libretro_api.deinit();
            }

            if (l->core.gl.use_shared_context)
            {
                // The context here doesn't have shared ownership with Unreal's render thread so we can delete it immediately
#if PLATFORM_WINDOWS
                verify(wglMakeCurrent(l->core.gl.shared_hdc, NULL));
                verify(wglDeleteContext(l->core.gl.shared_context));
                verify(ReleaseDC(l->core.gl.shared_window, l->core.gl.shared_hdc));
                verify(DestroyWindow(l->core.gl.shared_window));
#elif PLATFORM_ANDROID
                if (l->core.gl.shared_context) {
                    if (!eglDestroyContext(l->core.gl.egl_display, l->core.gl.shared_context)) {
                        UE_LOG(Libretro, Error, TEXT("eglDestroyContext() returned error %d"), eglGetError());
                    }
                }
#endif
                verify(0 == l->SwitchOpenGLContext(UNREALLIBRETRO_FRONTEND_CONTEXT));
            }

            if (l->libretro_api.handle)
            {
                FPlatformProcess::FreeDllHandle(l->libretro_api.handle);
            }

            if (l->netplay_session) {
                ulnet_session_tear_down(l->netplay_session);
                ULNET_FREE(l->netplay_session);
                l->netplay_session = NULL;
            }

            IPlatformFile::GetPlatformPhysical().DeleteFile(*InstancedCorePath);

            l->Unreal.AudioQueue.Reset();

            FFunctionGraphTask::CreateAndDispatchWhenReady([=]
            {
                AllocatedInstances[InstanceNumber] = false;
            }, TStatId(), nullptr, ENamedThreads::GameThread);
            
            {
#if PLATFORM_WINDOWS
                // On windows the thread that created a window MUST also destroy it https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-destroywindow#:~:text=A%20thread%20cannot%20use%20DestroyWindow%20to%20destroy%20a%20window%20created%20by%20a%20different%20thread
                if (l->core.gl.window)
                {
                    wglMakeCurrent(l->core.gl.hdc, NULL);
                    verify(ReleaseDC(l->core.gl.window, l->core.gl.hdc));
                    verify(DestroyWindow(l->core.gl.window));
                }
#endif
                auto FreeResourcesSharedWithRenderThread = [l]()
                            {
                                for (void* buffer : l->Unreal.FrameUpload.Buffers)
                                {
                                    if (buffer)
                                    {
                                        FMemory::Free(buffer);
                                    }
                                }

                                if (l->Unreal.FrameUpload.BufferFreedEvent)
                                {
                                    FPlatformProcess::ReturnSynchEventToPool(l->Unreal.FrameUpload.BufferFreedEvent);
                                }
#if PLATFORM_WINDOWS
                                if (l->core.gl.context)
                                {
                                    wglDeleteContext(l->core.gl.context); /** implicitly releases resources like fbos, pbos, and textures */
                                }
#elif PLATFORM_ANDROID
                                if (l->core.gl.egl_context) 
                                {
                                    if (!eglDestroyContext(l->core.gl.egl_display, l->core.gl.egl_context))
                                    {
                                        UE_LOG(Libretro, Error, TEXT("eglDestroyContext() for main context returned error %d"), eglGetError());
                                    }
                                }

                                if (l->core.gl.egl_display)
                                {
                                    if (!eglTerminate(l->core.gl.egl_display))
                                    {
                                        UE_LOG(Libretro, Error, TEXT("eglTerminate() returned error %d"), eglGetError());
                                    }
                                }
#endif

                                l->Unreal.TextureRHI.SafeRelease();
                                l->Unreal.SourceRGB565TextureRHI.SafeRelease();
#if PLATFORM_WINDOWS
                                if (l->core.gl.d3d12_interop_fence)
                                {
                                    // Any queue waits on this fence were enqueued before this cleanup command
                                    // (render commands are FIFO) and their matching GL signals were flushed,
                                    // so the queue can't be left waiting on a released fence
                                    l->core.gl.d3d12_interop_fence->Release();
                                    l->core.gl.d3d12_interop_fence = nullptr;
                                }
#endif
                            
                                if (l->LambdaRunnable)
                                {
                                    delete l->LambdaRunnable; /** This will block the render thread if for some reason the thread we were running on hasn't exited yet */
                                }

                                // The libretro thread has exited and Shutdown is initiated from the game
                                // thread, so no one can be waiting on these anymore
                                FPlatformProcess::ReturnSynchEventToPool(l->TickRequestedEvent);
                                FPlatformProcess::ReturnSynchEventToPool(l->TickCompletedEvent);

                                delete l; /** Task queue released */
                            };

                if (!FApp::CanEverRender())
                {
                    // Headless (dedicated server/commandlet/nullrhi): there is no render or RHI thread to
                    // defer to, and the render-command pipe would run this inline on the libretro thread
                    // itself, which must not delete its own runnable. The GL/RHI members are all null in
                    // this mode (video init was skipped) so only the CPU-side frees actually do anything
                    FFunctionGraphTask::CreateAndDispatchWhenReady([FreeResourcesSharedWithRenderThread]
                        {
                            FreeResourcesSharedWithRenderThread();
                        }, TStatId(), nullptr, ENamedThreads::GameThread);
                }
                else
                {
                    // The double nested command enqueue is based on boilerplate I found elsewhere in the engine
                    // Since render commands are executed fifo we only delete shared resources after the render thread is done with them
                    // The actual render command execution is done on the RHI thread so we have to synchronize there as well
                    ENQUEUE_RENDER_COMMAND(LibretroCleanupResourcesSharedWithRenderThread)([FreeResourcesSharedWithRenderThread](FRHICommandListImmediate& RHICmdList)
                        {
                            RHICmdList.EnqueueLambda([FreeResourcesSharedWithRenderThread](FRHICommandList&)
                                {
                                    FreeResourcesSharedWithRenderThread();
                                });
                        }
                    );
                }
            }
        }
    );

    return l;
}

void FLibretroContext::NetplayHost_CoreThread(uint16 PeerId)
{
    sam2_room_message_t HostRoomRequest = { SAM2_MAKE_HEADER };
    FCStringAnsi::Strncpy(HostRoomRequest.room.name, netplay_room_name, SAM2_ARRAY_LENGTH(HostRoomRequest.room.name));
    HostRoomRequest.room.flags |= SAM2_FLAG_ROOM_IS_NETWORK_HOSTED;
    HostRoomRequest.room.peer_topology |= (1ULL << SAM2_AUTHORITY_INDEX);
    HostRoomRequest.room.rom_hash = (uint32)rom_hash;
    sam2_format_core_version(&HostRoomRequest.room, system.library_name, system.library_version);

    if (netplay_session->room_we_are_in.flags & SAM2_FLAG_ROOM_IS_NETWORK_HOSTED)
    {
        if (ulnet_is_spectator(netplay_session, netplay_session->our_peer_id))
        {
            ulnet_session_tear_down(netplay_session);
        }
        else if (ulnet_is_authority(netplay_session))
        {
            SAM2_LOG_WARN("We're already hosting a room, we can't host a new room");
            return;
        }
        else
        {
            SAM2_LOG_ERROR("We're connected peer to peer we have to exit gracefully before we can host a new room");
            // @todo Disconnect gracefully
            return;
        }
    }

    if (PeerId)
    {
        // We want to change our peer id
        sam2_connect_message_t ChangePeerIdRequest = { SAM2_CONN_HEADER };
        ChangePeerIdRequest.peer_id = PeerId;

        if (int ErrorCode = sam2_client_send(sam_socket, (char*)&ChangePeerIdRequest))
        {
            SAM2_LOG_ERROR("Failed to send message with header '%.8s' (error=%d)", (char*)&ChangePeerIdRequest, ErrorCode);
        }
    }

    if (int ErrorCode = sam2_client_send(sam_socket, (char*)&HostRoomRequest))
    {
        SAM2_LOG_ERROR("Failed to send message with header '%.8s' (error=%d)", (char*)&HostRoomRequest, ErrorCode);
    }
}

void FLibretroContext::NetplaySync_CoreThread(uint16 PeerId)
{
    if (netplay_session->room_we_are_in.flags & SAM2_FLAG_ROOM_IS_NETWORK_HOSTED)
    {
        SAM2_LOG_INFO("Leaving the current room and connecting to peer %05d", PeerId);
        ulnet_session_tear_down(netplay_session);
    }

    // Directly signaling the authority just means spectate
    ulnet_session_init_defaulted(netplay_session);
    netplay_session->room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX] = PeerId;
    netplay_session->frame_counter = ULNET_WAITING_FOR_SAVE_STATE_SENTINEL;
    ulnet_startup_nat_for_peer(netplay_session, PeerId, SAM2_AUTHORITY_INDEX, NULL);
}

void FLibretroContext::Shutdown(FLibretroContext* Instance)
{
    // We enqueue the shutdown procedure as the final task since we want outstanding tasks to be executed first
    Instance->EnqueueTask([Instance](auto&&)
        {
            Instance->CoreState.store(ECoreState::Shutdown, std::memory_order_relaxed);
        });
}

void FLibretroContext::SetSynchronousTickMode(bool bEnabled)
{
    bSynchronousTickMode.store(bEnabled, std::memory_order_release);
}

int FLibretroContext::RunFrameSynchronously(uint32 TimeoutMilliseconds)
{
    if (CoreState.load(std::memory_order_acquire) != ECoreState::Running) return 0;
    if (!bSynchronousTickMode.load(std::memory_order_acquire)) return 0;

    SynchronousTickStatus.store(0, std::memory_order_release);
    TickRequestedEvent->Trigger();

    if (!TickCompletedEvent->Wait(TimeoutMilliseconds))
    {
        // The libretro thread is stalled or transitioning out of Running. The completion for this
        // request may fire late and be consumed by the next call, which then returns one pass early;
        // that's benign since these passes are idempotent from the caller's perspective
        UE_LOG(Libretro, Warning, TEXT("RunFrameSynchronously timed out after %ums"), TimeoutMilliseconds);
        return 0;
    }

    return SynchronousTickStatus.load(std::memory_order_acquire);
}

void FLibretroContext::Pause(bool ShouldPause)
{
    // We enqueue the state change because otherwise we might prematurely unset the Starting state
    // Alternatively you could add one more state to accomplish this, but this is fine for now
    EnqueueTask([this, ShouldPause](auto&&)
        {
            ECoreState State = CoreState.load(std::memory_order_relaxed);
            if (   State != ECoreState::Shutdown
                && State != ECoreState::StartFailed)
            {
                CoreState.store(ShouldPause ? ECoreState::Paused : ECoreState::Running, std::memory_order_relaxed);
            }
        });
    
}

void FLibretroContext::EnqueueTask(TUniqueFunction<void(libretro_api_t&)> LibretroAPITask)
{
    check(IsInGameThread()); // LibretroAPITasks is a single producer single consumer queue
    LibretroAPITasks.Enqueue(MoveTemp(LibretroAPITask));
};
