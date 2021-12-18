
#include "sdlarch.h"
extern "C"
{
#include "gfx/scaler/pixconv.h"
}

#include "UnrealLibretro.h" // For Libretro debug log category
#include "LibretroSettings.h"
#include "LibretroInputDefinitions.h"
#include "LambdaRunnable.h"

#include "HAL/FileManager.h"

#if PLATFORM_APPLE
#include <dispatch/dispatch.h>
#endif

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/PreWindowsApi.h"
#include <windows.h>
#include <d3d12.h>
#include "Windows/PostWindowsApi.h"
#include "Windows/HideWindowsPlatformTypes.h"
#endif

#define DEBUG_OPENGL 0

// MY EYEEEEESSS.... Even though this looks heavily obfuscated what this actually accomplishes is relatively simple. It allows us to run multiple libretro cores at once. 
// We have to do it this way because when libretro calls a callback we implemented there really isn't any suitable way to tell which core the call came from.
// So we just statically generate a bunch of callback functions with macros and write their function pointers into an array of libretro_callbacks_t's and issue them at runtime.
// These generated callbacks call std::functions which can capture arguments. So we capture this and now it calls our callbacks on a per instance basis.
#define REP10(P, M)  M(P##0) M(P##1) M(P##2) M(P##3) M(P##4) M(P##5) M(P##6) M(P##7) M(P##8) M(P##9)
#define REP100(M) REP10(,M) REP10(1,M) REP10(2,M) REP10(3,M) REP10(4,M) REP10(5,M) REP10(6,M) REP10(7,M) REP10(8,M) REP10(9,M)

struct libretro_callbacks_t {
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

#define load_sym(V, S) do {\
    if (0 == ((*(void**)&V) = FPlatformProcess::GetDllExport(libretro_api.handle, TEXT(#S)))) \
        UE_LOG(Libretro, Fatal, TEXT("Failed to load symbol '" #S "'': %u"), FPlatformMisc::GetLastError()); \
	} while (0)
#define load_retro_sym(S) load_sym(libretro_api.S, retro_##S)


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

 void LibretroContext::create_window() {
    SDL_GL_ResetAttributes(); // SDL state isn't thread local unlike OpenGL. So Libretro Cores could potentially interfere with eachother's Attributes since you're setting globals.

    if (core.hw.context_type == RETRO_HW_CONTEXT_OPENGL_CORE || core.hw.version_major >= 3) {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, core.hw.version_major); 
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, core.hw.version_minor);
    }

    switch (core.hw.context_type) {
    case RETRO_HW_CONTEXT_OPENGL_CORE:
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        break;
    case RETRO_HW_CONTEXT_OPENGLES2:
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
        break;
    case RETRO_HW_CONTEXT_OPENGL:
        if (core.hw.version_major >= 3)
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
        break;
    default:
        UE_LOG(Libretro, Fatal, TEXT("Unsupported hw context %i. (only OPENGL, OPENGL_CORE and OPENGLES2 supported)"), core.hw.context_type);
    }

    // Might be able to use this instead SWindow::GetNativeWindow()
    core.gl.window = SDL_CreateWindow("sdlarch", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 0, 0, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN); // @todo This is fine on windows, but creating a window from a background thread will crash on some versions Linux if you don't enable a special flag and everytime on MacOS

	if (!core.gl.window)
        UE_LOG(Libretro, Fatal, TEXT("Failed to create window: %s"), SDL_GetError());

#if DEBUG_OPENGL
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
#endif
    core.gl.context = SDL_GL_CreateContext(core.gl.window);


    if (!core.gl.context)
        UE_LOG(Libretro, Fatal, TEXT("Failed to create OpenGL context: %s"), SDL_GetError());

    #pragma warning(push)
    #pragma warning(disable:4191)

    // Initialize all entry points.
    #define GET_GL_PROCEDURES(Type,Func) Func = (Type)SDL_GL_GetProcAddress(#Func);
    ENUM_GL_PROCEDURES(GET_GL_PROCEDURES);
    ENUM_GL_WIN32_INTEROP_PROCEDURES(GET_GL_PROCEDURES);

    // Check that all of the entry points have been initialized.
    bool bFoundAllEntryPoints = true;
    #define CHECK_GL_PROCEDURES(Type,Func) if (Func == NULL) { bFoundAllEntryPoints = false; UE_LOG(Libretro, Warning, TEXT("Failed to find entry point for %s"), TEXT(#Func)); }
    ENUM_GL_PROCEDURES(CHECK_GL_PROCEDURES);
    checkf(bFoundAllEntryPoints, TEXT("Failed to find all OpenGL entry points."));

    ENUM_GL_WIN32_INTEROP_PROCEDURES(CHECK_GL_PROCEDURES);
    this->gl_win32_interop_supported_by_driver = false; // bFoundAllEntryPoints; Not ready
    
    // Restore warning C4191.
    #pragma warning(pop)

#if DEBUG_OPENGL
    GLint opengl_flags, 
          major_version, minor_version; 
    glGetIntegerv(GL_CONTEXT_FLAGS, &opengl_flags);
    glGetIntegerv(GL_MAJOR_VERSION, &major_version);
    glGetIntegerv(GL_MINOR_VERSION, &minor_version);
    if (   major_version >= 4 && minor_version >= 3 
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
}

 void LibretroContext::video_configure(const struct retro_game_geometry *geom) {
	if (!core.gl.pixel_format) {
        auto data = RETRO_PIXEL_FORMAT_0RGB1555;
        this->core_environment(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &data);
	}

    core.gl.pitch = geom->max_width * core.gl.bits_per_pixel;
	
    // Unreal Resource init
#if PLATFORM_WINDOWS
    HANDLE SharedHandle = nullptr;
#endif
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
                                FRHIResourceCreateInfo Info{ TEXT("Dummy Texture for now") };
                                this->Unreal.TextureRHI = RHICreateTexture2D(core.av.geometry.max_width,
                                                                             core.av.geometry.max_height,
                                                                             PF_R8G8B8A8,
                                                                             1,
                                                                             1,
                                                                             TexCreate_CPUWritable | TexCreate_Dynamic,
                                                                             Info);
                            });
                }
                else
                {
                    // Video Init
                    UnrealRenderTarget->bGPUSharedFlag = true; // Allows us to share this rendertarget with other applications and APIs in this case OpenGL
                    UnrealRenderTarget->InitCustomFormat(core.av.geometry.max_width,
                                                         core.av.geometry.max_height,
                                                         PF_R8G8B8A8,
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
                                        check(!FAILED(UE4D3DDevice->CreateSharedHandle(ResolvedTexture, NULL, GENERIC_ALL, *FString::Printf(TEXT("OpenGLSharedFramebuffer_UnrealLibretro_%u"), NamingIdx.Increment()), &SharedHandle)));
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
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, geom->max_width, 
                                                    geom->max_height,
                                                    0,
                                                    core.gl.pixel_format, 
                                                    core.gl.pixel_type,
                                                    NULL);
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

        SDL_assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

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
                                                              * core.av.geometry.max_height);
        }
    }
	
