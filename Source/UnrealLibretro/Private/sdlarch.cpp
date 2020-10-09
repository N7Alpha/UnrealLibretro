
#include "sdlarch.h"
#include "LibretroCoreInstance.h"

#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"

#if PLATFORM_APPLE
#include <dispatch/dispatch.h>
#endif

DEFINE_LOG_CATEGORY(Libretro)
#define DEBUG_OPENGL 0

// MY EYEEEEESSS.... Even though this looks heavily obfuscated what this actually accomplishes is relatively simple. It allows us to run multiple libretro cores at once. 
// We have to do it this way because when libretro calls a callback we implemented there really isn't any suitable way to tell which core the call came from.
// So we just statically generate a bunch of callback functions with macros and write their function pointers into an array of func_wrap_t's and issue them at runtime.
// These generated callbacks call std::functions which can capture arguments. So we capture this and now it calls our callbacks on a per instance basis.
#define REP10(P, M)  M(P##0) M(P##1) M(P##2) M(P##3) M(P##4) M(P##5) M(P##6) M(P##7) M(P##8) M(P##9)
#define REP100(M) REP10(,M) REP10(1,M) REP10(2,M) REP10(3,M) REP10(4,M) REP10(5,M) REP10(6,M) REP10(7,M) REP10(8,M) REP10(9,M)

#define FUNC_WRAP_INIT(M) { M ? func_wrap_table+M-1 : 0, func_wrap_audio_write##M, func_wrap_video_refresh##M, func_wrap_audio_sample##M, func_wrap_environment##M, func_wrap_input_poll##M, func_wrap_input_state##M, func_wrap_get_current_framebuffer##M },
#define FUNC_WRAP_DEF(M) size_t  func_wrap_audio_write##M(const int16_t *data, size_t frames) { return func_wrap_table[M].audio_write(data, frames); } \
                         void    func_wrap_video_refresh##M(const void *data, unsigned width, unsigned height, size_t pitch) { return func_wrap_table[M].video_refresh(data, width, height, pitch); } \
                         void    func_wrap_audio_sample##M(int16_t left, int16_t right) { return func_wrap_table[M].audio_sample(left, right); } \
                         bool    func_wrap_environment##M(unsigned cmd, void *data) { return func_wrap_table[M].environment(cmd, data); } \
                         void    func_wrap_input_poll##M() { return func_wrap_table[M].input_poll(); } \
                         int16_t func_wrap_input_state##M(unsigned port, unsigned device, unsigned index, unsigned id) { return func_wrap_table[M].input_state(port, device, index, id); } \
                         uintptr_t func_wrap_get_current_framebuffer##M() { return func_wrap_table[M].get_current_framebuffer(); }


REP100(FUNC_WRAP_DEF)
func_wrap_t func_wrap_table[] = { REP100(FUNC_WRAP_INIT) };
func_wrap_t* func_wrap_freelist = &func_wrap_table[99];

#define ENUM_GL_PROCEDURES(EnumMacro) \
        EnumMacro(PFNGLBINDFRAMEBUFFERPROC, glBindFramebuffer) \
        EnumMacro(PFNGLBINDRENDERBUFFERPROC, glBindRenderbuffer) \
        EnumMacro(PFNGLBINDTEXTUREPROC, glBindTexture) \
        EnumMacro(PFNGLCHECKFRAMEBUFFERSTATUSPROC, glCheckFramebufferStatus) \
        EnumMacro(PFNGLCLEARPROC, glClear) \
        EnumMacro(PFNGLCLEARCOLORPROC, glClearColor) \
        EnumMacro(PFNGLDEBUGMESSAGECALLBACKPROC, glDebugMessageCallback) \
        EnumMacro(PFNGLDEBUGMESSAGECONTROLPROC, glDebugMessageControl) \
        EnumMacro(PFNGLDELETEBUFFERSPROC, glDeleteBuffers) \
        EnumMacro(PFNGLDELETETEXTURESPROC, glDeleteTextures) \
        EnumMacro(PFNGLENABLEPROC, glEnable) \
        EnumMacro(PFNGLFRAMEBUFFERRENDERBUFFERPROC, glFramebufferRenderbuffer) \
        EnumMacro(PFNGLFRAMEBUFFERTEXTURE2DPROC, glFramebufferTexture2D) \
        EnumMacro(PFNGLGENFRAMEBUFFERSPROC, glGenFramebuffers) \
        EnumMacro(PFNGLGENRENDERBUFFERSPROC, glGenRenderbuffers) \
        EnumMacro(PFNGLGENTEXTURESPROC, glGenTextures) \
        EnumMacro(PFNGLGETINTEGERVPROC, glGetIntegerv) \
        EnumMacro(PFNGLGETSTRINGPROC, glGetString) \
        EnumMacro(PFNGLGETTEXIMAGEPROC, glGetTexImage) \
        EnumMacro(PFNGLPIXELSTOREIPROC, glPixelStorei) \
        EnumMacro(PFNGLRENDERBUFFERSTORAGEPROC, glRenderbufferStorage) \
        EnumMacro(PFNGLTEXIMAGE2DPROC, glTexImage2D)

/** Define all GL functions. */
#define DEFINE_GL_PROCEDURES(Type,Func) static Type Func = NULL;
ENUM_GL_PROCEDURES(DEFINE_GL_PROCEDURES);

