#include "LibretroContext.h"
extern "C"
{
#include "gfx/scaler/pixconv.h"
}

#include "Runtime/Launch/Resources/Version.h"

#include "Misc/FileHelper.h"

#include "LibretroCoreInstance.h"
#include "UnrealLibretro.h" // For Libretro debug log category
#include "LibretroSettings.h"
#include "LibretroInputDefinitions.h"
#include "LambdaRunnable.h"

#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "TextureResource.h"
#include "RenderingThread.h"
#include "Runtime/Launch/Resources/Version.h"

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
    this->gl_win32_interop_supported_by_driver = false; // bFoundAllEntryPoints; Not ready

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

    uint64_t SizeInBytes, MipLevels;
    GLenum handleType;
    FTaskGraphInterface::Get().WaitUntilTaskCompletes(
        FFunctionGraphTask::CreateAndDispatchWhenReady([&]
            {
                const unsigned CapacityMilliseconds = 50;
                const unsigned CapacityFrames = CapacityMilliseconds * (core.av.timing.sample_rate / 1000.0);
                Unreal.AudioQueue = MakeShared<TCircularQueue<int32>, ESPMode::ThreadSafe>(CapacityFrames); // @todo move to audio init when the hack below is removed

                // Make sure the game objects haven't been GCed
                if (!UnrealSoundBuffer.IsValid() || !UnrealRenderTarget.IsValid())
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
    } else {
        for (auto i : { 0, 1 }) {
            core.software.bgra_buffers[i] = FMemory::Malloc(4 * core.av.geometry.max_width
                                                              * core.av.geometry.max_height, PLATFORM_CACHE_LINE_SIZE);
        }
    }

    core.hw.context_reset();
    return 0;
}

#include "Async/TaskGraphInterfaces.h"
// Stripped down code for profiling purposes https://godbolt.org/z/c57esx
 void FLibretroContext::core_video_refresh(const void *data, unsigned width, unsigned height, unsigned pitch) {
    DECLARE_SCOPE_CYCLE_COUNTER(TEXT("PrepareFrameBufferForRenderThread"), STAT_LibretroPrepareFrameBufferForRenderThread, STATGROUP_UnrealLibretro);

    unsigned SrcPitch = 4 * core.av.geometry.max_width;

    auto prepare_frame_for_upload_to_unreal_RHI = [&](void* const buffer)
    {
        void* old_buffer;
        {
            FScopeLock SwapPointer(&this->Unreal.FrameUpload.CriticalSection);
            old_buffer = this->Unreal.FrameUpload.ClientBuffer;
            this->Unreal.FrameUpload.ClientBuffer = buffer;
        }

        if (!old_buffer)
        {
            ENQUEUE_RENDER_COMMAND(CopyToUnrealFramebufferTask)( // @todo this triggers an assert on MacOS you can get around it by enqueuing through the TaskGraph instead no idea why this is the case
                [this,
                MipIndex = 0,
                SrcPitch,
                Region = FUpdateTextureRegion2D(0, 0, 0, 0, width, height)]
            (FRHICommandListImmediate& RHICmdList)
            {
                if (!this->Unreal.TextureRHI.GetReference()) {
                    ErrorMessage = TEXT("Texture RHI reference is null");
                    UE_LOG(Libretro, Error, TEXT("%s"), *ErrorMessage);
                    return;
                }

                RHICmdList.EnqueueLambda([=, this](FRHICommandList& RHICmdList)
                    {
                        // Potentially this should be a TryLock() so you don't preempt the render thread although it's unlikely that would happen
                        FScopeLock UploadTextureToRenderHardware(&this->Unreal.FrameUpload.CriticalSection);
                        GDynamicRHI->RHIUpdateTexture2D(
#if    ENGINE_MAJOR_VERSION == 5 \
    && ENGINE_MINOR_VERSION >= 2
                            RHICmdList,
#endif
                            this->Unreal.TextureRHI.GetReference(),
                            MipIndex,
                            Region,
                            SrcPitch,
                            (uint8*)this->Unreal.FrameUpload.ClientBuffer);
                        this->Unreal.FrameUpload.ClientBuffer = nullptr;
                    }
                );
            }
            );
        }
    };

    if (data && data != RETRO_HW_FRAME_BUFFER_VALID) {
        DECLARE_SCOPE_CYCLE_COUNTER(TEXT("CPUConvertAndCopyFramebuffer"), STAT_LibretroCPUConvertAndCopyFramebuffer, STATGROUP_UnrealLibretro);

        auto bgra_buffer = core.software.bgra_buffers[core.free_framebuffer_index = !core.free_framebuffer_index];

        if (core.gl.pixel_format == GL_BGRA) {
        switch (core.gl.pixel_type) {
            case GL_UNSIGNED_SHORT_5_6_5: {
                conv_rgb565_argb8888(bgra_buffer, data,
                    width, height,
                    SrcPitch, pitch);
            }
            break;
            case GL_UNSIGNED_SHORT_5_5_5_1: {
                checkNoEntry();
            }
            break;
            case GL_UNSIGNED_BYTE: {
                conv_copy(bgra_buffer, data,
                    width, height,
                    SrcPitch, pitch);
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
                    SrcPitch, pitch);
            }
            break;
            case GL_UNSIGNED_BYTE: {
                conv_argb8888_abgr8888(bgra_buffer, data,
                    width, height,
                    SrcPitch, pitch);
            }
            break;
            default:
                checkNoEntry();
            }
        }

        prepare_frame_for_upload_to_unreal_RHI(bgra_buffer);
    }
    else if (data == RETRO_HW_FRAME_BUFFER_VALID) {
        check(core.using_opengl && core.gl.pixel_type == GL_UNSIGNED_BYTE);

        if (core.gl.rhi_interop_memory) {
            // @todo I make no attempt to synchronize the core's drawing operations with RHI reads, some cores will work but others have synchronization issues
            //       It seems like a good reference resource on how to do this is either ITextureShareItem Engine/Source/Programs/TextureShare/TextureShareSDK
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
                        prepare_frame_for_upload_to_unreal_RHI(frame_buffer);
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
                            LogGLErrors(glReadPixels(0, 0,
                                core.av.geometry.max_width, // @enhancement Only copy the portion of the buffer we need rather than the max possible potential size
                                core.av.geometry.max_height,
                                core.gl.pixel_format,
                                core.gl.pixel_type,
                                offset_into_pbo_where_data_is_written));
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
        check(hw->context_type < RETRO_HW_CONTEXT_VULKAN);
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
    libretro_callbacks->input_poll = [=]() { };
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

    if (filename && !system.need_fullpath) {
        if (!FFileHelper::LoadFileToArray(gameBinary, UTF8_TO_TCHAR(filename))) {
            ErrorMessage = FString::Printf(TEXT("Failed to load game file: %s"), UTF8_TO_TCHAR(filename));
            UE_LOG(Libretro, Warning, TEXT("%s"), *ErrorMessage);
            return 1;
        }

        info.data = gameBinary.GetData();
        info.size = gameBinary.Num();
    }

    if (!libretro_api.load_game(&info)) {
        ErrorMessage = TEXT("The core failed to load the content.");
        UE_LOG(Libretro, Warning, TEXT("%s"), *ErrorMessage);
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

    return video_configure(&core.av.geometry);
}

FLibretroContext* FLibretroContext::Launch(ULibretroCoreInstance* LibretroCoreInstance, FString core, FString game, UTextureRenderTarget2D* RenderTarget, URawAudioSoundWave* SoundBuffer, TUniqueFunction<void(FLibretroContext*, libretro_api_t&, const FString&)> LoadedCallback)
{

    check(IsInGameThread()); // So static initialization is safe + UObject access

    static constexpr uint32 max_instances = sizeof(libretro_callbacks_table) / sizeof(libretro_callbacks_table[0]);
    static TBitArray<TInlineAllocator<(max_instances / 8) + 1>> AllocatedInstances(false, max_instances);

    FLibretroContext *l = new FLibretroContext();

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

    auto LibretroSettings = GetDefault<ULibretroSettings>();

    ConvertPath(l->core.save_directory,   LibretroSettings->CoreSaveDirectory);
    ConvertPath(l->core.system_directory, LibretroSettings->CoreSystemDirectory);

    l->StartingOptions = LibretroSettings->GlobalCoreOptions;
    l->StartingOptions.Append(LibretroCoreInstance->EditorPresetOptions); // Potentially overrides global options

    l->UnrealRenderTarget = MakeWeakObjectPtr(RenderTarget);
    l->UnrealSoundBuffer  = MakeWeakObjectPtr(SoundBuffer);

    // Kick the initialization process off to another thread. It shouldn't be added to the Unreal task pool because those are too slow and my code relies on OpenGL state being thread local.
    // The Runnable system is the standard way for spawning and managing threads in Unreal. FThread looks enticing, but they removed any way to detach threads since "it doesn't work as expected"
    l->LambdaRunnable = FLambdaRunnable::RunLambdaOnBackGroundThread(FPaths::GetCleanFilename(core) + FPaths::GetCleanFilename(game),
        [=, LoadedCallback = MoveTemp(LoadedCallback), EditorPresetControllers = LibretroCoreInstance->EditorPresetControllers]() {
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

            uint64 frames = 0;
            auto   start = FDateTime::Now();

            if (!IPlatformFile::GetPlatformPhysical().CopyFile(*InstancedCorePath, *core)) {
                l->ErrorMessage = FString::Printf(TEXT("Failed to copy core file from %s to %s"), *core, *InstancedCorePath);
                UE_LOG(Libretro, Warning, TEXT("%s"), *l->ErrorMessage);
                l->CoreState.store(ECoreState::StartFailed, std::memory_order_release);
                goto cleanup;
            }

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

            while (l->CoreState.load(std::memory_order_relaxed) != ECoreState::Shutdown)
            {
                DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Frame"), STAT_LibretroFrame, STATGROUP_UnrealLibretro);
                {
                    DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Work"), STAT_LibretroWork, STATGROUP_UnrealLibretro);

                    if (l->CoreState.load(std::memory_order_relaxed) == ECoreState::Running)
                    {
                        l->libretro_api.run();
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

                { // @todo My timing solution is a bit adhoc. I'm sure theres probably a better way.
                    DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Sleep"), STAT_LibretroSleep, STATGROUP_UnrealLibretro);

                    frames++;

                    double sleep = (frames / l->core.av.timing.fps) - (FDateTime::Now() - start).GetTotalSeconds();
                    if (sleep > 0.0) {
                        FPlatformProcess::Sleep(sleep); // This always yields so only call it when we actually need to sleep
                    } else if (sleep < -(1 / l->core.av.timing.fps)) { // If over a frame behind don't try to catch up to the next frame
                        start = FDateTime::Now();
                        frames = 0;
                    }
                }
            }

cleanup:    // @todo Make state transitions better so StartFailed can't be overwritten
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

            if (l->libretro_api.initialized)
            {
                l->libretro_api.deinit();
            }

            if (l->libretro_api.handle)
            {
                FPlatformProcess::FreeDllHandle(l->libretro_api.handle);
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
                // The double nested command enqueue is based on boilerplate I found elsewhere in the engine
                // Since render commands are executed fifo we only delete shared resources after the render thread is done with them
                // The actual render command execution is done on the RHI thread so we have to synchronize there as well
                ENQUEUE_RENDER_COMMAND(LibretroCleanupResourcesSharedWithRenderThread)([l](FRHICommandListImmediate& RHICmdList)
                    {
                        RHICmdList.EnqueueLambda([l](FRHICommandList&)
                            {
                                for (int i : {0, 1})
                                {
                                    if (l->core.software.bgra_buffers[i])
                                    {
                                        FMemory::Free(l->core.software.bgra_buffers[i]);
                                    }
                                }
#if PLATFORM_WINDOWS
                                if (l->core.gl.context)
                                {
                                    wglDeleteContext(l->core.gl.context); /** implicitly releases resources like fbos, pbos, and textures */
                                }
#elif PLATFORM_ANDROID
                                if (l->core.gl.egl_context) 
                                {
                                    verify(eglDestroyContext(l->core.gl.egl_display, 
                                                             l->core.gl.egl_context));
                                }
#endif

                                l->Unreal.TextureRHI.SafeRelease();
                            
                                if (l->LambdaRunnable)
                                {
                                    delete l->LambdaRunnable; /** This will block the render thread if for some reason the thread we were running on hasn't exited yet */
                                }
                            
                                delete l; /** Task queue released */
                            });
                    }
                );
            }
        }
    );

    return l;
}

void FLibretroContext::Shutdown(FLibretroContext* Instance) 
{
    // We enqueue the shutdown procedure as the final task since we want outstanding tasks to be executed first
    Instance->EnqueueTask([Instance](auto&&)
        {
            Instance->CoreState.store(ECoreState::Shutdown, std::memory_order_relaxed);
        });
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