    core.hw.context_reset();
}

// Stripped down code for profiling purposes https://godbolt.org/z/c57esx
 void LibretroContext::core_video_refresh(const void *data, unsigned width, unsigned height, unsigned pitch) {
    DECLARE_SCOPE_CYCLE_COUNTER(TEXT("PrepareFrameBufferForRenderThread"), STAT_LibretroPrepareFrameBufferForRenderThread, STATGROUP_UnrealLibretro);

    unsigned SrcPitch = 4 * width;
	
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
                check(this->Unreal.TextureRHI.GetReference());

                RHICmdList.EnqueueLambda([=](FRHICommandList&)
                    {
                        FScopeLock UploadTextureToRenderHardware(&this->Unreal.FrameUpload.CriticalSection);
                        GDynamicRHI->RHIUpdateTexture2D(
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

        switch (core.gl.pixel_type) {
            case GL_UNSIGNED_SHORT_5_6_5: {
                conv_rgb565_abgr8888(bgra_buffer, data,
                    width, height,
                    SrcPitch, pitch);
            }
            break;
            case GL_UNSIGNED_SHORT_5_5_5_1: {
                checkNoEntry();
            }
            break;
            case GL_UNSIGNED_INT_8_8_8_8_REV: {
                conv_argb8888_abgr8888(bgra_buffer, data,
                    width, height,
                    SrcPitch, pitch);
            }
            break;
            default:
                checkNoEntry();
        }

        prepare_frame_for_upload_to_unreal_RHI(bgra_buffer);
    }
    else if (data == RETRO_HW_FRAME_BUFFER_VALID) {
        check(core.using_opengl && core.gl.pixel_type == GL_UNSIGNED_INT_8_8_8_8_REV);

    	if (core.gl.rhi_interop_memory) {
            // @todo I make no attempt to synchronize the core's drawing operations with RHI reads, some cores will work but others have synchronization issues
            //       It seems like a good reference resource on how to do this is either ITextureShareItem Engine/Source/Programs/TextureShare/TextureShareSDK
        } else {
        // OpenGL is asynchronous and because of GPU driver reasons (work is executed FIFO for some drivers)
        // if we try reading the framebuffer we'll block here and consequently the framerate will be capped by Unreal Engines framerate
        // which will cause stuttering if its too low since most emulated games logic is tied to the framerate so we async copy the framebuffer and check a fence later
            if UNLIKELY(pitch != core.gl.pitch) {
                glBindTexture(GL_TEXTURE_2D, core.gl.texture);
                core.gl.pitch = pitch;
                glPixelStorei(GL_UNPACK_ROW_LENGTH, core.gl.pitch / core.gl.bits_per_pixel);
                glBindTexture(GL_TEXTURE_2D, 0);
            }

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

                        void* frame_buffer = glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
                        check(frame_buffer);
                        prepare_frame_for_upload_to_unreal_RHI(frame_buffer);
                    }

                    { // Download Libretro Core frame from OpenGL asynchronously
                        glBindTexture(GL_TEXTURE_2D, core.gl.texture);
                        glBindBuffer(GL_PIXEL_PACK_BUFFER, core.gl.pixel_buffer_objects[core.free_framebuffer_index]);
                        verify(glUnmapBuffer(GL_PIXEL_PACK_BUFFER) == GL_TRUE);
                        auto async_read_bound_texture_into_bound_pbo = [&]()
                        {
                            GLint mip_level = 0;
                            void* offset_into_pbo_where_data_is_written = 0x0;
                            // This call is async always and a DMA transfer on most platforms
                            glGetTexImage(GL_TEXTURE_2D,
                                mip_level,
                                core.gl.pixel_format,
                                core.gl.pixel_type,
                                offset_into_pbo_where_data_is_written);
                        };
                        async_read_bound_texture_into_bound_pbo();
                        glDeleteSync(core.gl.fence);
                        core.gl.fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
                    }

                    core.free_framebuffer_index = !core.free_framebuffer_index;

                    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
                    glBindTexture(GL_TEXTURE_2D, 0);
                    break;
                }
                case GL_WAIT_FAILED:
                default:
                    checkNoEntry();
            }
        }
    }
    else {
        // *Duplicate frame*
        return;
    }
}