void glDebugOutput(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam) {

    switch (severity) {
    case GL_DEBUG_SEVERITY_HIGH:
        UE_LOG(LogTemp, Fatal, TEXT("%s"), ANSI_TO_TCHAR(message));
        break;
    case GL_DEBUG_SEVERITY_MEDIUM:
        UE_LOG(LogTemp, Warning, TEXT("%s"), ANSI_TO_TCHAR(message));
        break;
    case GL_DEBUG_SEVERITY_LOW:
        UE_LOG(LogTemp, Warning, TEXT("%s"), ANSI_TO_TCHAR(message));
        break;
    case GL_DEBUG_SEVERITY_NOTIFICATION:
    default:
        UE_LOG(LogTemp, Verbose, TEXT("%s"), ANSI_TO_TCHAR(message));
        break;
    }

}





#define load_sym(V, S) do {\
    if (0 == ((*(void**)&V) = FPlatformProcess::GetDllExport(g_retro.handle, TEXT(#S)))) \
        UE_LOG(Libretro, Fatal, TEXT("Failed to load symbol '" #S "'': %u"), FPlatformMisc::GetLastError()); \
	} while (0)
#define load_retro_sym(S) load_sym(g_retro.S, retro_##S)


 void LibretroContext::init_framebuffer(int width, int height)
{
    glGenFramebuffers(1, &g_video.fbo_id);
    glBindFramebuffer(GL_FRAMEBUFFER, g_video.fbo_id);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_video.tex_id, 0);

    if (g_video.hw.depth && g_video.hw.stencil) {
        glGenRenderbuffers(1, &g_video.rbo_id);
        glBindRenderbuffer(GL_RENDERBUFFER, g_video.rbo_id);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);

        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, g_video.rbo_id);
    } else if (g_video.hw.depth) {
        glGenRenderbuffers(1, &g_video.rbo_id);
        glBindRenderbuffer(GL_RENDERBUFFER, g_video.rbo_id);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);

        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, g_video.rbo_id);
    }

    if (g_video.hw.depth || g_video.hw.stencil)
        glBindRenderbuffer(GL_RENDERBUFFER, 0);

    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    SDL_assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}



 void LibretroContext::create_window(int width, int height) {
    SDL_GL_ResetAttributes(); // SDL state isn't thread local unlike OpenGL. So Libretro Cores could potentially interfere with eachother's Attributes since you're setting globals.

    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

    if (g_video.hw.context_type == RETRO_HW_CONTEXT_OPENGL_CORE || g_video.hw.version_major >= 3) {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, g_video.hw.version_major); 
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, g_video.hw.version_minor);
    }

    switch (g_video.hw.context_type) {
    case RETRO_HW_CONTEXT_OPENGL_CORE:
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        break;
    case RETRO_HW_CONTEXT_OPENGLES2:
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
        break;
    case RETRO_HW_CONTEXT_OPENGL:
        if (g_video.hw.version_major >= 3)
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
        break;
    default:
        UE_LOG(Libretro, Fatal, TEXT("Unsupported hw context %i. (only OPENGL, OPENGL_CORE and OPENGLES2 supported)"), g_video.hw.context_type);
    }

    // Might be able to use this instead SWindow::GetNativeWindow()
    g_win = SDL_CreateWindow("sdlarch", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 0, 0, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN); // @todo This is fine on windows, but creating a window from a background thread will crash on some versions Linux if you don't enable a special flag and everytime on MacOS

	if (!g_win)
        UE_LOG(Libretro, Fatal, TEXT("Failed to create window: %s"), SDL_GetError());

#if DEBUG_OPENGL
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
#endif
    g_ctx = SDL_GL_CreateContext(g_win);


    if (!g_ctx)
        UE_LOG(Libretro, Fatal, TEXT("Failed to create OpenGL context: %s"), SDL_GetError());

    if (g_video.hw.context_type == RETRO_HW_CONTEXT_OPENGLES2) {
        /// @todo auto success = gladLoadGLES2((GLADloadfunc)SDL_GL_GetProcAddress);
        checkNoEntry();
    } else {
        #pragma warning(push)
        #pragma warning(disable:4191)

        // Initialize all entry points.
        #define GET_GL_PROCEDURES(Type,Func) Func = (Type)SDL_GL_GetProcAddress(#Func);
        ENUM_GL_PROCEDURES(GET_GL_PROCEDURES);

        // Check that all of the entry points have been initialized.
        bool bFoundAllEntryPoints = true;
        #define CHECK_GL_PROCEDURES(Type,Func) if (Func == NULL) { bFoundAllEntryPoints = false; UE_LOG(Libretro, Warning, TEXT("Failed to find entry point for %s"), TEXT(#Func)); }
        ENUM_GL_PROCEDURES(CHECK_GL_PROCEDURES);
        checkf(bFoundAllEntryPoints, TEXT("Failed to find all OpenGL entry points."));

        // Restore warning C4191.
        #pragma warning(pop)
    }

#if DEBUG_OPENGL
    GLint opengl_flags; 
    glGetIntegerv(GL_CONTEXT_FLAGS, &opengl_flags);
    if (GLAD_GL_VERSION_4_3) {
        if (opengl_flags & GL_CONTEXT_FLAG_DEBUG_BIT)
        {
            glEnable(GL_DEBUG_OUTPUT);
            glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
            glDebugMessageCallback(glDebugOutput, nullptr);
            glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
        }

    }
#endif

    UE_LOG(Libretro, Verbose, TEXT("GL_SHADING_LANGUAGE_VERSION: %s\n"), glGetString(GL_SHADING_LANGUAGE_VERSION));
    UE_LOG(Libretro, Verbose, TEXT("GL_VERSION: %s\n"), glGetString(GL_VERSION));
}


 void LibretroContext::resize_to_aspect(double ratio, int sw, int sh, int *dw, int *dh) {
	*dw = sw;
	*dh = sh;

	if (ratio <= 0)
		ratio = (double)sw / sh;

	if ((float)sw / sh < 1)
		*dw = *dh * ratio;
	else
		*dh = *dw / ratio;
}


 void LibretroContext::video_configure(const struct retro_game_geometry *geom) {
	int nwidth, nheight;

	resize_to_aspect(geom->aspect_ratio, geom->base_width * 1, geom->base_height * 1, &nwidth, &nheight);

	nwidth *= g_scale;
	nheight *= g_scale;
    

    if (!g_win) { // Create window
        #if PLATFORM_APPLE
            dispatch_sync(dispatch_get_main_queue(),
            ^{
                create_window(nwidth, nheight);
            });
        #else
            static FCriticalSection WindowLock; // SDL State isn't threadlocal like OpenGL so we haveto synchronize here when we create a window @todo don't think the initialization of WindowLock is threadsafe here unless its constructor does some magic
            FScopeLock scoped_lock(&WindowLock);
            create_window(nwidth, nheight);
        #endif
    }

    if (g_video.tex_id)
        glDeleteTextures(1, &g_video.tex_id);

    g_video.tex_id = 0;

	if (!g_video.pixfmt)
		g_video.pixfmt = GL_UNSIGNED_SHORT_5_5_5_1;

	glGenTextures(1, &g_video.tex_id);

	if (!g_video.tex_id)
		UE_LOG(Libretro, Fatal, TEXT("Failed to create the video texture"));

	g_video.pitch = geom->base_width * g_video.bpp;

	glBindTexture(GL_TEXTURE_2D, g_video.tex_id);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, geom->max_width, geom->max_height, 0,
			g_video.pixtype, g_video.pixfmt, NULL);

	glBindTexture(GL_TEXTURE_2D, 0);

    init_framebuffer(geom->base_width, geom->base_height);

	g_video.tex_w = geom->max_width;
	g_video.tex_h = geom->max_height;
	g_video.clip_w = geom->base_width;
	g_video.clip_h = geom->base_height;



    g_video.hw.context_reset();
}


 bool LibretroContext::video_set_pixel_format(unsigned format) {
	if (g_video.tex_id)
		UE_LOG(Libretro, Fatal, TEXT("Tried to change pixel format after initialization."));

	switch (format) {
	case RETRO_PIXEL_FORMAT_0RGB1555:
		g_video.pixfmt = GL_UNSIGNED_SHORT_5_5_5_1;
		g_video.pixtype = GL_BGRA;
		g_video.bpp = sizeof(uint16_t);
		break;
	case RETRO_PIXEL_FORMAT_XRGB8888:
		g_video.pixfmt = GL_UNSIGNED_INT_8_8_8_8_REV;
		g_video.pixtype = GL_BGRA;
		g_video.bpp = sizeof(uint32_t);
		break;
	case RETRO_PIXEL_FORMAT_RGB565:
		g_video.pixfmt  = GL_UNSIGNED_SHORT_5_6_5;
		g_video.pixtype = GL_RGB;
		g_video.bpp = sizeof(uint16_t);
		break;
	default:
		UE_LOG(Libretro, Fatal, TEXT("Unknown pixel type %u"), format);
	}

	return true;
}



 struct _1555_color
 {
     uint16 R : 5;
     uint16 G : 5;
     uint16 B : 5;
     uint16 A : 1;
 };