size_t LibretroContext::core_audio_write(const int16_t *buf, size_t frames) {
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

bool LibretroContext::core_environment(unsigned cmd, void *data) {
    bool delegate_status;
    if (CoreEnvironmentCallback)
    {
        delegate_status = CoreEnvironmentCallback(cmd, data);
    }
    
	switch (cmd) {
    case RETRO_ENVIRONMENT_GET_VARIABLE: {
        struct retro_variable* var = (struct retro_variable*)data;

        auto key = std::string(var->key);

        if (core.settings.find(key) != core.settings.end()) {
            var->value = core.settings.at(key).c_str();
            return true;
        }
        else {
            return false;
        }
    }
    case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE: {
        const struct retro_variable* var = (const struct retro_variable*)data;
        //checkNoEntry();
        return false;
    }
    case RETRO_ENVIRONMENT_SET_VARIABLES: {
        const struct retro_variable* arr_var = (const struct retro_variable*)data;
        
        do {
            // Initialize key
            const std::string key(arr_var->key);

            // Parse and initialize default setting, First delimited setting is default by Libretro convention
            auto advance_past_space = [](const char* x) { while (*x == ' ') { x++; } return x; };
            auto past_comment = advance_past_space(strchr(arr_var->value, ';') + 1);
            const char *delimiter_ptr = strchr(arr_var->value, '|');
            if (delimiter_ptr == nullptr) delimiter_ptr = strchr(arr_var->value, '\0');
            std::string default_setting(past_comment, delimiter_ptr - past_comment);
            
            // Write setting to table
            core.settings[key] = default_setting;

        } while ((++arr_var)->key);

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
		
	case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: {
		const enum retro_pixel_format *format = (enum retro_pixel_format *)data;

		if (*format > RETRO_PIXEL_FORMAT_RGB565)
			return false;

        if (core.gl.texture)
            UE_LOG(Libretro, Fatal, TEXT("Tried to change pixel format after initialization."));

        switch (*format) {
	        case RETRO_PIXEL_FORMAT_0RGB1555:
	            core.gl.pixel_type = GL_UNSIGNED_SHORT_5_5_5_1;
	            core.gl.pixel_format = GL_BGRA;
	            core.gl.bits_per_pixel = sizeof(uint16_t);
	            break;
	        case RETRO_PIXEL_FORMAT_XRGB8888:
	            core.gl.pixel_type = GL_UNSIGNED_INT_8_8_8_8_REV;
	            core.gl.pixel_format = GL_RGBA;
	            core.gl.bits_per_pixel = sizeof(uint32_t);
	            break;
	        case RETRO_PIXEL_FORMAT_RGB565:
	            core.gl.pixel_type = GL_UNSIGNED_SHORT_5_6_5;
	            core.gl.pixel_format = GL_RGB;
	            core.gl.bits_per_pixel = sizeof(uint16_t);
	            break;
	        default:
	            UE_LOG(Libretro, Fatal, TEXT("Unknown pixel type %u"), *format);
        }

        return true;
	}
    case RETRO_ENVIRONMENT_SET_HW_RENDER: {
        struct retro_hw_render_callback *hw = (struct retro_hw_render_callback*)data;
        check(hw->context_type < RETRO_HW_CONTEXT_VULKAN);
        hw->get_current_framebuffer = libretro_callbacks->c_get_current_framebuffer;
#pragma warning(push)
#pragma warning(disable:4191)
        hw->get_proc_address = (retro_hw_get_proc_address_t)SDL_GL_GetProcAddress;
#pragma warning(pop)
        core.hw = *hw;
        core.using_opengl = true;
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
        auto controller_info = (const struct retro_controller_info*)data;
        for (unsigned i = 0; i < controller_info->num_types; i++) {
            UE_LOG(Libretro, Verbose, TEXT("Supported Controllers: %s"), ANSI_TO_TCHAR(controller_info->types[i].desc));
        }

        return true;
    }
    case RETRO_ENVIRONMENT_GET_PREFERRED_HW_RENDER: {
        unsigned* library = (unsigned*)data;
        *library = RETRO_HW_CONTEXT_OPENGL_CORE;
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
	default:
		core_log(RETRO_LOG_WARN, "Unhandled env #%u", cmd);
		return false;
	}

    return delegate_status;
}

int16_t LibretroContext::core_input_state(unsigned port, unsigned device, unsigned index, unsigned id) {

    switch (device) {
        case RETRO_DEVICE_ANALOG:   
            //check(index < 2); // "I haven't implemented Triggers and other analog controls yet"
            return InputState[port].analog[id][index];
        case RETRO_DEVICE_JOYPAD:   return InputState[port].digital[id];
        default:                    return 0;
    }
}


void LibretroContext::core_audio_sample(int16_t left, int16_t right) {
	int16_t buf[2] = {left, right};
	core_audio_write(buf, (size_t)1);
}


void LibretroContext::load(const char *sofile) {
	void (*set_environment)(retro_environment_t) = NULL;
	void (*set_video_refresh)(retro_video_refresh_t) = NULL;
	void (*set_input_poll)(retro_input_poll_t) = NULL;
	void (*set_input_state)(retro_input_state_t) = NULL;
	void (*set_audio_sample)(retro_audio_sample_t) = NULL;
	void (*set_audio_sample_batch)(retro_audio_sample_batch_t) = NULL;
    
    libretro_api.handle = FPlatformProcess::GetDllHandle(ANSI_TO_TCHAR(sofile));

	if (!libretro_api.handle)
        UE_LOG(LogTemp, Fatal ,TEXT("Failed to load core: %s"), ANSI_TO_TCHAR(sofile));

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

    libretro_callbacks->video_refresh = [=](const void* data, unsigned width, unsigned height, size_t pitch) { return core_video_refresh(data, width, height, pitch); };
    libretro_callbacks->audio_write = [=](const int16_t *data, size_t frames) { return core_audio_write(data, frames); };
    libretro_callbacks->audio_sample = [=](int16_t left, int16_t right) { return core_audio_sample(left, right); };
    libretro_callbacks->input_state = [=](unsigned port, unsigned device, unsigned index, unsigned id) { return core_input_state(port, device, index, id); };
    libretro_callbacks->input_poll = [=]() { };
    libretro_callbacks->environment = [=](unsigned cmd, void* data) { return core_environment(cmd, data); };
 	libretro_callbacks->get_current_framebuffer = [=]() { return core.gl.framebuffer; };

    set_environment(libretro_callbacks->c_environment);
    set_video_refresh(libretro_callbacks->c_video_refresh);
    set_input_poll(libretro_callbacks->c_input_poll);
    set_input_state(libretro_callbacks->c_input_state);
    set_audio_sample(libretro_callbacks->c_audio_sample);
    set_audio_sample_batch(libretro_callbacks->c_audio_write);

	libretro_api.init();
	libretro_api.initialized = true;
}


void LibretroContext::load_game(const char* filename) {
    struct retro_system_info system = { 0 };
    struct retro_game_info info = { filename, 0 };

    SDL_RWops* file = SDL_RWFromFile(filename, "rb");

    if (!file)
        UE_LOG(Libretro, Fatal, TEXT("Failed to load %hs: %hs"), filename, SDL_GetError());

    info.path = filename;
    info.meta = "";
    info.data = NULL;
    info.size = SDL_RWsize(file);

    libretro_api.get_system_info(&system);

    if (!system.need_fullpath) {
        info.data = SDL_malloc(info.size);

        if (!info.data)
            UE_LOG(Libretro, Fatal, TEXT("Failed to allocate memory for the content"));

        if (!SDL_RWread(file, (void*)info.data, info.size, 1))
            UE_LOG(Libretro, Fatal, TEXT("Failed to read file data: %hs"), SDL_GetError());
    }

    if (!libretro_api.load_game(&info))
        UE_LOG(Libretro, Fatal, TEXT("The core failed to load the content."));

    libretro_api.get_system_av_info(&core.av);
 	
    if (core.using_opengl) {
// SDL State isn't threadlocal like OpenGL so we have to synchronize here when we create a window
#if PLATFORM_APPLE
        // Apple OS's impose an additional requirement that 'all' rendering operations are done on the main thread
        dispatch_sync(dispatch_get_main_queue(),
            ^{
                create_window();
             });
#else
        static FCriticalSection WindowLock; // Threadsafe initializatation as of c++11
        FScopeLock scoped_lock(&WindowLock);
        create_window();
#endif
    }

    video_configure(&core.av.geometry);
 	
    if (info.data)
        SDL_free((void*)info.data);

    SDL_RWclose(file);
    
}

LibretroContext* LibretroContext::Launch(FString core, FString game, UTextureRenderTarget2D* RenderTarget, URawAudioSoundWave* SoundBuffer, TUniqueFunction<void(LibretroContext*, libretro_api_t&)> LoadedCallback)
{

    check(IsInGameThread()); // So static initialization is safe + UObject access

    static const uint32 max_instances = sizeof(libretro_callbacks_table) / sizeof(libretro_callbacks_table[0]);
    static FCriticalSection CallbacksLock;
    static TBitArray<TInlineAllocator<(max_instances / 8) + 1>> AllocatedInstances(false, max_instances);

    LibretroContext *l = new LibretroContext();

    auto ConvertPath = [](auto &core_directory, const FString& CoreDirectory)
    {
        FString AbsoluteCoreDirectory = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*FUnrealLibretroModule::IfRelativeResolvePathRelativeToThisPluginWithPathExtensions(CoreDirectory));
        core_directory.SetNumZeroed(TStringConvert<TCHAR, char>::ConvertedLength(*AbsoluteCoreDirectory, AbsoluteCoreDirectory.Len()) + 1);
        TStringConvert<TCHAR, char>::Convert(core_directory.GetData(), // has internal assertion if fails
                                             core_directory.Num(),
                                            *AbsoluteCoreDirectory,
                                             AbsoluteCoreDirectory.Len());
    };

    auto LibretroSettings = GetDefault<ULibretroSettings>();

    ConvertPath(l->core.save_directory,   LibretroSettings->CoreSaveDirectory);
    ConvertPath(l->core.system_directory, LibretroSettings->CoreSystemDirectory);

    l->UnrealRenderTarget = MakeWeakObjectPtr(RenderTarget);
    l->UnrealSoundBuffer  = MakeWeakObjectPtr(SoundBuffer );

    // Kick the initialization process off to another thread. It shouldn't be added to the Unreal task pool because those are too slow and my code relies on OpenGL state being thread local.
    // The Runnable system is the standard way for spawning and managing threads in Unreal. FThread looks enticing, but they removed any way to detach threads since "it doesn't work as expected"
    FLambdaRunnable::RunLambdaOnBackGroundThread(FPaths::GetCleanFilename(core) + FPaths::GetCleanFilename(game),
        [=, LoadedCallback = MoveTemp(LoadedCallback)]() {

            // Grab a statically generated callback structure
            int32 InstanceNumber;
            {
                FScopeLock ScopeLock(&CallbacksLock);

                InstanceNumber = AllocatedInstances.FindAndSetFirstZeroBit();
                check(InstanceNumber != INDEX_NONE);
                l->libretro_callbacks = libretro_callbacks_table + InstanceNumber;
            }
        	
            // Here I load a copy of the dll instead of the original. If you load the same dll multiple times you won't obtain a new instance of the dll loaded into memory,
            // instead all variables and function pointers will point to the original loaded dll
            const FString InstancedCorePath = FString::Printf(TEXT("%s.%s"), *FGuid::NewGuid().ToString(), *FPaths::GetExtension(*core));
            verify(IPlatformFile::GetPlatformPhysical().CopyFile(*InstancedCorePath, *core));

            l->core.hw.version_major = 4;
            l->core.hw.version_minor = 5;
            l->core.hw.context_type = RETRO_HW_CONTEXT_OPENGL_CORE;
            l->core.hw.context_reset = []() {};
            l->core.hw.context_destroy = []() {};

            // Loads the dll and its function pointers into libretro_api
            l->load(TCHAR_TO_ANSI(*IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*InstancedCorePath)));

            // This does load the game but does many other things as well. If hardware rendering is needed it loads OpenGL resources from the OS and this also initializes the unreal engine resources for audio and video.
            l->load_game(TCHAR_TO_ANSI(*IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*game).Replace(TEXT("/"), TEXT("\\"))));
		
            // This is needed for some cores nestopia specifically is one example
            l->libretro_api.set_controller_port_device(0, RETRO_DEVICE_JOYPAD);

            LoadedCallback(l, l->libretro_api);
        	
        	// This simplifies the logic in core_video_refresh
            if (l->core.using_opengl) {
                l->glBindBuffer(GL_PIXEL_PACK_BUFFER, l->core.gl.pixel_buffer_objects[l->core.free_framebuffer_index]);
                l->glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
                l->core.gl.fence = l->glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
                l->glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
            }

            uint64 frames = 0;
            auto   start = FDateTime::Now();
            while (l->CoreState.load(std::memory_order_relaxed) != ECoreState::Shutdown)
            {
                DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Frame"), STAT_LibretroFrame, STATGROUP_UnrealLibretro);
                {
                    DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Work"), STAT_LibretroWork, STATGROUP_UnrealLibretro);

                    if (l->CoreState.load(std::memory_order_relaxed) == ECoreState::Running)
                    {
                        l->libretro_api.run();
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

            // Explicit Cleanup
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
        	
            {
                FScopeLock ScopedLock(&CallbacksLock);

                AllocatedInstances[InstanceNumber] = false;
            }
        	
            {
                if (l->core.gl.window)
                {
                    verify(SDL_GL_MakeCurrent(l->core.gl.window, NULL) == 0);
                    SDL_DestroyWindow(l->core.gl.window);  // @todo: In SDLarch's code SDL_Quit was here and that implicitly destroyed some things like windows. So I'm not sure if I'm exhaustively destroying everything that it destroyed yet. In order to fix this you could probably just run SDL_Quit here and step with the debugger to see all the stuff it destroys.
                }
            	
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

                                if (l->core.gl.context)
                                {
                                    SDL_GL_DeleteContext(l->core.gl.context); /** implicitly releases resources like fbos, pbos, and textures */
                                }

                                l->Unreal.TextureRHI.SafeRelease();
                        	
                                delete l; /** Task queue released */
                            });
                    }
                );
            }
        }
    );

    return l;
}

void LibretroContext::Shutdown(LibretroContext* Instance) 
{
	// We enqueue the shutdown procedure as the final task since we want outstanding tasks to be executed first
    Instance->EnqueueTask([Instance](auto&&)
        {
            Instance->CoreState.store(ECoreState::Shutdown, std::memory_order_relaxed);
        });
}

void LibretroContext::Pause(bool ShouldPause)
{
    this->CoreState.store(ShouldPause ? ECoreState::Paused : ECoreState::Running,
                          std::memory_order_relaxed);
}

void LibretroContext::EnqueueTask(TUniqueFunction<void(libretro_api_t&)> LibretroAPITask)
{
    check(IsInGameThread()); // LibretroAPITasks is a single producer single consumer queue
    LibretroAPITasks.Enqueue(MoveTemp(LibretroAPITask));
};