// Stripped down code for profiling purposes https://godbolt.org/z/c57esx
// @todo : Theres a data race here that seems like it would rarely hit and would just result in screen tearing. I should eventually just use a lock and swap the pointer from the background thread and lock when reading in data on the render thread. Doesn't seem like a big deal for now.
// @todo : From what I've read it should be unsafe to have a data race on a buffer shared between two threads not using an atomic wrapper at least for c++11, but I don't really fully understand this yet and this seems to work
 void LibretroContext::video_refresh(const void *data, unsigned width, unsigned height, unsigned pitch) {

    check(bgra_buffers[0]->Num() == width * height);
     
    // @todo: this might need to be wrapped in some atomic primitive so the data isn't modified
     auto bgra_buffer = bgra_buffers[which = !which].Get()->GetData();

    // if framebuffer is on CPU
    if (data && data != RETRO_HW_FRAME_BUFFER_VALID) {
        auto region = FUpdateTextureRegion2D(0, 0, 0, 0, width, height);
        
        const auto& _5_to_8_bit = [](uint8 pixel_channel)->uint8 { return (pixel_channel << 3) | (pixel_channel >> 2); };
        const auto& _6_to_8_bit = [](uint8 pixel_channel)->uint8 { return (pixel_channel << 2) | (pixel_channel >> 4); };

        switch (g_video.pixfmt) {
            case GL_UNSIGNED_SHORT_5_6_5: {
                auto* rgb565_buffer = (FDXTColor565*)data;
                pitch = pitch / sizeof(rgb565_buffer[0]);

                for (unsigned int y = 0; y < height; y++) {
                    for (unsigned int x = 0; x < width; x++) {
                        bgra_buffer[x + y * width].B = _5_to_8_bit(rgb565_buffer[x + pitch * y].B);
                        bgra_buffer[x + y * width].G = _6_to_8_bit(rgb565_buffer[x + pitch * y].G);
                        bgra_buffer[x + y * width].R = _5_to_8_bit(rgb565_buffer[x + pitch * y].R);
                        bgra_buffer[x + y * width].A = 255;
                    }
                }
            }
            break;
            case GL_UNSIGNED_SHORT_5_5_5_1: {
                auto* rgba5551_buffer = (_1555_color*)data;
                pitch = pitch / sizeof(rgba5551_buffer[0]);

                for (unsigned int y = 0; y < height; y++) {
                    for (unsigned int x = 0; x < width; x++) {
                        bgra_buffer[x + y * width].B = _5_to_8_bit(rgba5551_buffer[x + pitch * y].B);
                        bgra_buffer[x + y * width].G = _5_to_8_bit(rgba5551_buffer[x + pitch * y].G);
                        bgra_buffer[x + y * width].R = _5_to_8_bit(rgba5551_buffer[x + pitch * y].R);
                        bgra_buffer[x + y * width].A = 255;
                    }
                }
            }
            break;
            case GL_UNSIGNED_INT_8_8_8_8_REV: {
                auto* bgra8888_buffer = (_8888_color*)data;
                pitch = pitch / sizeof(bgra8888_buffer[0]);

                for (unsigned int y = 0; y < height; y++) {
                    for (unsigned int x = 0; x < width; x++) {
                        bgra_buffer[x + y * width].B = bgra8888_buffer[x + pitch * y].B;
                        bgra_buffer[x + y * width].G = bgra8888_buffer[x + pitch * y].G;
                        bgra_buffer[x + y * width].R = bgra8888_buffer[x + pitch * y].R;
                        bgra_buffer[x + y * width].A = 255;
                    }
                }
            }
            break;
            default:
                checkNoEntry();
        }
    }
    //   if  framebuffer is on GPU
    else if (data == RETRO_HW_FRAME_BUFFER_VALID) {

        if (g_video.clip_w != width || g_video.clip_h != height)
        {
            g_video.clip_h = height;
            g_video.clip_w = width;
        }

        check(UsingOpenGL && g_video.pixfmt == GL_UNSIGNED_INT_8_8_8_8_REV);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, g_video.tex_id);
        if (pitch != g_video.pitch) { // @todo: I should check if pitch is used anywhere else
            g_video.pitch = pitch;
            glPixelStorei(GL_UNPACK_ROW_LENGTH, g_video.pitch / g_video.bpp);
        }

        glGetTexImage(GL_TEXTURE_2D,
            0,
            g_video.pixtype,
            g_video.pixfmt,
            bgra_buffer);
        glBindTexture(GL_TEXTURE_2D, 0);

    }
    else {
        // *Duplicate frame*
        return;
    }

    auto *old_buffer = RenderThreadsBuffer.Exchange(bgra_buffer);

    if (!old_buffer) {
        ENQUEUE_RENDER_COMMAND(CopyToUnrealFramebufferTask) // @todo this triggers an assert on MacOS
        (
            [TextureRHI = this->TextureRHI.GetReference(), MipIndex = 0, Region = FUpdateTextureRegion2D(0, 0, 0, 0, width, height), SrcPitch = 4 * width, SrcBpp = 4, &Buffer = this->RenderThreadsBuffer]
            (FRHICommandListImmediate& RHICmdList)
            {
            check(TextureRHI);

            uint8* SrcData = (uint8*)Buffer.Exchange(nullptr);

            RHIUpdateTexture2D(
                TextureRHI,
                MipIndex,
                Region,
                SrcPitch,
                SrcData
                + Region.SrcY * SrcPitch
                + Region.SrcX * SrcBpp
            );

            }
        );
    }
}

 void LibretroContext::video_deinit() {
	if (g_video.tex_id)
		glDeleteTextures(1, &g_video.tex_id);

	g_video.tex_id = 0;
}

size_t LibretroContext::audio_write(const int16_t *buf, size_t frames) {
    if LIKELY( enqueue_audio ) { 
        unsigned FramesEnqueued = 0;
        while (FramesEnqueued < frames && QueuedAudio->Enqueue(((int32*)buf)[FramesEnqueued])) {
            FramesEnqueued++;
        }

        return FramesEnqueued;
    } else {
        return frames; // With some cores audio_write is called repeatedly until it is written. So we have to check to make sure we're still supposed to be running since the consumer on the gamethread may stop consuming audio anytime which would put us in an infinite loop if we didn't check.
                       // You could also just lie to the core and always say you're reading in all the data all the time, but I've found that causes more audio buffer underruns somehow.
    }
    
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
        UE_LOG(Libretro, Verbose, TEXT("%s"), ANSI_TO_TCHAR(buffer));
        break;
    case RETRO_LOG_INFO:
        UE_LOG(Libretro, Log, TEXT("%s"), ANSI_TO_TCHAR(buffer));
        break;
    case RETRO_LOG_WARN:
        UE_LOG(Libretro, Warning, TEXT("%s"), ANSI_TO_TCHAR(buffer));
        break;
    case RETRO_LOG_ERROR:
        UE_LOG(Libretro, Fatal, TEXT("%s"), ANSI_TO_TCHAR(buffer));
        break;
    }

}

uintptr_t LibretroContext::core_get_current_framebuffer() {
    return g_video.fbo_id;
}


bool LibretroContext::core_environment(unsigned cmd, void *data) {
	bool *bval;

	switch (cmd) {
    case RETRO_ENVIRONMENT_GET_VARIABLE: {
        struct retro_variable* var = (struct retro_variable*)data;

        auto key = std::string(var->key);

        if (settings.find(key) != settings.end()) {
            var->value = settings.at(key).c_str();
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
            auto delimiter_ptr = strchr(arr_var->value, '|');
            std::string default_setting(past_comment, delimiter_ptr - past_comment);
            
            // Write setting to table
            settings[key] = default_setting;

        } while ((++arr_var)->key);

        return true;
    }
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS: {
        checkNoEntry();

        return true;
    }
	case RETRO_ENVIRONMENT_GET_LOG_INTERFACE: {
		struct retro_log_callback *cb = (struct retro_log_callback *)data;
		cb->log = core_log;
        return true;
	}
	case RETRO_ENVIRONMENT_GET_CAN_DUPE:
		bval = (bool*)data;
		*bval = true;
        return true;
	case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: {
		const enum retro_pixel_format *fmt = (enum retro_pixel_format *)data;

		if (*fmt > RETRO_PIXEL_FORMAT_RGB565)
			return false;

		return video_set_pixel_format(*fmt);
	}
    case RETRO_ENVIRONMENT_SET_HW_RENDER: {
        struct retro_hw_render_callback *hw = (struct retro_hw_render_callback*)data;
        hw->get_current_framebuffer = callback_instance->c_get_current_framebuffer;
        hw->get_proc_address = (retro_hw_get_proc_address_t)SDL_GL_GetProcAddress;
        g_video.hw = *hw;
        UsingOpenGL = true;
        return true;
    }
    case RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK: {
        const struct retro_frame_time_callback *frame_time =
            (const struct retro_frame_time_callback*)data;
        runloop_frame_time = *frame_time;
        break;
    }
    case RETRO_ENVIRONMENT_SET_AUDIO_CALLBACK: {
        struct retro_audio_callback *audio_cb = (struct retro_audio_callback*)data;
        audio_callback = *audio_cb;
        return true;
    }
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY: {
        const char** retro_path = (const char**)data;
        *retro_path = save_directory.data();

        return true;
    }
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY: {
        const char** retro_path = (const char**)data;
        *retro_path = system_directory.data();
        
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
            UE_LOG(LogTemp, Warning, TEXT("Button Found: %s"), ANSI_TO_TCHAR(input_descriptor->description));
        } while ((++input_descriptor)->description);

        return true;
    } 
    case RETRO_ENVIRONMENT_GET_PREFERRED_HW_RENDER: {
        unsigned* library = (unsigned*)data;
        *library = RETRO_HW_CONTEXT_OPENGL_CORE;
        return true;
    }
    case RETRO_ENVIRONMENT_SET_HW_SHARED_CONTEXT: {
        return true;
    }
    case RETRO_ENVIRONMENT_SET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE: {
        const struct retro_hw_render_context_negotiation_interface* iface =
            (const struct retro_hw_render_context_negotiation_interface*)data;

        hw_render_context_negotiation = iface;
        return true;
    }
	default:
		core_log(RETRO_LOG_WARN, "Unhandled env #%u", cmd);
		return false;
	}

    return false;
}


void LibretroContext::core_input_poll(void) {

}


int16_t LibretroContext::core_input_state(unsigned port, unsigned device, unsigned index, unsigned id) {

    switch (device) {
        case RETRO_DEVICE_ANALOG:   
            check(index < 2); // "I haven't implemented Triggers and other analog controls yet"
            return (*UnrealInputState)[port].analog[id][index];
        case RETRO_DEVICE_JOYPAD:   return (*UnrealInputState)[port].digital[id];
        default:                    return 0;
    }
}


void LibretroContext::core_audio_sample(int16_t left, int16_t right) {
	int16_t buf[2] = {left, right};
	audio_write(buf, (size_t)1);
}


void LibretroContext::core_load(const char *sofile) {
	void (*set_environment)(retro_environment_t) = NULL;
	void (*set_video_refresh)(retro_video_refresh_t) = NULL;
	void (*set_input_poll)(retro_input_poll_t) = NULL;
	void (*set_input_state)(retro_input_state_t) = NULL;
	void (*set_audio_sample)(retro_audio_sample_t) = NULL;
	void (*set_audio_sample_batch)(retro_audio_sample_batch_t) = NULL;
	memset(&g_retro, 0, sizeof(g_retro));
    auto LibretroPluginRootPath = IPluginManager::Get().FindPlugin("UnrealLibretro")->GetBaseDir();
    auto dllPath = FPaths::Combine(LibretroPluginRootPath, "libretro");
    FPlatformProcess::AddDllDirectory(*dllPath); // @todo: Cleanup the directory searching stuff here and in LibretroCoreInstance
    g_retro.handle = FPlatformProcess::GetDllHandle(ANSI_TO_TCHAR(sofile));

	if (!g_retro.handle)
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

    callback_instance->audio_write = [=](const int16_t *data, size_t frames) {
        return audio_write(data, frames);
    };
    callback_instance->video_refresh = [=](const void* data, unsigned width, unsigned height, size_t pitch) {
        return video_refresh(data, width, height, pitch);
    };
    callback_instance->environment = [=](unsigned cmd, void* data) { return core_environment(cmd, data); };
    callback_instance->audio_sample = [=](int16_t left, int16_t right) { return core_audio_sample(left, right); };
    callback_instance->input_poll = [=]() { return core_input_poll();  };
    callback_instance->input_state = [=](unsigned port, unsigned device, unsigned index, unsigned id) { return core_input_state(port, device, index, id); };
    callback_instance->get_current_framebuffer = [=]() { return core_get_current_framebuffer(); };

    set_environment(callback_instance->c_environment);
    set_video_refresh(callback_instance->c_video_refresh);
    set_input_poll(callback_instance->c_input_poll);
    set_input_state(callback_instance->c_input_state);
    set_audio_sample(callback_instance->c_audio_sample);
    set_audio_sample_batch(callback_instance->c_audio_write);

    

	g_retro.init();
	g_retro.initialized = true;

	UE_LOG(Libretro, Log, TEXT("Core loaded"));
}


void LibretroContext::core_load_game(const char* filename) {
    struct retro_system_info system = { 0 };
    struct retro_game_info info = { filename, 0 };

    SDL_RWops* file = SDL_RWFromFile(filename, "rb");

    if (!file)
        UE_LOG(Libretro, Fatal, TEXT("Failed to load %hs: %hs"), filename, SDL_GetError());

    info.path = filename;
    info.meta = "";
    info.data = NULL;
    info.size = SDL_RWsize(file);

    g_retro.get_system_info(&system);

    if (!system.need_fullpath) {
        info.data = SDL_malloc(info.size);

        if (!info.data)
            UE_LOG(Libretro, Fatal, TEXT("Failed to allocate memory for the content"));

        if (!SDL_RWread(file, (void*)info.data, info.size, 1))
            UE_LOG(Libretro, Fatal, TEXT("Failed to read file data: %hs"), SDL_GetError());
    }

    if (!g_retro.load_game(&info))
        UE_LOG(Libretro, Fatal, TEXT("The core failed to load the content."));

    g_retro.get_system_av_info(&av);
    
    // @todo: move this
    
    bgra_buffers[0] = MakeShared<TArray<_8888_color>, ESPMode::ThreadSafe>();
    bgra_buffers[0]->AddUninitialized(av.geometry.base_width * av.geometry.base_height);
    bgra_buffers[1] = MakeShared<TArray<_8888_color>, ESPMode::ThreadSafe>();
    bgra_buffers[1]->AddUninitialized(av.geometry.base_width * av.geometry.base_height);

    if (UsingOpenGL) {
        video_configure(&av.geometry);
    }

    { // Unreal Resource init
        auto RenderInitTask = FPlatformProcess::GetSynchEventFromPool();
        auto GameThreadMediaResourceInitTask = FFunctionGraphTask::CreateAndDispatchWhenReady([=]
        {
            // Make sure the game objects haven't been invalidated
            if (!UnrealSoundBuffer.IsValid() || !UnrealRenderTarget.IsValid())
            { 
                { // @hack until we acquire are own resources and don't rely on getting them by proxy through a UObject
                    callback_instance->audio_write = [](const int16_t* data, size_t frames) {
                        return frames;
                    };
                    callback_instance->video_refresh = [=](const void* data, unsigned width, unsigned height, size_t pitch) {};
                    callback_instance->audio_sample = [=](int16_t left, int16_t right) {};
                }
                RenderInitTask->Trigger();
                return; 
            }

            //  Video Init
            UnrealRenderTarget->InitCustomFormat(av.geometry.base_width, av.geometry.base_height, PF_B8G8R8A8, false);
            ENQUEUE_RENDER_COMMAND(InitCommand)
            (
                [RenderTargetResource = (FTextureRenderTarget2DResource*)UnrealRenderTarget->GameThread_GetRenderTargetResource() , &MyTextureRHI = TextureRHI, RenderInitTask](FRHICommandListImmediate& RHICmdList)
                {
                    MyTextureRHI = RenderTargetResource->GetTextureRHI();
                    RenderInitTask->Trigger();
                }
            );

             // Audio init
            UnrealSoundBuffer->SetSampleRate(av.timing.sample_rate);
            UnrealSoundBuffer->NumChannels = 2;
            QueuedAudio = MakeShared<TCircularQueue<int32>, ESPMode::ThreadSafe>(UNREAL_LIBRETRO_AUDIO_BUFFER_SIZE);
            UnrealSoundBuffer->QueuedAudio = QueuedAudio;

        }, TStatId(), nullptr, ENamedThreads::GameThread);
        
        FTaskGraphInterface::Get().WaitUntilTaskCompletes(GameThreadMediaResourceInitTask);

        RenderInitTask->Wait();
        FPlatformProcess::ReturnSynchEventToPool(RenderInitTask);
    }

    // Let the core know that the audio device has been initialized.
    if (audio_callback.set_state) {
        audio_callback.set_state(true);
    }

    if (info.data)
        SDL_free((void*)info.data);

    SDL_RWclose(file);

    // Now that we have the system info, set the window title.
    if (UsingOpenGL) {
        char window_title[255];
        snprintf(window_title, sizeof(window_title), "sdlarch %s %s", system.library_name, system.library_version);
        SDL_SetWindowTitle(g_win, window_title);
    }
    
}

/**
 * cpu_features_get_time_usec:
 *
 * Gets time in microseconds.
 *
 * Returns: time in microseconds.
 **/


static retro_time_t cpu_features_get_time_usec(void) {
    return (retro_time_t)SDL_GetTicks();
}
#include <string.h>
LibretroContext::LibretroContext(TSharedRef<TStaticArray<FLibretroInputState, PortCount>, ESPMode::ThreadSafe> InputState) : UnrealInputState(InputState) {}

std::array<char, 260> LibretroContext::save_directory;
std::array<char, 260> LibretroContext::system_directory;

LibretroContext* LibretroContext::Launch(FString core, FString game, UTextureRenderTarget2D* RenderTarget, URawAudioSoundWave* SoundBuffer, TSharedPtr<TStaticArray<FLibretroInputState, PortCount>, ESPMode::ThreadSafe> InputState, TUniqueFunction
                                         <void(libretro_api_t&, bool)> LoadedCallback)
{

    check(IsInGameThread()); // So static initialization is safe

    static bool MemberStaticsInitialized = false;
    if (!MemberStaticsInitialized) 
    {
        auto InitDirectory = [] (auto &cstr, FString &&Path) {
            FString AbsolutePath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*Path);
            bool success = IFileManager::Get().MakeDirectory(*AbsolutePath, true); // This will block for a small amount of time probably
            check(success && cstr.size() > AbsolutePath.Len());
            auto RAII_ANSIString = StringCast<ANSICHAR>(*AbsolutePath);
            strcpy(cstr.data(), RAII_ANSIString.Get()); // @todo should probably just make the AbsolutePath a TArray
        };
        
        auto LibretroPluginRootPath = IPluginManager::Get().FindPlugin(TEXT("UnrealLibretro"))->GetBaseDir();
        InitDirectory(system_directory, FPaths::Combine(LibretroPluginRootPath, TEXT("System")));
        InitDirectory(save_directory, FPaths::Combine(LibretroPluginRootPath, TEXT("Saves"), TEXT("Core")));
        MemberStaticsInitialized = true;
    }

    static const uint32 MAX_INSTANCES = 100;
    static const uint32 MAX_INSTANCES_PER_CORE = 8*8;
    static FCriticalSection MultipleDLLInstanceHandlingLock;
    static TMap<FString, TBitArray<TInlineAllocator<MAX_INSTANCES_PER_CORE/8>>> PerCoreAllocatedInstances;
    static TBitArray<TInlineAllocator<(MAX_INSTANCES / 8) + 1>> AllocatedInstances(false, MAX_INSTANCES);
    
    // SDL is needed to get OpenGL contexts and windows from the OS in a sane way. I tried looking for an official Unreal way to do it, but I couldn't really find one SDL is so portable though it shouldn't matter
    
    const auto my_SDL_init = []() { // This is called in separate places depending on the platform
        if (SDL_Init(SDL_INIT_VIDEO) != 0) {
            UE_LOG(Libretro, Fatal, TEXT("Failed to initialize SDL"));
        }
    };
    
    #if !PLATFORM_APPLE
        my_SDL_init();
    #endif


    LibretroContext *l = new LibretroContext(InputState.ToSharedRef());

    l->UnrealRenderTarget = MakeWeakObjectPtr(RenderTarget);
    l->UnrealSoundBuffer  = MakeWeakObjectPtr(SoundBuffer );

    // Kick the initialization process off to another thread. It shouldn't be added to the Unreal task pool because those are too slow and my code relies on OpenGL state being thread local.
    // The Runnable system is the standard way for spawning and managing threads in Unreal. FThread looks enticing, but they removed anyway to detach threads since "it doesn't work as expected"
    l->UnrealThreadTask = FLambdaRunnable::RunLambdaOnBackGroundThread
    (
        [=, LoadedCallback = MoveTemp(LoadedCallback)]() {
            #if PLATFORM_APPLE
                dispatch_sync(dispatch_get_main_queue(), ^{ my_SDL_init(); }); // here so we don't block the game thread too long since this has to synchronize with the main thread every time.
            #endif
        	
            // Here I check that the same dll isn't loaded twice. If it is you won't obtain a new instance of the dll loaded into memory, instead all variables and function pointers will point to the original loaded dll
            // Luckily you can bypass this limitation by just making copies of that dll and loading those. Which I automate here.
            FString InstancedCorePath = core;
            int32 InstanceNumber = 0, CoreInstanceNumber = 0;
        	[&core, &InstancedCorePath, &InstanceNumber, &CoreInstanceNumber]()
            {
                {
                    FScopeLock ScopeLock(&MultipleDLLInstanceHandlingLock);

                    auto& CoreInstanceBitArray = PerCoreAllocatedInstances.FindOrAdd(core, TBitArray<TInlineAllocator<MAX_INSTANCES_PER_CORE / 8>>(false, MAX_INSTANCES_PER_CORE));
                    CoreInstanceNumber = CoreInstanceBitArray.FindAndSetFirstZeroBit();
                    InstanceNumber = AllocatedInstances.FindAndSetFirstZeroBit();
                    check(CoreInstanceNumber != INDEX_NONE && InstanceNumber != INDEX_NONE);
                }

                if (CoreInstanceNumber > 0) {
                    InstancedCorePath = FString::Printf(TEXT("%s%d.%s"), *FPaths::GetBaseFilename(*core, false), CoreInstanceNumber, *FPaths::GetExtension(*core));
                    bool success = IPlatformFile::GetPlatformPhysical().CopyFile(*InstancedCorePath, *core);
                    check(success || IPlatformFile::GetPlatformPhysical().FileExists(*InstancedCorePath));
                }
            }();

            l->callback_instance = func_wrap_table + InstanceNumber;

            l->g_video.hw.version_major = 4;
            l->g_video.hw.version_minor = 5;
            l->g_video.hw.context_type = RETRO_HW_CONTEXT_OPENGL_CORE;
            l->g_video.hw.context_reset = []() {};
            l->g_video.hw.context_destroy = []() {};


            // Loads the dll and its function pointers into g_retro
            l->core_load(TCHAR_TO_ANSI(*InstancedCorePath));

            // This does load the game but does many other things as well. If hardware rendering is needed it loads OpenGL resources from the OS and this also initializes the unreal engine resources for audio and video.
            l->core_load_game(TCHAR_TO_ANSI(*IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*game)));

            // Configure the player input devices.
            l->g_retro.set_controller_port_device(0, RETRO_DEVICE_JOYPAD);

            LoadedCallback(l->g_retro, l->g_video.hw.bottom_left_origin);

            uint64 frames = 0;
            auto   start = FDateTime::Now();
            while (l->running) {

                if (l->runloop_frame_time.callback) {
                    retro_time_t current = cpu_features_get_time_usec();
                    retro_time_t delta = current - l->runloop_frame_time_last;

                    if (!l->runloop_frame_time_last)
                        delta = l->runloop_frame_time.reference;
                    l->runloop_frame_time_last = current;
                    l->runloop_frame_time.callback(delta * 1000);
                }

                // Ask the core to emit the audio.
                if (l->audio_callback.callback) {
                    l->audio_callback.callback();
                }

                if (l->UsingOpenGL) { glBindFramebuffer(GL_FRAMEBUFFER, 0); } // @todo: leftover from sdlarch pretty sure this isn't needed remove after testing a few cores without it.
                

                // @todo My timing solution is a bit adhoc. I'm sure theres probably a better way.
                // Timing loop
                l->g_retro.run();
            	
                // Execute tasks from command queue
                // It's semantically significant that this is here. Since I hook in save state operations here it must necessarily come after run is called on the core
                TUniqueFunction<void(libretro_api_t&)> Task;
                while (l->LibretroAPITasks.Dequeue(Task)) {
                    Task(l->g_retro);
                }

                frames++;

                auto sleep = (frames / l->av.timing.fps) - (FDateTime::Now() - start).GetTotalSeconds();
                if (sleep < -(1 / l->av.timing.fps)) { // If over a frame behind don't try to catch up to the next frame
                    start = FDateTime::Now();
                    frames = 0;
                }

                FPlatformProcess::Sleep(FMath::Max(0.0, sleep));
                // End of Timing loop
            }


            if (l->g_retro.initialized)
                l->g_retro.deinit();

            if (l->g_retro.handle)
                FPlatformProcess::FreeDllHandle(l->g_retro.handle);
        	
            l->video_deinit();
            if (l->g_ctx) { SDL_GL_DeleteContext(l->g_ctx); }
            if (l->g_win) { SDL_DestroyWindow(l->g_win); }// @todo: In SDLarch's code SDL_Quit was here and that implicitly destroyed some things like windows. So I'm not sure if I'm exhaustively destroying everything that it destroyed yet. In order to fix this you could probably just run SDL_Quit here and step with the debugger to see all the stuff it destroys.

        	if (l->bgra_buffers[0] || l->bgra_buffers[1])
            {
                /*
                 From https://docs.unrealengine.com/en-US/Programming/Rendering/ParallelRendering/index.html#synchronization
                 
                 "It is also guaranteed that, regardless of how parallelization is configured, the order of submission of commands to the GPU is unchanged from the order the commands would have been submitted in a single-threaded renderer. This is required to ensure consistency and must be maintained if rendering code is changed."
                 */
                auto CopyToUnrealFramebufferFinished = TGraphTask<FNullGraphTask>::CreateTask().ConstructAndHold(TStatId(), ENamedThreads::AnyThread);
                
                ENQUEUE_RENDER_COMMAND(CopyToUnrealFramebufferFence)
                (
                    [CopyToUnrealFramebufferFinished](FRHICommandListImmediate& RHICmdList)
                    {
                        auto RHIThreadFence = RHICmdList.RHIThreadFence();
                    	if (RHIThreadFence)
                    	{
                    		if (!RHIThreadFence->AddSubsequent(CopyToUnrealFramebufferFinished))
                    		{
                                CopyToUnrealFramebufferFinished->Unlock();
                    		}
                    	}
                    }
                );

        		if (l->bgra_buffers[0])
        		{
                    FMemory::Free(l->bgra_buffers[0]);
        		}

        		if (l->bgra_buffers[1])
        		{
                    FMemory::Free(l->bgra_buffers[1]);
        		}
            }
            
            l->~LibretroContext(); // Partially implemented RAII so this implicitly releases some shared and unique pointers

            {
                FScopeLock scoped_lock(&MultipleDLLInstanceHandlingLock);

                AllocatedInstances[InstanceNumber] = false;
                PerCoreAllocatedInstances[core][CoreInstanceNumber] = false;
            }
        }
    );

    return l;
}

void LibretroContext::Shutdown(LibretroContext* Instance) 
{
    Instance->Pause(false);

    Instance->enqueue_audio.Store(false, EMemoryOrder::SequentiallyConsistent);
	// We enqueue the shutdown procedure as the final task since we want outstanding tasks to be executed first
    Instance->EnqueueTask([Instance](auto&&)
    {
        Instance->running.Store(false, EMemoryOrder::SequentiallyConsistent);
    });
    
}

void LibretroContext::Pause(bool ShouldPause) // @todo not safe implementation if you aquire locks which we do
{
    UnrealThreadTask->Thread->Suspend(ShouldPause);
}

void LibretroContext::EnqueueTask(TUniqueFunction<void(libretro_api_t&)> LibretroAPITask)
{
    check(IsInGameThread()); // LibretroAPITasks is a single producer single consumer queue
    LibretroAPITasks.Enqueue(MoveTemp(LibretroAPITask));
};
