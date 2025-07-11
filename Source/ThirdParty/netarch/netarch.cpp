// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include <stdbool.h>
#include <stdint.h>
#ifdef _WIN32
#define NOMINMAX  1
#endif

#define SAM2_IMPLEMENTATION
#define SAM2_ENABLE_LOGGING
#define SAM2_TEST
int g_log_level = 1; // Info

#define ULNET_IMPLEMENTATION
#define ULNET_IMGUI
#define ULNET_TEST_IMPLEMENTATION
#include "ulnet.h"
#include "sam2.h"
#include "miniz.h"

#define MAX_SAMPLE_SIZE ULNET_MAX_SAMPLE_SIZE

#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS
#include "NetImgui_Api.h"
#include "imgui.h"
#if !defined(NETARCH_NO_SDL)
#include "imgui_impl_sdl3.h"
#include "imgui_impl_opengl3.h"
#endif
#include "implot.h"

#define ZDICT_STATIC_LINKING_ONLY
#include "zdict.h"

#if !defined(NETARCH_NO_SDL)
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_assert.h>
#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_cpuinfo.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_loadso.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_gamepad.h>
#endif
#include "libretro.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#define PATH_SEPARATOR "\\"
#else
#include <dirent.h>
#define PATH_SEPARATOR "/"
#endif

#include <math.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#ifndef MAX_PATH
#define MAX_PATH 260
#endif


//portable_basename:
//  Returns a pointer to the last component of path (i.e., the filename).
//  Handles both Unix-style '/' and Windows-style '\' path separators.
//  Modifies the input string in place by inserting a null terminator
//  after stripping trailing separators.
//
//  If path is NULL or empty, returns "." (a static string).
//
//  Notes:
//    1) This function returns a pointer into the given string. Make sure
//       the lifetime of 'path' lasts as long as you need the basename.
//    2) If you need to preserve the original string, you should make a copy
//       of it before calling this function.
//
//Example usage:
//  char path[] = "C:\\folder\\subfolder\\filename.txt";
//  printf("%s\n", portable_basename(path)); // prints "filename.txt"
char* portable_basename(char *path)
{
    static const char *dot = ".";

    if (path == NULL || *path == '\0') {
        // Edge case: NULL or empty input
        return (char*)dot;
    }

    // 1. Strip trailing slashes/backslashes.
    //    We'll replace them with '\0' so they don't appear in the filename.
    size_t len = strlen(path);
    while (len > 0) {
        if (path[len - 1] == '/' || path[len - 1] == '\\') {
            path[--len] = '\0';
        } else {
            break;
        }
    }

    // If everything was trailing slashes, return ".".
    if (len == 0) {
        return (char*)dot;
    }

    // 2. Find the start of the filename component.
    //    We'll search for the last occurrence of either separator.
    char *last_slash = strrchr(path, '/');
    char *last_backslash = strrchr(path, '\\');
    char *last_sep = NULL;

    if (last_slash && last_backslash) {
        // Whichever separator appears later in the string is the "real" last separator
        last_sep = (last_slash > last_backslash) ? last_slash : last_backslash;
    } else if (last_slash) {
        last_sep = last_slash;
    } else if (last_backslash) {
        last_sep = last_backslash;
    } else {
        // No path separator found
        last_sep = NULL;
    }

    // 3. Return the filename portion (the part after the last separator).
    if (last_sep != NULL) {
        return last_sep + 1;
    }

    // If there's no separator at all, the entire input is the filename.
    return path;
}

// Hide OpenGL functions SDL declares with external linkage we just load all of them dynamically to support headless operation
#define glActiveTexture RENAMED_BY_NETARCH_CPP_glActiveTexture
#define glBindTexture RENAMED_BY_NETARCH_CPP_glBindTexture
#define glClear RENAMED_BY_NETARCH_CPP_glClear
#define glClearColor RENAMED_BY_NETARCH_CPP_glClearColor
#define glDeleteTextures RENAMED_BY_NETARCH_CPP_glDeleteTextures
#define glDisable RENAMED_BY_NETARCH_CPP_glDisable
#define glEnable RENAMED_BY_NETARCH_CPP_glEnable
#define glIsEnabled RENAMED_BY_NETARCH_CPP_glIsEnabled
#define glGenTextures RENAMED_BY_NETARCH_CPP_glGenTextures
#define glGetIntegerv RENAMED_BY_NETARCH_CPP_glGetIntegerv
#define glGetString RENAMED_BY_NETARCH_CPP_glGetString
#define glReadPixels RENAMED_BY_NETARCH_CPP_glReadPixels
#define glPixelStorei RENAMED_BY_NETARCH_CPP_glPixelStorei
#define glTexImage2D RENAMED_BY_NETARCH_CPP_glTexImage2D
#define glReadBuffer RENAMED_BY_NETARCH_CPP_glReadBuffer
#define glViewport RENAMED_BY_NETARCH_CPP_glViewport
#define glDrawArrays RENAMED_BY_NETARCH_CPP_glDrawArrays
#define glTexParameteri RENAMED_BY_NETARCH_CPP_glTexParameteri
#define glTexSubImage2D RENAMED_BY_NETARCH_CPP_glTexSubImage2D
#define glGetError RENAMED_BY_NETARCH_CPP_glGetError
#define glDrawBuffer RENAMED_BY_NETARCH_CPP_glDrawBuffer
#include <SDL3/SDL_opengl.h>
#undef glActiveTexture
#undef glBindTexture
#undef glClear
#undef glClearColor
#undef glDeleteTextures
#undef glDisable
#undef glEnable
#undef glIsEnabled
#undef glGenTextures
#undef glGetIntegerv
#undef glGetString
#undef glReadPixels
#undef glPixelStorei
#undef glTexImage2D
#undef glReadBuffer
#undef glViewport
#undef glDrawArrays
#undef glTexParameteri
#undef glTexSubImage2D
#undef glGetError
#undef glDrawBuffer

typedef void (APIENTRYP PFNGLBINDTEXTUREPROC)(GLenum target, GLuint texture);
typedef void (APIENTRYP PFNGLCLEARPROC)(GLbitfield mask);
typedef void (APIENTRYP PFNGLCLEARCOLORPROC)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
typedef void (APIENTRYP PFNGLDELETETEXTURESPROC)(GLsizei n, const GLuint *textures);
typedef void (APIENTRYP PFNGLENABLEPROC)(GLenum cap);
typedef void (APIENTRYP PFNGLGENTEXTURESPROC)(GLsizei n, GLuint *textures);
typedef void (APIENTRYP PFNGLGETINTEGERVPROC)(GLenum pname, GLint *data);
typedef const GLubyte * (APIENTRYP PFNGLGETSTRINGPROC)(GLenum name);
typedef void (APIENTRYP PFNGLREADPIXELSPROC)(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels);
typedef void (APIENTRYP PFNGLPIXELSTOREIPROC)(GLint pname, GLint param);
typedef void (APIENTRYP PFNGLTEXIMAGE2DPROC)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
typedef void (APIENTRYP PFNGLREADBUFFERPROC)(GLenum mode);
typedef void (APIENTRYP PFNGLGENVERTEXARRAYSPROC) (GLsizei n, GLuint *arrays);
typedef void (APIENTRYP PFNGLVIEWPORTPROC)(GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (APIENTRYP PFNGLDRAWARRAYSPROC)(GLenum mode, GLint first, GLsizei count);
typedef void (APIENTRYP PFNGLTEXPARAMETERIPROC)(GLenum target, GLenum pname, GLint param);
typedef void (APIENTRYP PFNGLTEXSUBIMAGE2DPROC)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);
typedef GLenum (APIENTRY* PFNGLGETERRORPROC) (void);
typedef void (APIENTRYP PFNGLDISABLEPROC) (GLenum cap);
typedef void (APIENTRYP PFNGLENABLEPROC) (GLenum cap);
typedef GLboolean (APIENTRYP PFNGLISENABLEDPROC) ( GLenum cap );
typedef void(APIENTRYP PFNGLGETUNIFORMIVPROC)(GLuint program, GLint location, GLint* params);
typedef void (APIENTRYP PFNGLDRAWBUFFERPROC) ( GLenum mode );
#define ENUM_GL_PROCEDURES(EnumMacro) \
        EnumMacro(PFNGLACTIVETEXTUREPROC, glActiveTexture) \
        EnumMacro(PFNGLBINDFRAMEBUFFERPROC, glBindFramebuffer) \
        EnumMacro(PFNGLBINDRENDERBUFFERPROC, glBindRenderbuffer) \
        EnumMacro(PFNGLBINDTEXTUREPROC, glBindTexture) \
        EnumMacro(PFNGLCHECKFRAMEBUFFERSTATUSPROC, glCheckFramebufferStatus) \
        EnumMacro(PFNGLCLEARPROC, glClear) \
        EnumMacro(PFNGLCLEARCOLORPROC, glClearColor) \
        EnumMacro(PFNGLDELETEBUFFERSPROC, glDeleteBuffers) \
        EnumMacro(PFNGLDELETETEXTURESPROC, glDeleteTextures) \
        EnumMacro(PFNGLDISABLEPROC, glDisable) \
        EnumMacro(PFNGLENABLEPROC, glEnable) \
        EnumMacro(PFNGLISENABLEDPROC, glIsEnabled) \
        EnumMacro(PFNGLFRAMEBUFFERRENDERBUFFERPROC, glFramebufferRenderbuffer) \
        EnumMacro(PFNGLFRAMEBUFFERTEXTURE2DPROC, glFramebufferTexture2D) \
        EnumMacro(PFNGLGENFRAMEBUFFERSPROC, glGenFramebuffers) \
        EnumMacro(PFNGLGENRENDERBUFFERSPROC, glGenRenderbuffers) \
        EnumMacro(PFNGLGENTEXTURESPROC, glGenTextures) \
        EnumMacro(PFNGLGETINTEGERVPROC, glGetIntegerv) \
        EnumMacro(PFNGLGETSTRINGPROC, glGetString) \
        EnumMacro(PFNGLREADPIXELSPROC, glReadPixels) \
        EnumMacro(PFNGLPIXELSTOREIPROC, glPixelStorei) \
        EnumMacro(PFNGLRENDERBUFFERSTORAGEPROC, glRenderbufferStorage) \
        EnumMacro(PFNGLTEXIMAGE2DPROC, glTexImage2D) \
        EnumMacro(PFNGLFENCESYNCPROC, glFenceSync) \
        EnumMacro(PFNGLDELETESYNCPROC, glDeleteSync) \
        EnumMacro(PFNGLCLIENTWAITSYNCPROC, glClientWaitSync) \
        EnumMacro(PFNGLGENBUFFERSPROC, glGenBuffers) \
        EnumMacro(PFNGLBINDBUFFERPROC, glBindBuffer) \
        EnumMacro(PFNGLMAPBUFFERRANGEPROC, glMapBufferRange) \
        EnumMacro(PFNGLUNMAPBUFFERPROC, glUnmapBuffer) \
        EnumMacro(PFNGLBUFFERDATAPROC, glBufferData) \
        EnumMacro(PFNGLREADBUFFERPROC, glReadBuffer) \
        EnumMacro(PFNGLCREATEPROGRAMPROC, glCreateProgram) \
        EnumMacro(PFNGLATTACHSHADERPROC, glAttachShader) \
        EnumMacro(PFNGLLINKPROGRAMPROC, glLinkProgram) \
        EnumMacro(PFNGLVALIDATEPROGRAMPROC, glValidateProgram) \
        EnumMacro(PFNGLGETPROGRAMIVPROC, glGetProgramiv) \
        EnumMacro(PFNGLGETPROGRAMINFOLOGPROC, glGetProgramInfoLog) \
        EnumMacro(PFNGLGETATTRIBLOCATIONPROC, glGetAttribLocation) \
        EnumMacro(PFNGLGETUNIFORMLOCATIONPROC, glGetUniformLocation) \
        EnumMacro(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays) \
        EnumMacro(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray) \
        EnumMacro(PFNGLUSEPROGRAMPROC, glUseProgram) \
        EnumMacro(PFNGLUNIFORM1IPROC, glUniform1i) \
        EnumMacro(PFNGLUNIFORMMATRIX4FVPROC, glUniformMatrix4fv) \
        EnumMacro(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer) \
        EnumMacro(PFNGLDISABLEVERTEXATTRIBARRAYPROC, glDisableVertexAttribArray) \
        EnumMacro(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray) \
        EnumMacro(PFNGLCREATESHADERPROC, glCreateShader) \
        EnumMacro(PFNGLSHADERSOURCEPROC, glShaderSource) \
        EnumMacro(PFNGLCOMPILESHADERPROC, glCompileShader) \
        EnumMacro(PFNGLGETSHADERIVPROC, glGetShaderiv) \
        EnumMacro(PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog) \
        EnumMacro(PFNGLDELETESHADERPROC, glDeleteShader) \
        EnumMacro(PFNGLDELETEFRAMEBUFFERSPROC, glDeleteFramebuffers) \
        EnumMacro(PFNGLDELETEVERTEXARRAYSPROC, glDeleteVertexArrays) \
        EnumMacro(PFNGLDELETEPROGRAMPROC, glDeleteProgram) \
        EnumMacro(PFNGLVIEWPORTPROC, glViewport) \
        EnumMacro(PFNGLDRAWARRAYSPROC, glDrawArrays) \
        EnumMacro(PFNGLTEXPARAMETERIPROC, glTexParameteri) \
        EnumMacro(PFNGLTEXSUBIMAGE2DPROC, glTexSubImage2D) \
        EnumMacro(PFNGLGETERRORPROC, glGetError) \
        EnumMacro(PFNGLGETUNIFORMIVPROC, glGetUniformiv) \
        EnumMacro(PFNGLDRAWBUFFERPROC, glDrawBuffer) \

#define DEFINE_GL_PROCEDURES(Type,Func) Type Func = NULL;
ENUM_GL_PROCEDURES(DEFINE_GL_PROCEDURES);

#if !defined(DEBUG_OPENGL_CALLBACK)
#define LogGLErrors(x) GLClearErrors();\
    x;\
    GLLogCall(#x, __FILE__, __LINE__)
#else
#define LogGLErrors(x) x
#endif

static void GLClearErrors() {
    /* loop while there are errors and until GL_NO_ERROR is returned */
    while (glGetError() != GL_NO_ERROR);
}

static bool GLLogCall(const char* function, const char* file, int line) {
    bool success = true;
    while (GLenum error = glGetError())
    {
        SAM2_LOG_ERROR("OpenGL: %s:%s:%d: GLenum (%d)", function, file, line, error);
        success = false;
    }
    return success;
}

static bool g_use_shared_context = false;
static SDL_Window *g_win = NULL;
static SDL_GLContext g_ctx = NULL;
static SDL_GLContext g_core_ctx = NULL;
static SDL_AudioStream *g_pcm = NULL;
static struct retro_frame_time_callback runloop_frame_time;
static retro_usec_t runloop_frame_time_last = 0;
static const bool *g_kbd = NULL;
static SDL_Gamepad *g_gamepad = NULL;
static SDL_JoystickID g_gamepad_instance_id = 0;
struct retro_system_av_info g_av = {0};
static struct retro_audio_callback audio_callback;

static float g_scale = 2.0f;
bool running = true;


static struct {
    GLuint tex_id;
    GLuint fbo_id;
    GLuint rbo_id;

    int glmajor;
    int glminor;


    GLuint pitch;
    GLint tex_w, tex_h;
    GLuint clip_w, clip_h;

    GLuint pixfmt;
    GLuint pixtype;
    GLuint bpp;

    struct retro_hw_render_callback hw;
} g_video  = {0};

static struct {
    GLuint vao;
    GLuint vbo;
    GLuint program;

    GLint i_pos;
    GLint i_coord;
    GLint u_tex;
    GLint u_mvp;

} g_shader = {0};

static struct retro_variable *g_vars = NULL; // Default options

static const char *g_vshader_src =
    "#version 150\n"
    "in vec2 i_pos;\n"
    "in vec2 i_coord;\n"
    "out vec2 o_coord;\n"
    "uniform mat4 u_mvp;\n"
    "void main() {\n"
        "o_coord = i_coord;\n"
        "gl_Position = vec4(i_pos, 0.0, 1.0) * u_mvp;\n"
    "}";

static const char *g_fshader_src =
    "#version 150\n"
    "in vec2 o_coord;\n"
    "uniform sampler2D u_tex;\n"
    "out vec4 outColor;\n"
    "void main() {\n"
        "outColor = texture(u_tex, o_coord);\n"
    "}";




static struct {
    SDL_SharedObject *handle;
    bool initialized;
    bool game_loaded;
    bool supports_no_game;
    uint64_t quirks;
    // The last performance counter registered. TODO: Make it a linked list.
    struct retro_perf_counter* perf_counter_last;

    void (*retro_init)(void);
    void (*retro_deinit)(void);
    unsigned (*retro_api_version)(void);
    void (*retro_get_system_info)(struct retro_system_info *info);
    void (*retro_get_system_av_info)(struct retro_system_av_info *info);
    void (*retro_set_controller_port_device)(unsigned port, unsigned device);
    void (*retro_reset)(void);
    void (*retro_run)(void);
    size_t (*retro_serialize_size)(void);
    bool (*retro_serialize)(void *data, size_t size);
    bool (*retro_unserialize)(const void *data, size_t size);
//  void retro_cheat_reset(void);
//  void retro_cheat_set(unsigned index, bool enabled, const char *code);
    bool (*retro_load_game)(const struct retro_game_info *game);
//  bool retro_load_game_special(unsigned game_type, const struct retro_game_info *info, size_t num_info);
    void (*retro_unload_game)(void);
//  unsigned retro_get_region(void);
//  void *retro_get_memory_data(unsigned id);
//  size_t retro_get_memory_size(unsigned id);
} g_retro;

#define UMETA(...)

enum ERetroDeviceID
{
    // RETRO_DEVICE_ID_JOYPAD
    JoypadB = 0,
    JoypadY,
    JoypadSelect,
    JoypadStart,
    JoypadUp,
    JoypadDown,
    JoypadLeft,
    JoypadRight,
    JoypadA,
    JoypadX,
    JoypadL,
    JoypadR,
    JoypadL2,
    JoypadR2,
    JoypadL3,
    JoypadR3,

    // RETRO_DEVICE_ID_LIGHTGUN
    LightgunX UMETA(Hidden), // The Lightgun entries marked UMETA(Hidden) here are deprecated according to libretro.h
    LightgunY UMETA(Hidden),
    LightgunTrigger,
    LightgunAuxA,
    LightgunAuxB,
    LightgunPause UMETA(Hidden),
    LightgunStart,
    LightgunSelect,
    LightgunAuxC,
    LightgunDpadUp,
    LightgunDpadDown,
    LightgunDpadLeft,
    LightgunDpadRight,
    LightgunScreenX,
    LightgunScreenY,
    LightgunIsOffscreen,
    LightgunReload,

    // RETRO_DEVICE_ID_ANALOG                                       (For triggers)
    // CartesianProduct(RETRO_DEVICE_ID_ANALOG, RETRO_DEVICE_INDEX) (For stick input)
    AnalogLeftX,
    AnalogLeftY,
    AnalogRightX,
    AnalogRightY,
    AnalogL2,
    AnalogR2,

    // RETRO_DEVICE_ID_POINTER
    PointerX,
    PointerY,
    PointerPressed,
    PointerCount,
    PointerX1         UMETA(Hidden),
    PointerY1         UMETA(Hidden),
    PointerPressed1   UMETA(Hidden),
    PointerCountVoid1 UMETA(Hidden),
    PointerX2         UMETA(Hidden),
    PointerY2         UMETA(Hidden),
    PointerPressed2   UMETA(Hidden),
    PointerCountVoid2 UMETA(Hidden),
    PointerX3         UMETA(Hidden),
    PointerY3         UMETA(Hidden),
    PointerPressed3   UMETA(Hidden),

    Size UMETA(Hidden),
};


struct FLibretroContext {

    sam2_socket_t sam2_socket = 0;
    ulnet_session_t ulnet_session = {0};
    struct retro_system_info system_info = {0};

    sam2_message_u message_history[2048];
    int message_history_length = 0;

    ulnet_input_state_t InputState[ULNET_PORT_COUNT] = {0};
    bool fuzz_input = false;

    int SAM2Send(char *message) {
        // Do some sanity checks on the request
        if (sam2_header_matches(message, sam2_sign_header)) {
            sam2_signal_message_t *signal_message = (sam2_signal_message_t *) message;
            assert(signal_message->peer_id > SAM2_PORT_SENTINELS_MAX);

            if (signal_message->peer_id == ulnet_session.our_peer_id) {
                SAM2_LOG_FATAL("We tried to signal ourself");
            }

            if (signal_message->peer_id == 0) {
                SAM2_LOG_FATAL("We tried to signal no one");
            }
        }

        int ret = sam2_client_send(sam2_socket, message);

        // Bookkeep all sent requests for debugging purposes
        if (message_history_length < SAM2_ARRAY_LENGTH(message_history)) {
            memcpy(&message_history[message_history_length++], message, sam2_get_metadata(message)->message_size);
        }

        return ret;
    }

    void AuthoritySendSaveState(juice_agent_t *agent);

    int16_t core_input_state(unsigned port, unsigned device, unsigned index, unsigned id);
    void core_input_poll();
};

static struct FLibretroContext g_libretro_context;
auto &g_ulnet_session = g_libretro_context.ulnet_session;
static bool g_headless = 0;
static int g_netimgui_port = 0;

struct keymap {
    unsigned k;
    unsigned rk;
};

static struct keymap g_binds[] = {
    { SDL_SCANCODE_X, JoypadA },
    { SDL_SCANCODE_Z, JoypadB },
    { SDL_SCANCODE_A, JoypadY },
    { SDL_SCANCODE_S, JoypadX },
    { SDL_SCANCODE_UP, JoypadUp },
    { SDL_SCANCODE_DOWN, JoypadDown },
    { SDL_SCANCODE_LEFT, JoypadLeft },
    { SDL_SCANCODE_RIGHT, JoypadRight },
    { SDL_SCANCODE_RETURN, JoypadStart },
    { SDL_SCANCODE_BACKSPACE, JoypadSelect },
    { SDL_SCANCODE_Q, JoypadL },
    { SDL_SCANCODE_W, JoypadR },
    { 0, 0 }
};

struct gamepad_map {
    SDL_GamepadButton button;
    unsigned retro_id;
};

static struct gamepad_map g_gamepad_binds[] = {
    { SDL_GAMEPAD_BUTTON_SOUTH, JoypadA },        // A/Cross
    { SDL_GAMEPAD_BUTTON_EAST, JoypadB },         // B/Circle
    { SDL_GAMEPAD_BUTTON_WEST, JoypadY },         // Y/Square
    { SDL_GAMEPAD_BUTTON_NORTH, JoypadX },        // X/Triangle
    { SDL_GAMEPAD_BUTTON_DPAD_UP, JoypadUp },
    { SDL_GAMEPAD_BUTTON_DPAD_DOWN, JoypadDown },
    { SDL_GAMEPAD_BUTTON_DPAD_LEFT, JoypadLeft },
    { SDL_GAMEPAD_BUTTON_DPAD_RIGHT, JoypadRight },
    { SDL_GAMEPAD_BUTTON_START, JoypadStart },
    { SDL_GAMEPAD_BUTTON_BACK, JoypadSelect },
    { SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, JoypadL },
    { SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, JoypadR },
    { SDL_GAMEPAD_BUTTON_LEFT_STICK, JoypadL3 },
    { SDL_GAMEPAD_BUTTON_RIGHT_STICK, JoypadR3 },
    { (SDL_GamepadButton)0, 0 }
};


#define load_sym(V, S) do {\
    if (!((*(SDL_FunctionPointer*)&V) = SDL_LoadFunction((SDL_SharedObject*)g_retro.handle, #S))) \
        SAM2_LOG_FATAL("Failed to load symbol '" #S "'': %s", SDL_GetError()); \
    } while (0)
#define load_retro_sym(S) load_sym(g_retro.S, S)

static void logical_partition(int sz, int redundant, int *n, int *out_k, int *packet_size, int *packet_groups);

// This is a little confusing since the lower byte of sequence corresponds to the largest stride
static int64_t logical_partition_offset_bytes(uint8_t sequence_hi, uint8_t sequence_lo, int block_size_bytes, int block_stride);

static GLuint compile_shader(unsigned type, unsigned count, const char **strings) {
    GLuint shader = glCreateShader(type);
    LogGLErrors(glShaderSource(shader, count, strings, NULL));
    LogGLErrors(glCompileShader(shader));

    GLint status;
    LogGLErrors(glGetShaderiv(shader, GL_COMPILE_STATUS, &status));

    if (status == GL_FALSE) {
        char buffer[4096];
        glGetShaderInfoLog(shader, sizeof(buffer), NULL, buffer);
        SAM2_LOG_FATAL("Failed to compile %s shader: %s", type == GL_VERTEX_SHADER ? "vertex" : "fragment", buffer);
    }

    return shader;
}

void ortho2d(float m[4][4], float left, float right, float bottom, float top) {
    m[0][0] = 1; m[0][1] = 0; m[0][2] = 0; m[0][3] = 0;
    m[1][0] = 0; m[1][1] = 1; m[1][2] = 0; m[1][3] = 0;
    m[2][0] = 0; m[2][1] = 0; m[2][2] = 1; m[2][3] = 0;
    m[3][0] = 0; m[3][1] = 0; m[3][2] = 0; m[3][3] = 1;

    m[0][0] = 2.0f / (right - left);
    m[1][1] = 2.0f / (top - bottom);
    m[2][2] = -1.0f;
    m[3][0] = -(right + left) / (right - left);
    m[3][1] = -(top + bottom) / (top - bottom);
}



static void init_shaders() {
    GLuint vshader = compile_shader(GL_VERTEX_SHADER, 1, &g_vshader_src);
    GLuint fshader = compile_shader(GL_FRAGMENT_SHADER, 1, &g_fshader_src);
    GLuint program = glCreateProgram();

    SDL_assert(program);

    LogGLErrors(glAttachShader(program, vshader));
    LogGLErrors(glAttachShader(program, fshader));
    LogGLErrors(glLinkProgram(program));

    LogGLErrors(glDeleteShader(vshader));
    LogGLErrors(glDeleteShader(fshader));

    LogGLErrors(glValidateProgram(program));

    GLint status;
    LogGLErrors(glGetProgramiv(program, GL_LINK_STATUS, &status));

    if(status == GL_FALSE) {
        char buffer[4096];
        glGetProgramInfoLog(program, sizeof(buffer), NULL, buffer);
        SAM2_LOG_FATAL("Failed to link shader program: %s", buffer);
    }

    g_shader.program = program;
    g_shader.i_pos   = glGetAttribLocation(program,  "i_pos");
    g_shader.i_coord = glGetAttribLocation(program,  "i_coord");
    g_shader.u_tex   = glGetUniformLocation(program, "u_tex");
    g_shader.u_mvp   = glGetUniformLocation(program, "u_mvp");

    LogGLErrors(glGenVertexArrays(1, &g_shader.vao));
    LogGLErrors(glGenBuffers(1, &g_shader.vbo));

    LogGLErrors(glUseProgram(g_shader.program));

    LogGLErrors(glUniform1i(g_shader.u_tex, 0));

    float m[4][4];
    if (g_video.hw.bottom_left_origin)
        ortho2d(m, -1, 1, 1, -1);
    else
        ortho2d(m, -1, 1, -1, 1);

    glUniformMatrix4fv(g_shader.u_mvp, 1, GL_FALSE, (float*)m);

    LogGLErrors(glUseProgram(0));
}


static void refresh_vertex_data() {
    SDL_assert(g_video.tex_w);
    SDL_assert(g_video.tex_h);
    SDL_assert(g_video.clip_w);
    SDL_assert(g_video.clip_h);

    float bottom = (float)g_video.clip_h / g_video.tex_h;
    float right  = (float)g_video.clip_w / g_video.tex_w;

    float vertex_data[] = {
        // pos, coord
        -1.0f, -1.0f, 0.0f,  bottom, // left-bottom
        -1.0f,  1.0f, 0.0f,  0.0f,   // left-top
         1.0f, -1.0f, right,  bottom,// right-bottom
         1.0f,  1.0f, right,  0.0f,  // right-top
    };

    LogGLErrors(glBindVertexArray(g_shader.vao));

    LogGLErrors(glBindBuffer(GL_ARRAY_BUFFER, g_shader.vbo));
    LogGLErrors(glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_data), vertex_data, GL_STREAM_DRAW));

    LogGLErrors(glEnableVertexAttribArray(g_shader.i_pos));
    LogGLErrors(glEnableVertexAttribArray(g_shader.i_coord));
    LogGLErrors(glVertexAttribPointer(g_shader.i_pos, 2, GL_FLOAT, GL_FALSE, sizeof(float)*4, 0));
    LogGLErrors(glVertexAttribPointer(g_shader.i_coord, 2, GL_FLOAT, GL_FALSE, sizeof(float)*4, (void*)(2 * sizeof(float))));

    LogGLErrors(glBindVertexArray(0));
    LogGLErrors(glBindBuffer(GL_ARRAY_BUFFER, 0));
}

static void init_framebuffer(int width, int height)
{
    LogGLErrors(glGenFramebuffers(1, &g_video.fbo_id));
    LogGLErrors(glBindFramebuffer(GL_FRAMEBUFFER, g_video.fbo_id));

    LogGLErrors(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_video.tex_id, 0));

    if (g_video.hw.depth && g_video.hw.stencil) {
        LogGLErrors(glGenRenderbuffers(1, &g_video.rbo_id));
        LogGLErrors(glBindRenderbuffer(GL_RENDERBUFFER, g_video.rbo_id));
        LogGLErrors(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height));

        LogGLErrors(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, g_video.rbo_id));
    } else if (g_video.hw.depth) {
        LogGLErrors(glGenRenderbuffers(1, &g_video.rbo_id));
        LogGLErrors(glBindRenderbuffer(GL_RENDERBUFFER, g_video.rbo_id));
        LogGLErrors(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height));

        LogGLErrors(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, g_video.rbo_id));
    }

    if (g_video.hw.depth || g_video.hw.stencil) {
        LogGLErrors(glBindRenderbuffer(GL_RENDERBUFFER, 0));
    }

    LogGLErrors(glBindRenderbuffer(GL_RENDERBUFFER, 0));

    SDL_assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

    LogGLErrors(glClearColor(0, 0, 0, 1));
    LogGLErrors(glClear(GL_COLOR_BUFFER_BIT));

    LogGLErrors(glBindFramebuffer(GL_FRAMEBUFFER, 0));
}


static void resize_cb(int w, int h) {
    LogGLErrors(glViewport(0, 0, w, h));
}


static void create_window(int width, int height) {
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

    if (g_video.hw.context_type == RETRO_HW_CONTEXT_OPENGL_CORE || g_video.hw.version_major >= 3) {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, g_video.hw.version_major);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, g_video.hw.version_minor);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
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
        SAM2_LOG_FATAL("Unsupported hw context %i. (only OPENGL, OPENGL_CORE and OPENGLES2 supported)", g_video.hw.context_type);
    }

    g_win = SDL_CreateWindow("netarch", width, height, SDL_WINDOW_OPENGL);

    if (!g_win)
        SAM2_LOG_FATAL("Failed to create window: %s", SDL_GetError());

    g_ctx = SDL_GL_CreateContext(g_win);

    SDL_GL_MakeCurrent(g_win, g_ctx);

    if (g_use_shared_context) {
        // g_ctx must be current here
        //SDL_GL_MakeCurrent(g_win, g_ctx);

        SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
        g_core_ctx = SDL_GL_CreateContext(g_win);
        SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 0);

        SDL_GL_MakeCurrent(g_win, g_ctx);

        if (!g_core_ctx) {
           SAM2_LOG_ERROR("Failed to create shared GL context: %s",
                          SDL_GetError());
        }
    }

    if (!g_ctx)
        SAM2_LOG_FATAL("Failed to create OpenGL context: %s", SDL_GetError());

    // Initialize all entry points.
    #define GET_GL_PROCEDURES(Type,Func) Func = (Type)SDL_GL_GetProcAddress(#Func);
    ENUM_GL_PROCEDURES(GET_GL_PROCEDURES);

    // Check that all of the entry points have been initialized.
    bool bFoundAllEntryPoints = true;
    #define CHECK_GL_PROCEDURES(Type,Func) if (Func == NULL) { bFoundAllEntryPoints = false; SAM2_LOG_ERROR("Failed to find entry point for %s", #Func); }
    ENUM_GL_PROCEDURES(CHECK_GL_PROCEDURES);

    if (!bFoundAllEntryPoints) {
        SAM2_LOG_FATAL("Failed to find all OpenGL entry points");
    }

    SAM2_LOG_DEBUG("GL_SHADING_LANGUAGE_VERSION: %s", glGetString(GL_SHADING_LANGUAGE_VERSION));
    SAM2_LOG_DEBUG("GL_VERSION: %s", glGetString(GL_VERSION));

    init_shaders();

    SDL_GL_SetSwapInterval(1);
    SDL_GL_SwapWindow(g_win); // make apitrace output nicer

    resize_cb(width, height);
}


static void resize_to_aspect(double ratio, int sw, int sh, int *dw, int *dh) {
    *dw = sw;
    *dh = sh;

    if (ratio <= 0)
        ratio = (double)sw / sh;

    if ((float)sw / sh < 1)
        *dw = round(*dh * ratio);
    else
        *dh = round(*dw / ratio);
}


static void video_configure(const struct retro_game_geometry *geom) {
    int nwidth, nheight;
    for (int counter_so_loop_exits = 0; counter_so_loop_exits < 4; counter_so_loop_exits++) {
        resize_to_aspect(geom->aspect_ratio, geom->base_width * 1, geom->base_height * 1, &nwidth, &nheight);
        nwidth = round(nwidth * g_scale);
        nheight = round(nheight * g_scale);

        if (nheight > 400) {
            break;
        } else {
            SAM2_LOG_INFO("The window is small so we are scaling it up");
            g_scale++;
        }
    }

    if (g_win == NULL && !g_headless) {
        create_window(nwidth, nheight);
    }

    if (g_use_shared_context) {
        SDL_GL_MakeCurrent(g_win, g_core_ctx);
    }
    if (g_video.tex_id) {
        LogGLErrors(glDeleteTextures(1, &g_video.tex_id));
        g_video.tex_id = 0;
    }

    if (!g_video.pixfmt) {
        g_video.pixfmt = GL_UNSIGNED_SHORT_5_5_5_1;
    }

    if (!g_headless) {
        if (!SDL_SetWindowSize(g_win, nwidth, nheight)) {
            SAM2_LOG_FATAL("Failed to set window size: %s", SDL_GetError());
        }

        LogGLErrors(glGenTextures(1, &g_video.tex_id));

        if (!g_video.tex_id) {
            SAM2_LOG_FATAL("Failed to create the video texture");
        }

        g_video.pitch = geom->max_width * g_video.bpp;

        LogGLErrors(glBindTexture(GL_TEXTURE_2D, g_video.tex_id));

        LogGLErrors(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
        LogGLErrors(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, geom->max_width, geom->max_height, 0,
                g_video.pixtype, g_video.pixfmt, NULL);

        LogGLErrors(glBindTexture(GL_TEXTURE_2D, 0));

        init_framebuffer(geom->max_width, geom->max_height);
    }

    g_video.tex_w = geom->max_width;
    g_video.tex_h = geom->max_height;
    g_video.clip_w = geom->base_width;
    g_video.clip_h = geom->base_height;

    if (g_video.hw.context_reset) {
        g_video.hw.context_reset();
    }

    if (g_use_shared_context) {
        SDL_GL_MakeCurrent(g_win, g_ctx);
    }

    if (!g_headless) {
        refresh_vertex_data();
    }
}


static bool video_set_pixel_format(unsigned format) {
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
        SAM2_LOG_FATAL("Unknown pixel type %u", format);
    }

    return true;
}

#define FORMAT_UNIT_COUNT_SIZE 64
double format_unit_count(double count, char *unit)
{
    // Make sure the postfix isn't too long
    if (strlen(unit) > 32) {
        strcpy(unit, "units");
    }

    static char postfix[FORMAT_UNIT_COUNT_SIZE];
    strcpy(postfix, unit);
    const char* prefixes[] = { "", "kilo", "mega", "giga" };
    const char* binary_prefixes[] = { "", "kibi", "mebi", "gibi" };
    int prefix_count = sizeof(prefixes) / sizeof(prefixes[0]);

    // Choose the correct set of prefixes and scaling factor based on the postfix
    const char** prefix_to_use = prefixes;
    double scale_factor = 1000.0;
    if (   memcmp(postfix, "bits", 4) == 0
        || memcmp(postfix, "bytes", 5) == 0) {
        prefix_to_use = binary_prefixes;
        scale_factor = 1024.0;
    }

    double display_count = (double)count;
    int prefix_index = 0;

    while (display_count >= scale_factor && prefix_index < prefix_count - 1) {
        display_count /= scale_factor;
        prefix_index++;
    }

    // Generate the unit string
    snprintf(unit, FORMAT_UNIT_COUNT_SIZE, "%s%s", prefix_to_use[prefix_index], postfix);

    return display_count;
}

int g_argc;
char **g_argv;
char g_rom_path[MAX_PATH];
static char g_core_path[MAX_PATH] = {0};
static bool g_core_needs_reload = false;
bool g_rom_needs_reload = false;
static sam2_room_t g_new_room_set_through_gui = {
    "My Room Name", 0, "VERSIONCORE", 0,
    { SAM2_PORT_UNAVAILABLE,   SAM2_PORT_AVAILABLE,   SAM2_PORT_AVAILABLE,   SAM2_PORT_AVAILABLE,
      SAM2_PORT_UNAVAILABLE, SAM2_PORT_UNAVAILABLE, SAM2_PORT_UNAVAILABLE, SAM2_PORT_UNAVAILABLE, SAM2_PORT_UNAVAILABLE }
};

#define MAX_ROOMS 1024
static sam2_room_t g_sam2_rooms[MAX_ROOMS];
static int64_t g_sam2_room_count = 0;
sam2_room_list_message_t last_sam2_room_list_response;
int64_t sam2_room_count;
sam2_room_t sam2_rooms[1024];

static sam2_server_t *g_sam2_server = NULL;
static char g_sam2_address[64] = "127.0.0.1"; //"sam2.cornbass.com";
static int g_sam2_port = SAM2_SERVER_DEFAULT_PORT;
static sam2_socket_t &g_sam2_socket = g_libretro_context.sam2_socket;

static int g_sample_size = 100;
static int g_zstd_compress_level = 0;
static uint64_t g_zstd_cycle_count[MAX_SAMPLE_SIZE] = {1}; // The 1 is so we don't divide by 0
static size_t g_zstd_compress_size[MAX_SAMPLE_SIZE] = {0};
static uint64_t g_reed_solomon_encode_cycle_count[MAX_SAMPLE_SIZE] = {0};
static float g_frame_time_milliseconds[MAX_SAMPLE_SIZE] = {0};
static float g_core_wants_tick_in_milliseconds[MAX_SAMPLE_SIZE] = {0};
static int64_t g_main_loop_cyclic_offset = 0;
static size_t g_serialize_size = 0;
static bool g_do_zstd_compress = true;
static bool g_do_zstd_delta_compress = false;
static bool g_use_rle = false;

int g_zstd_thread_count = 4;

size_t dictionary_size = 0;
unsigned char g_dictionary[256*1024];

static bool g_use_dictionary = false;
static bool g_dictionary_is_dirty = true;
static ZDICT_cover_params_t g_parameters = {0};

static int g_lost_packets = 0;

uint64_t g_remote_savestate_hash = 0x0; // 0x6AEBEEF1EDADD1E5;

#define MAX_SAVE_STATES 64
static unsigned char g_savebuffer[MAX_SAVE_STATES][20 * 1024 * 1024] = {0};

static int g_save_state_index = 0;
static int g_save_state_used_for_delta_index_offset = 1;

static bool g_is_refreshing_rooms = false;

static int g_volume = 0;
static bool g_vsync_enabled = true;

static ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
static bool g_connected_to_sam2 = false;
static sam2_error_message_t g_last_sam2_error = { SAM2_RESPONSE_SUCCESS };

static void peer_ids_to_string(uint16_t peer_ids[], char *output) {
    for (int i = 0; i < SAM2_PORT_MAX; i++) {
        switch(peer_ids[i]) {
            case SAM2_PORT_UNAVAILABLE:
                output[i] = 'U';
                break;
            case SAM2_PORT_AVAILABLE:
                output[i] = 'A';
                break;
            default:
                output[i] = 'P';
                break;
        }
    }

    output[SAM2_AUTHORITY_INDEX] = 'a';

    // Null terminate the string
    output[SAM2_AUTHORITY_INDEX + 1] = '\0';
}

static int read_whole_file(const char *filename, void **data, size_t *size) {
    SDL_IOStream *file = SDL_IOFromFile(filename, "rb");

    if (!file)
        SAM2_LOG_FATAL("Failed to load %s: %s", filename, SDL_GetError());

    int64_t ret = SDL_GetIOSize(file);

    if (ret < 0) {
        SAM2_LOG_FATAL("Failed to query file size: %s", SDL_GetError());
    } else {
        *size = ret;
    }

    *data = SDL_malloc(*size);

    if (!*data)
        SAM2_LOG_FATAL("Failed to allocate memory for the content");

    if (!SDL_ReadIO(file, *data, *size))
        SAM2_LOG_FATAL("Failed to read file data: %s", SDL_GetError());

    SDL_CloseIO(file);
    return 0;
}

// I feel like switching here shouldn't be necessary but I'm on bleeding edge code and this is needed @todo
namespace ImGuiJank {
    void NewFrame() {
        if (g_netimgui_port) {
            NetImgui::NewFrame();
        } else {
            ImGui::NewFrame();
        }
    }

    void EndFrame() {
        if (g_netimgui_port) {
            NetImgui::EndFrame();
        } else {
            ImGui::Render();
        }
    }
}

static void strip_last_path_component(char *path) {
    if (!path || !*path) return;

    size_t len = strlen(path);

    // Remove trailing slashes
    while (len > 0 && (path[len-1] == '/' || path[len-1] == '\\')) {
        path[--len] = '\0';
    }

    // Handle Windows root with drive letter (e.g., "C:\")
    #ifdef _WIN32
    if (len == 2 && path[1] == ':') {
        path[2] = '\\';
        path[3] = '\0';
        return;
    }
    #endif

    // Find last separator
    char *last_sep = NULL;
    for (char *p = path; *p; p++) {
        if (*p == '/' || *p == '\\') {
            last_sep = p;
        }
    }

    if (!last_sep) {
        // No separator found
        #ifdef _WIN32
        if (len >= 2 && path[1] == ':') {
            // Windows drive letter without separator
            path[2] = '\\';
            path[3] = '\0';
        } else {
            path[0] = '.';
            path[1] = '\0';
        }
        #else
        path[0] = '.';
        path[1] = '\0';
        #endif
    } else if (last_sep == path) {
        // Root directory
        path[1] = '\0';
    } else {
        *last_sep = '\0';
    }
}

#define NETARCH_CONTENT_FLAG_IS_DIRECTORY 0b00000001

static int list_folder_contents(
    char path_utf8[/*MAX_PATH*/],
    char content_name_utf8[/*content_capacity*/][MAX_PATH],
    int content_flag[/*content_capacity*/],
    int content_capacity
) {
    int count = 0;

    // Add trailing slash if needed
    size_t len = strlen(path_utf8);
    if (len > 0 && path_utf8[len-1] != '/' && path_utf8[len-1] != '\\') {
        strcat(path_utf8, PATH_SEPARATOR);
    }

    // Add parent directory unconditionally
    strncpy(content_name_utf8[0], "..", MAX_PATH - 1);
    content_name_utf8[0][MAX_PATH - 1] = '\0';
    content_flag[0] = NETARCH_CONTENT_FLAG_IS_DIRECTORY;
    count = 1;

#ifdef _WIN32
    WIN32_FIND_DATAA find_data;
    char search_path[MAX_PATH];
    snprintf(search_path, sizeof(search_path), "%s*", path_utf8);

    HANDLE find_handle = FindFirstFileA(search_path, &find_data);
    if (find_handle == INVALID_HANDLE_VALUE) {
        return -1;
    }

    do {
        if (count >= content_capacity) {
            break;
        }

        // Skip "." and ".." entries
        if (strcmp(find_data.cFileName, ".") == 0 ||
            strcmp(find_data.cFileName, "..") == 0) {
            continue;
        }

        strncpy(content_name_utf8[count], find_data.cFileName, MAX_PATH - 1);
        content_name_utf8[count][MAX_PATH - 1] = '\0';

        content_flag[count] = (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ?
            NETARCH_CONTENT_FLAG_IS_DIRECTORY : 0;

        count++;
    } while (FindNextFileA(find_handle, &find_data));

    FindClose(find_handle);

#else
    DIR* dir = opendir(path_utf8);
    if (!dir) {
        return -1;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (count >= content_capacity) {
            break;
        }

        // Skip "." and ".." entries
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        strncpy(content_name_utf8[count], entry->d_name, MAX_PATH - 1);
        content_name_utf8[count][MAX_PATH - 1] = '\0';

        content_flag[count] = (entry->d_type == DT_DIR) ?
            NETARCH_CONTENT_FLAG_IS_DIRECTORY : 0;

        count++;
    }

    closedir(dir);
#endif

    return count;
}

int imgui_file_picker(
    const char *str_id,
    char path_utf8[/*MAX_PATH*/],
    const char content_name_utf8[/*n*/][MAX_PATH],
    const int content_flag[/*n*/],
    int n
) {
    int selected_index = -1;

    // Show current path
    ImGui::Text("Path: %s", path_utf8);
    ImGui::Separator();

    // File list
    if (ImGui::BeginChild(str_id, ImVec2(0, 300), ImGuiWindowFlags_NoTitleBar)) {
        for (int i = 0; i < n; ++i) {
            const bool is_dir = content_flag[i] & NETARCH_CONTENT_FLAG_IS_DIRECTORY;
            const char* icon = is_dir ? "[D] " : "[F] ";

            char label[MAX_PATH + 10];
            SDL_snprintf(label, sizeof(label), "%s%s", icon, content_name_utf8[i]);

            ImGui::Selectable(label);

            if(ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                selected_index = i;

                size_t path_len = strlen(path_utf8);
                const char* selected_name = content_name_utf8[selected_index];

                if (strcmp(selected_name, "..") == 0) {
                    // Go up one directory
                    strip_last_path_component(path_utf8);
                } else {
                    // Append directory to path
                    if (path_len > 0 && path_utf8[path_len-1] != '/' && path_utf8[path_len-1] != '\\') {
                        strcat(path_utf8, PATH_SEPARATOR);
                    }
                    strcat(path_utf8, selected_name);
                }
            }
        }
    }
    ImGui::EndChild();

    return selected_index;
}

#include "imgui_internal.h"
void draw_imgui() {
    static int spinnerIndex = 0;
    char spinnerFrames[4] = { '|', '/', '-', '\\' };
    char spinnerGlyph = spinnerFrames[(spinnerIndex++/4)%4];
    const ImVec4 WHITE(1.0f, 1.0f, 1.0f, 1.0f);
    const ImVec4 GREY(0.5f, 0.5f, 0.5f, 1.0f);
    const ImVec4 GOLD(1.0f, 0.843f, 0.0f, 1.0f);
    const ImVec4 RED(1.0f, 0.0f, 0.0f, 1.0f);

    ImGuiIO& io = ImGui::GetIO(); (void)io;
    static bool show_demo_window = false;


    auto show_message = [=](char *message) {
        ImGui::Text("Header: %.8s", (char *) message);

        if (sam2_header_matches(message, sam2_sign_header)) {
            sam2_signal_message_t *signal_message = (sam2_signal_message_t *) message;
            ImGui::Text("Peer ID: %05" PRId16, signal_message->peer_id);
            ImGui::InputTextMultiline("ICE SDP", signal_message->ice_sdp, sizeof(signal_message->ice_sdp), ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 16), ImGuiInputTextFlags_ReadOnly);
        } else if (sam2_header_matches(message, sam2_make_header)) {
            sam2_room_make_message_t *make_message = (sam2_room_make_message_t *) message;
            ImGui::Separator();
            ulnet_imgui_show_room(make_message->room, g_ulnet_session.our_peer_id);
        } else if (sam2_header_matches(message, sam2_list_header)) {
            if (message[7] == 'r') {
                // Request
                ImGui::Text("Room List Request");
            } else {
                // Response
                sam2_room_list_message_t *list_response = (sam2_room_list_message_t *) message;
                ImGui::Separator();
                ulnet_imgui_show_room(list_response->room, g_ulnet_session.our_peer_id);
            }
        } else if (sam2_header_matches(message, sam2_join_header)) {
            sam2_room_join_message_t *join_message = (sam2_room_join_message_t *) message;
            ImGui::Text("Peer ID: %05" PRId16, (uint16_t) join_message->peer_id);
            ImGui::Separator();
            ulnet_imgui_show_room(join_message->room, g_ulnet_session.our_peer_id);
        } else if (sam2_header_matches(message, sam2_conn_header)) {
            sam2_connect_message_t *connect_message = (sam2_connect_message_t *) message;
            ImGui::Text("Peer ID: %05" PRId16, connect_message->peer_id);
        } else if (sam2_header_matches(message, sam2_fail_header)) {
            sam2_error_message_t *error_response = (sam2_error_message_t *) message;
            ImGui::Text("Code: %" PRId64, error_response->code);
            ImGui::Text("Description: %s", error_response->description);
            ImGui::Text("Peer ID: %05" PRId16, error_response->peer_id);
        }
    };

    // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
    if (show_demo_window)
        ImGui::ShowDemoWindow(&show_demo_window);

    // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
    {
        char unit[FORMAT_UNIT_COUNT_SIZE] = {0};

        ImGui::Begin("Compression investigation");

        ImGui::SliderInt("Volume", &g_volume, 0, 100);

        double avg_cycle_count = 0;
        double avg_zstd_compress_size = 0;
        double max_compress_size = 0;
        double avg_zstd_cycle_count = 0;

        for (int i = 0; i < g_sample_size; i++) {
            avg_cycle_count += g_ulnet_session.save_state_execution_time_cycles[i];
            avg_zstd_compress_size += g_zstd_compress_size[i];
            avg_zstd_cycle_count += g_zstd_cycle_count[i];
            if (g_zstd_compress_size[i] > max_compress_size) {
                max_compress_size = g_zstd_compress_size[i];
            }
        }
        avg_cycle_count        /= g_sample_size;
        avg_zstd_compress_size /= g_sample_size;
        avg_zstd_cycle_count   /= g_sample_size;

        strcpy(unit, "cycles");
        double display_count = format_unit_count(avg_cycle_count, unit);
        ImGui::Text("retro_serialize average cycle count: %.2f %s", display_count, unit);
        ImGui::Checkbox("Compress serialized data with zstd", &g_do_zstd_compress);
        if (g_do_zstd_compress) {
            const char *algorithm_name = g_use_rle ? "rle" : "zstd";
            ImGui::Checkbox("Use RLE", &g_use_rle);
            ImGui::Checkbox("Delta Compression", &g_do_zstd_delta_compress);
            ImGui::Checkbox("Use Dictionary", &g_use_dictionary);
            if (g_use_dictionary) {
                unsigned k_min = 16;
                unsigned k_max = 2048;

                unsigned d_min = 6;
                unsigned d_max = 16;

                g_dictionary_is_dirty |= ImGui::SliderScalar("k", ImGuiDataType_U32, &g_parameters.k, &k_min, &k_max);
                g_dictionary_is_dirty |= ImGui::SliderScalar("d", ImGuiDataType_U32, &g_parameters.d, &d_min, &d_max);
            }

            strcpy(unit, "bits");
            display_count = format_unit_count(8 * avg_zstd_compress_size, unit);
            ImGui::Text("%s compression average size: %.2f %s", algorithm_name, display_count, unit);

            // Show compression max size
            strcpy(unit, "bits");
            display_count = format_unit_count(8 * max_compress_size, unit);
            ImGui::Text("%s compression max size: %.2f %s", algorithm_name, display_count, unit);

            strcpy(unit, "bytes/cycle");
            display_count = format_unit_count(g_serialize_size / avg_zstd_cycle_count, unit);
            ImGui::Text("%s compression average speed: %.2f %s", algorithm_name, display_count, unit);

            ImGui::Text("Remote Savestate hash: %016" PRIx64 "", g_remote_savestate_hash);
        }

        ImGui::SliderInt("Sample size", &g_sample_size, 1, MAX_SAMPLE_SIZE);
        if (!g_use_rle) {
            g_dictionary_is_dirty |= ImGui::SliderInt("Compression level", (int*)&g_zstd_compress_level, -22, 22);
            g_parameters.zParams.compressionLevel = g_zstd_compress_level;
        }

        { // Show a graph of one of the data sets
            // Add a combo box for buffer selection
            static const char* items[] = {"save_cycle_count", "cycle_count", "compress_size"};
            static int current_item = 0;  // default selection
            ImGui::Combo("Buffers", &current_item, items, IM_ARRAYSIZE(items));

            // Create a temporary array to hold float values for plotting.
            float temp[MAX_SAMPLE_SIZE];

            // Based on the selection, copy data to the temp array and draw the graph for the corresponding buffer.
            if (current_item == 0) {
                for (int i = 0; i < g_sample_size; ++i) {
                    temp[i] = static_cast<float>(g_ulnet_session.save_state_execution_time_cycles[(i+g_ulnet_session.frame_counter)%g_sample_size]);
                }
            } else if (current_item == 1) {
                for (int i = 0; i < g_sample_size; ++i) {
                    temp[i] = static_cast<float>(g_zstd_cycle_count[(i+g_ulnet_session.frame_counter)%g_sample_size]);
                }
            } else if (current_item == 2) {
                for (int i = 0; i < g_sample_size; ++i) {
                    temp[i] = static_cast<float>(g_zstd_compress_size[(i+g_ulnet_session.frame_counter)%g_sample_size]);
                }
            }
        }

        // Slider to select the current save state index
        ImGui::SliderInt("Save State Index (saved every frame)", &g_save_state_index, 0, MAX_SAVE_STATES-1);
        ImGui::SliderInt("Delta compression frame offset", &g_save_state_used_for_delta_index_offset, 0, MAX_SAVE_STATES-1);

        // Compression comparison plot
        static bool show_compression_plot = false;
        ImGui::Checkbox("Show Compression Comparison Plot", &show_compression_plot);

        if (show_compression_plot && ImPlot::BeginPlot("Compression Throughput vs Size", ImVec2(-1, 400), ImPlotFlags_None)) {
            ImPlot::SetupAxis(ImAxis_X1, "Compressed Size (KB)");
            ImPlot::SetupAxis(ImAxis_Y1, "Throughput", ImPlotAxisFlags_None);
            ImPlot::SetupAxisScale(ImAxis_Y1, ImPlotScale_Log10);

            // Test different compression levels for both algorithms
            int miniz_levels[] = { 1, 2, 3, MZ_DEFAULT_LEVEL /* 6 */, /* MZ_UBER_COMPRESSION  10 */ };
            int zstd_levels[] = { -1, 0, ZSTD_CLEVEL_DEFAULT, 6, 9, 12, /* ZSTD_maxCLevel() 22 (currently) */ };
            constexpr int miniz_levels_count = sizeof(miniz_levels) / sizeof(miniz_levels[0]);
            constexpr int zstd_levels_count = sizeof(zstd_levels) / sizeof(zstd_levels[0]);

            double miniz_sizes[miniz_levels_count] = {0};
            double miniz_throughputs[miniz_levels_count] = {0};
            double zstd_sizes[zstd_levels_count] = {0};
            double zstd_throughputs[zstd_levels_count] = {0};

            // Track min/max values for setting plot limits
            double min_throughput = DBL_MAX;
            double max_throughput = 0.0;
            double min_size = DBL_MAX;
            double max_size = 0.0;

            // Test miniz at different compression levels (level 0 is no compression)
            uint8_t* compressed_buffer = NULL;
            for (int i = 0; i < miniz_levels_count; i++) {
                // Compress with miniz
                uLongf compressed_size = compressBound(g_serialize_size);
                compressed_buffer = (uint8_t*)realloc(compressed_buffer, compressed_size);

                if (compressed_buffer) {
                    uint64_t start_cycles = ulnet__rdtsc();
                    int result = compress2(compressed_buffer, &compressed_size,
                                        (const Bytef*)g_savebuffer[g_save_state_index],
                                        g_serialize_size, miniz_levels[i]);
                    uint64_t end_cycles = ulnet__rdtsc();

                    if (result == Z_OK) {
                        double compression_time_cycles = end_cycles - start_cycles;
                        double throughput_bytes_per_cycle = g_serialize_size / compression_time_cycles;

                        miniz_sizes[i] = compressed_size / 1024.0;
                        miniz_throughputs[i] = throughput_bytes_per_cycle;

                        // Update bounds
                        min_throughput = fmin(min_throughput, throughput_bytes_per_cycle);
                        max_throughput = fmax(max_throughput, throughput_bytes_per_cycle);
                        min_size = fmin(min_size, miniz_sizes[i]);
                        max_size = fmax(max_size, miniz_sizes[i]);
                    }
                }
            }

            for (int i = 0; i < zstd_levels_count; i++) {
                size_t compressed_size = ZSTD_compressBound(g_serialize_size);
                compressed_buffer = (uint8_t*)realloc(compressed_buffer, compressed_size);

                if (compressed_buffer) {
                    uint64_t start_cycles = ulnet__rdtsc();
                    compressed_size = ZSTD_compress(compressed_buffer, compressed_size,
                                                g_savebuffer[g_save_state_index], g_serialize_size,
                                                zstd_levels[i]);
                    uint64_t end_cycles = ulnet__rdtsc();

                    if (!ZSTD_isError(compressed_size)) {
                        double compression_time_cycles = end_cycles - start_cycles;
                        double throughput_bytes_per_cycle = g_serialize_size / compression_time_cycles;

                        zstd_sizes[i] = compressed_size / 1024.0;
                        zstd_throughputs[i] = throughput_bytes_per_cycle;

                        // Update bounds
                        min_throughput = fmin(min_throughput, throughput_bytes_per_cycle);
                        max_throughput = fmax(max_throughput, throughput_bytes_per_cycle);
                        min_size = fmin(min_size, zstd_sizes[i]);
                        max_size = fmax(max_size, zstd_sizes[i]);
                    }
                }
            }
            free(compressed_buffer);

            // Set plot limits with some padding
            if (min_throughput < DBL_MAX && max_throughput > 0) {
                // For log scale, expand the range slightly in log space
                double log_min = log10(min_throughput);
                double log_max = log10(max_throughput);
                double log_padding = (log_max - log_min) * 0.1;
                ImPlot::SetupAxisLimits(ImAxis_Y1, pow(10, log_min - log_padding), pow(10, log_max + log_padding), ImPlotCond_Always);
            }
            if (min_size < DBL_MAX && max_size > 0) {
                double size_padding = (max_size - min_size) * 0.1;
                ImPlot::SetupAxisLimits(ImAxis_X1, min_size - size_padding, max_size + size_padding, ImPlotCond_Always);
            }

            ImPlot::SetupAxisFormat(ImAxis_Y1, [](double value, char* buff, int size, void* user_data) -> int {
                char unit[FORMAT_UNIT_COUNT_SIZE] = {0};
                double display_value = format_unit_count(value, strcpy(unit, "bytes/cycle"));
                return snprintf(buff, size, "%.2f %s", display_value, unit);
            }, nullptr);

            ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle);

            const char *miniz_label = "miniz " MZ_VERSION;
            ImPlot::PlotScatter(miniz_label, miniz_sizes, miniz_throughputs, miniz_levels_count);
            ImPlot::PlotLine(miniz_label, miniz_sizes, miniz_throughputs, miniz_levels_count);

            const char *zstd_label = "zstd " ZSTD_VERSION_STRING;
            ImPlot::SetNextMarkerStyle(ImPlotMarker_Square);
            ImPlot::PlotScatter(zstd_label, zstd_sizes, zstd_throughputs, zstd_levels_count);
            ImPlot::PlotLine(zstd_label, zstd_sizes, zstd_throughputs, zstd_levels_count);

            for (int i = 0; i < miniz_levels_count; i++) {
                if (miniz_levels[i] == MZ_DEFAULT_LEVEL) {
                    // Plot a larger marker at the default level
                    ImPlot::PushStyleVar(ImPlotStyleVar_MarkerSize, 5);
                    ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle, -1);
                    ImPlot::PlotScatter("##miniz_default", &miniz_sizes[i], &miniz_throughputs[i], 1);
                    ImPlot::PopStyleVar();

                    // Add text annotation
                    ImPlot::Annotation(miniz_sizes[i], miniz_throughputs[i],
                                    ImVec4(1,1,1,1), ImVec2(10, -10), true, "level=MZ_DEFAULT_LEVEL");
                }
            }

            for (int i = 0; i < zstd_levels_count; i++) {
                if (zstd_levels[i] == 0) {
                    // Plot a larger marker at the default level
                    ImPlot::PushStyleVar(ImPlotStyleVar_MarkerSize, 10);
                    ImPlot::SetNextMarkerStyle(ImPlotMarker_Square, -1, ImVec4(0,0,1,1), 2); // Blue outline
                    ImPlot::PlotScatter("##zstd_default", &zstd_sizes[i], &zstd_throughputs[i], 1);
                    ImPlot::PopStyleVar();

                    // Add text annotation
                    ImPlot::Annotation(zstd_sizes[i], zstd_throughputs[i],
                                    ImVec4(1,1,1,1), ImVec2(10, 10), true, "level=%d", zstd_levels[i]);
                }
            }

            ImPlot::EndPlot();
        }

        ImGui::End();
    }

    {
        ImGui::Begin("Signaling Server and a Match Maker", NULL, ImGuiWindowFlags_AlwaysAutoResize);

        if (g_sam2_server) {
            ImGui::SeparatorText("Server");
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "We're listening on %s:%d (IPv4 tunneling is OS dependent)", g_sam2_address, g_sam2_port);

            int room_count = 0;
            bool room_header_is_open = ImGui::CollapsingHeader("Rooms");
            for (uint16_t peer_id = g_sam2_server->peer_id_pool.used_list; peer_id != SAM2__INDEX_NULL; peer_id = g_sam2_server->peer_id_pool_node[peer_id].next) {
                if (g_sam2_server->rooms[peer_id].flags & SAM2_FLAG_ROOM_IS_NETWORK_HOSTED) {
                    if (room_header_is_open) {
                        ulnet_imgui_show_room(g_sam2_server->rooms[peer_id], g_ulnet_session.our_peer_id);
                    }
                    room_count++;
                }
            }

            ImGui::Text("Clients connected: %d", g_sam2_server->client_pool.used);
            ImGui::Text("Rooms Hosted: %d", room_count);


            ImGui::SeparatorText("Client");
        }

        if (g_connected_to_sam2) {
            bool is_ipv6 = false;
            for (int i = 0; g_sam2_address[i] != '\0'; i++) {
                if (g_sam2_address[i] == ':') {
                    is_ipv6 = true;
                    break;
                }
            }

            ImGui::AlignTextToFramePadding();
            if (is_ipv6) ImGui::TextColored(ImVec4(0, 1, 0, 1), "Connected to [" "%s" "]:%d", g_sam2_address, g_sam2_port);
            else         ImGui::TextColored(ImVec4(0, 1, 0, 1), "Connected to "  "%s"  ":%d", g_sam2_address, g_sam2_port);

            ImGui::SameLine();
            ImGui::TextColored(GOLD, "Our Peer ID");

            static uint16_t editablePeerId = 0;
            // First time initialization
            if (editablePeerId == 0 && g_ulnet_session.our_peer_id != 0) {
                editablePeerId = g_ulnet_session.our_peer_id;
            }

            // Check that our peer id is in sync with the server
            if (editablePeerId != g_ulnet_session.our_peer_id) {
                ImGui::SameLine();
                ImGui::TextColored(GREY, "%c", spinnerGlyph);
            }

            ImGui::SameLine();
            ImGui::PushItemWidth(40);
            ImGui::PushStyleColor(ImGuiCol_Text, GOLD);
            ImGui::InputScalar("##PeerIdInput", ImGuiDataType_U16, &editablePeerId, NULL, NULL, "%u", 0);
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                if (editablePeerId != g_ulnet_session.our_peer_id) {
                    sam2_connect_message_t message = { SAM2_CONN_HEADER };
                    message.peer_id = editablePeerId;

                    g_libretro_context.SAM2Send((char *) &message);
                }
            }
            ImGui::PopItemWidth();
            ImGui::PopStyleColor();

            ImGui::SameLine();
            if (ImGui::Button("Disconnect")) {
                if (sam2_client_disconnect(g_libretro_context.sam2_socket)) {
                    SAM2_LOG_FATAL("Couldn't disconnect socket");
                }
                g_libretro_context.sam2_socket = SAM2_SOCKET_INVALID;
            }
        } else {
            if (g_libretro_context.sam2_socket == SAM2_SOCKET_INVALID) {
                char port_str[64]; // Buffer to store the input text
                snprintf(port_str, sizeof(port_str), "%d", g_sam2_port);
                bool connect = false;

                connect |= ImGui::InputText("##input_address", g_sam2_address, sizeof(g_sam2_address), ImGuiInputTextFlags_EnterReturnsTrue); ImGui::SameLine();
                connect |= ImGui::InputText("##input_port", port_str, sizeof(port_str), ImGuiInputTextFlags_EnterReturnsTrue); ImGui::SameLine();
                connect |= ImGui::Button("Connect");

                if (atoi(port_str) > 0 && atoi(port_str) < 65536) {
                    g_sam2_port = atoi(port_str);
                }

                if (connect) {
                    sam2_client_connect(&g_libretro_context.sam2_socket, g_sam2_address, g_sam2_port);
                }
            } else {
                ImGui::TextColored(ImVec4(0.5, 0.5, 0.5, 1), "Connecting to %s:%d %c", g_sam2_address, g_sam2_port, spinnerGlyph);
                ImGui::SameLine();
                if (ImGui::Button("Stop")) {
                    if (sam2_client_disconnect(g_libretro_context.sam2_socket)) {
                        SAM2_LOG_FATAL("Couldn't disconnect socket");
                    }
                    g_libretro_context.sam2_socket = SAM2_SOCKET_INVALID;
                }
            }
        }

        if (!g_connected_to_sam2) goto finished_drawing_sam2_interface; // We're not connected, so don't draw the rest of the interface

        if (g_last_sam2_error.code) {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "Last error: %s", g_last_sam2_error.description);
            ImGui::SameLine();
            if (ImGui::Button("Clear")) {
                g_last_sam2_error.code = 0;
            }
        }

        static bool isWindowOpen = false;
        static int selected_message_index = 0;

        char header_label[32];
        snprintf(header_label, sizeof(header_label), "Messages (%d)", g_libretro_context.message_history_length);
        if (ImGui::CollapsingHeader(header_label)) {
            if (ImGui::BeginTable("MessagesTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupColumn("Header", ImGuiTableColumnFlags_WidthFixed, 150.0f);
                ImGui::TableSetupColumn("Origin", ImGuiTableColumnFlags_WidthFixed, 100.0f);
                ImGui::TableHeadersRow();

                for (int i = 0; i < g_libretro_context.message_history_length; ++i) {
                    sam2_message_u *message = &g_libretro_context.message_history[i];
                    ImGui::TableNextRow();

                    // Header column
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%.8s", (char *) message);
                    ImGui::SameLine();
                    char button_label[64] = {0};
                    snprintf(button_label, sizeof(button_label), "Show##%d", i);

                    if (ImGui::Button(button_label)) {
                        selected_message_index = i;
                        isWindowOpen = true;
                    }

                    // Origin column
                    ImGui::TableSetColumnIndex(1);
                    const char* origin = "Unknown";
                    char header[9] = {0};
                    SDL_strlcpy(header, (char*)message, 8);

                    if (header[7] == 'r') {
                        ImGui::TextColored(GOLD, "Peer %05" PRIu16, g_ulnet_session.our_peer_id);
                    } else {
                        uint16_t peer_id = 0;
                        if (strncmp(header, sam2_make_header, 4) == 0 ||
                            strncmp(header, sam2_list_header, 4) == 0 ||
                            strncmp(header, sam2_fail_header, 4) == 0 ||
                            strncmp(header, sam2_conn_header, 4) == 0) {
                            origin = g_sam2_address;
                        } else if (strncmp(header, sam2_join_header, 4) == 0) {
                            peer_id = ((sam2_room_join_message_t*)message)->peer_id;
                        } else if (strncmp(header, sam2_sign_header, 4) == 0) {
                            peer_id = ((sam2_signal_message_t*)message)->peer_id;
                        }

                        if (peer_id == 0) {
                            origin = g_sam2_address;
                        } else {
                            static char peer_id_str[16];
                            snprintf(peer_id_str, sizeof(peer_id_str), "Peer %05" PRIu16, peer_id);
                            origin = peer_id_str;
                        }

                        ImGui::Text("%s", origin);
                    }
                }

                ImGui::EndTable();
            }
        }

        if (ImGui::CollapsingHeader("Tests")) {
            if (ImGui::Button("Stress test message framing")) {
                for (int i = 0; i < 1000; ++i) {
                    sam2_room_make_message_t message = { SAM2_MAKE_HEADER };
                    message.room.peer_ids[SAM2_AUTHORITY_INDEX] = g_ulnet_session.our_peer_id;
                    message.room.flags |= SAM2_FLAG_ROOM_IS_NETWORK_HOSTED;
                    snprintf((char *) &message.room.name, sizeof(message.room.name), "Test message %d", i);
                    sam2_client_send(g_libretro_context.sam2_socket, (char *) &message);
                }
            }

            static ulnet_session_t *test_session1 = NULL;
            static ulnet_session_t *test_session2 = NULL;
            static bool show_test_sessions = false;

            if (ImGui::Button("Run ICE connection tests")) {
                SAM2_LOG_INFO("\033[1;32m=========================================");
                SAM2_LOG_INFO("\033[1;32m====== Running ulnet ICE tests ==========");
                SAM2_LOG_INFO("\033[1;32m=========================================");
                if (ulnet_test_ice(&test_session1, &test_session2)) {
                    SAM2_LOG_ERROR("ICE tests failed");
                    show_test_sessions = true;
                } else {
                    SAM2_LOG_INFO("ICE tests passed");
                    show_test_sessions = true;
                }
            }

            if (ImGui::Button("Run ulnet in-process tests")) {
                SAM2_LOG_INFO("\033[1;32m=========================================");
                SAM2_LOG_INFO("\033[1;32m====== Running ulnet in-process tests ===");
                SAM2_LOG_INFO("\033[1;32m=========================================");
                if (ulnet_test_inproc(&test_session1, &test_session2)) {
                    SAM2_LOG_ERROR("In process tests failed");
                    show_test_sessions = true;
                } else {
                    SAM2_LOG_INFO("In process tests passed");
                }
            }

            if (ImGui::Button("Show sessions") && (test_session1 || test_session2)) {
                show_test_sessions = !show_test_sessions;
            }

            // Only show the sessions window if we have sessions and the show flag is true
            if (show_test_sessions && (test_session1 || test_session2)) {
                ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.8f, 0.1f, 0.1f, 1.0f));
                ImGui::Begin("Failed Test");
                if (ImGui::BeginTabBar("SessionTabs")) {
                    if (test_session1 && ImGui::BeginTabItem("Session 1")) {
                        ulnet_imgui_show_session(test_session1);
                        ImGui::EndTabItem();
                    }

                    if (test_session2 && ImGui::BeginTabItem("Session 2")) {
                        ulnet_imgui_show_session(test_session2);
                        ImGui::EndTabItem();
                    }

                    ImGui::EndTabBar();
                }
                ImGui::End();
                ImGui::PopStyleColor();
            }


            if (ImGui::Button("Ask for equivalent peer id")) {
                sam2_connect_message_t message = { SAM2_CONN_HEADER };
                message.peer_id = g_ulnet_session.our_peer_id;

                g_libretro_context.SAM2Send((char *) &message);
            }

            if (ulnet_is_authority(&g_libretro_context.ulnet_session)) {
                if (ImGui::Button("Test Authority Join Self")) {
                    sam2_room_join_message_t joinMsg = { 0 };
                    memcpy(joinMsg.header, sam2_join_header, SAM2_HEADER_SIZE);
                    joinMsg.peer_id = g_ulnet_session.our_peer_id;
                    joinMsg.room = g_ulnet_session.room_we_are_in;

                    int result = ulnet_process_message(&g_ulnet_session, (char *)&joinMsg);
                    if (result == -1) {
                        SAM2_LOG_INFO("Test Authority Join Self passed: join request rejected.");
                    } else {
                        SAM2_LOG_ERROR("Test Authority Join Self FAILED: join request was not rejected (result = %d).", result);
                    }
                }
            }
        }

        const char* levelNames[] = {"Debug", "Info", "Warn", "Error", "Fatal"};
        ImGui::SliderInt("Log Level", &g_log_level, 0, 4, levelNames[g_log_level]);

        if (isWindowOpen && selected_message_index != -1) {
            ImGui::Begin("Messages", &isWindowOpen); // Use isWindowOpen to allow closing the window

            char *message = (char *) &g_libretro_context.message_history[selected_message_index];
            show_message(message);

            // Optionally, provide a way to close the window manually
            if (ImGui::Button("Close")) {
                isWindowOpen = false; // Close the window
            }

            ImGui::End();
        }

        if (g_ulnet_session.room_we_are_in.flags & SAM2_FLAG_ROOM_IS_NETWORK_HOSTED) {
            ImGui::SeparatorText("In Room");
        } else {
            ImGui::SeparatorText("Create a Room");
        }


        if (g_ulnet_session.room_we_are_in.flags & SAM2_FLAG_ROOM_IS_NETWORK_HOSTED) {
            ImGui::Text("Room: %s", g_ulnet_session.room_we_are_in.name);
            // Fixed text fields to display binary values
            //char ports_str[65] = {0};
            char flags_str[65] = {0};

            // Convert the integer values to binary strings
            for (int i = 0; i < 64; i+=4) {
                //ports_str[i/4] = '0' + ((g_room.ports >> (60 - i)) & 0xF);
                flags_str[i/4] = '0' + ((g_ulnet_session.room_we_are_in.flags >> (60 - i)) & 0xF);
            }

            ImGui::Text("Flags bitfield: %s", flags_str);
        } else {
            // Editable text field for room name
            ImGui::InputText("##name", g_new_room_set_through_gui.name, sizeof(g_new_room_set_through_gui.name), ImGuiInputTextFlags_None);
        }
    }

    if (g_ulnet_session.room_we_are_in.flags & SAM2_FLAG_ROOM_IS_NETWORK_HOSTED) {
        ImGui::SeparatorText("Connection Status");
        for (int p = 0; p < SAM2_PORT_MAX+1; p++) {
            if (g_ulnet_session.room_we_are_in.peer_ids[p] == SAM2_PORT_UNAVAILABLE) {
                ImGui::Text("Port %d: Unavailable", p);
            } else if (g_ulnet_session.room_we_are_in.peer_ids[p] == SAM2_PORT_AVAILABLE) {
                char label[32];
                snprintf(label, sizeof(label), "Port %d", p);
                if (ImGui::Button(label)) {
                    // Send a join room request for the available port
                    sam2_room_join_message_t request = { SAM2_JOIN_HEADER };
                    request.room = g_ulnet_session.room_we_are_in;
                    request.room.peer_ids[p] = g_ulnet_session.our_peer_id;
                    ulnet_message_send(&g_ulnet_session, SAM2_AUTHORITY_INDEX, (unsigned char *) &request);
                }
            } else {
                if (p == SAM2_AUTHORITY_INDEX) {
                    ImGui::Text("Authority:");
                } else {
                    ImGui::Text("Port %d:", p);
                }

                ImGui::SameLine();

                ImVec4 color = WHITE;

                if (g_ulnet_session.agent[p]) {
                    juice_state_t connection_state = juice_get_state(g_ulnet_session.agent[p]);

                if (   g_ulnet_session.room_we_are_in.flags & (SAM2_FLAG_PORT0_PEER_IS_INACTIVE << p)
                    || connection_state != JUICE_STATE_COMPLETED) {
                        color = GREY;
                    } else if (g_ulnet_session.peer_desynced_frame[p]) {
                        color = RED;
                    }
                } else if (g_ulnet_session.room_we_are_in.peer_ids[p] == g_ulnet_session.our_peer_id) {
                    color = GOLD;
                }

                ImGui::TextColored(color, "%05" PRId16, g_ulnet_session.room_we_are_in.peer_ids[p]);
                if (g_ulnet_session.agent[p]) {
                    juice_state_t connection_state = juice_get_state(g_ulnet_session.agent[p]);

                    if (g_ulnet_session.peer_desynced_frame[p]) {
                        ImGui::SameLine();
                        ImGui::TextColored(color, "Peer desynced (frame %" PRId64 ")", g_ulnet_session.peer_desynced_frame[p]);
                    }

                    if (connection_state != JUICE_STATE_COMPLETED) {
                        ImGui::SameLine();
                        ImGui::TextColored(color, "%s %c", juice_state_to_string(connection_state), spinnerGlyph);
                    }
                } else {
                    if (g_ulnet_session.room_we_are_in.peer_ids[p] != g_ulnet_session.our_peer_id) {
                        ImGui::SameLine();
                        ImGui::TextColored(color, "ICE agent not created");
                    }
                }

                char buffer_depth[ULNET_DELAY_BUFFER_SIZE] = {0};

                int64_t peer_num_frames_ahead = g_ulnet_session.state[p].frame - g_ulnet_session.frame_counter;
                for (int f = 0; f < sizeof(buffer_depth)-1; f++) {
                    buffer_depth[f] = f < peer_num_frames_ahead ? 'X' : 'O';
                }

                ImGui::SameLine();
                ImGui::TextColored(color, "Queue: %s Frame: %" PRId64, buffer_depth, g_ulnet_session.state[p].frame);
            }
        }

        {
            ImGui::BeginChild("SpectatorsTableWindow",
                ImVec2(
                    ImGui::GetContentRegionAvail().x,
                    ImGui::GetWindowContentRegionMax().y / 4
                    ), ImGuiWindowFlags_NoTitleBar);

            ImGui::SeparatorText("Spectators");
            if (ImGui::BeginTable("SpectatorsTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
                ImGui::TableSetupColumn("Peer ID");
                ImGui::TableSetupColumn("ICE Connection");
                ImGui::TableHeadersRow();

                for (int s = SAM2_SPECTATOR_START; s < SAM2_TOTAL_PEERS; s++) {
                    if (g_ulnet_session.room_we_are_in.peer_ids[s] <= SAM2_PORT_SENTINELS_MAX) continue;
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);

                    // Display peer ID
                    uint16_t peer_id = g_ulnet_session.room_we_are_in.peer_ids[s];
                    if (peer_id == g_ulnet_session.our_peer_id) ImGui::TextColored(GOLD, "%05" PRId16, peer_id);
                    else                                        ImGui::Text(             "%05" PRId16, peer_id);

                    ImGui::TableSetColumnIndex(1);
                    // Display ICE connection status
                    // Assuming g_ulnet_session.agent[] is an array of juice_agent_t* representing the ICE agents
                    juice_agent_t *spectator_agent = g_ulnet_session.agent[s];
                    if (spectator_agent) {
                        juice_state_t connection_state = juice_get_state(spectator_agent);

                        if (connection_state >= JUICE_STATE_CONNECTED) {
                            ImGui::Text("%s", juice_state_to_string(connection_state));
                        } else {
                            ImGui::TextColored(GREY, "%s %c", juice_state_to_string(connection_state), spinnerGlyph);
                        }

                    } else {
                        ImGui::Text("ICE agent not created");
                    }
                }

                ImGui::EndTable();
            }

            ImGui::EndChild();
        }

        sam2_room_join_message_t message = { SAM2_JOIN_HEADER };
        message.room = g_ulnet_session.room_we_are_in;
        int our_port = sam2_get_port_of_peer(&g_ulnet_session.room_we_are_in, g_ulnet_session.our_peer_id);
        if (our_port <= -1) {
            SAM2_LOG_WARN("No port associated for our peer_id=%d", g_ulnet_session.our_peer_id);
        } else if (our_port == SAM2_AUTHORITY_INDEX) {
            if (ImGui::Button("Abandon")) {
                message.room = g_ulnet_session.room_we_are_in;
                message.room.flags &= ~SAM2_FLAG_ROOM_IS_NETWORK_HOSTED;
                message.peer_id = g_ulnet_session.our_peer_id;
                ulnet_process_message(&g_ulnet_session, (const char *) &message); // *Send* a message to ourselves
            }
        } else if (our_port >= SAM2_SPECTATOR_START) {
            if (ImGui::Button("Exit")) {
                ulnet_session_tear_down(&g_ulnet_session);
                g_ulnet_session.room_we_are_in = g_new_room_set_through_gui;
            }
        } else {
            if (ImGui::Button("Detach Port")) {
#if 1
                message.room.peer_ids[our_port] = SAM2_PORT_AVAILABLE;
                ulnet_message_send(&g_ulnet_session, SAM2_AUTHORITY_INDEX, (unsigned char *) &message);
#else
                sam2_room_t future_room_we_are_in = ulnet_future_room_we_are_in(&g_ulnet_session);
                int future_our_port = sam2_get_port_of_peer(&future_room_we_are_in, g_ulnet_session.our_peer_id);
                if (future_our_port != -1) {
                    g_ulnet_session.next_room_xor_delta.peer_ids[future_our_port] = future_room_we_are_in.peer_ids[future_our_port] ^ SAM2_PORT_AVAILABLE;
                }
#endif
            }
        }
    } else {
        // Create a "Make" button that sends a make room request when clicked
        if (ImGui::Button("Make")) {
            sam2_room_make_message_t request = { SAM2_MAKE_HEADER };
            request.room = g_new_room_set_through_gui;
            request.room.flags |= SAM2_FLAG_ROOM_IS_NETWORK_HOSTED;
            g_libretro_context.SAM2Send((char *) &request);
        }
        if (ImGui::Button(g_is_refreshing_rooms ? "Stop" : "Refresh")) {
            g_is_refreshing_rooms = !g_is_refreshing_rooms;

            if (g_is_refreshing_rooms) {
                g_sam2_room_count = 0;
                // The list request is only a header
                sam2_room_list_message_t request = { SAM2_LIST_HEADER };
                g_libretro_context.SAM2Send((char *) &request);
            } else {

            }
        }

        // If we're in the "Stop" state
        if (g_is_refreshing_rooms) {
            // Run your "Stop" code here
        }

        ImGui::BeginChild("TableWindow",
            ImVec2(
                500,
                ImGui::GetWindowContentRegionMax().y/2
            ), ImGuiWindowFlags_NoTitleBar);

        static int selected_room_index = -1;  // Initialize as -1 to indicate no selection
        // Table
        if (ImGui::BeginTable("Rooms", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY)) {
            ImGui::TableSetupColumn("Room Name");
            ImGui::TableSetupColumn("Core");
            ImGui::TableSetupColumn("ROM Hash");
            ImGui::TableHeadersRow();

            for (int room_index = 0; room_index < g_sam2_room_count; ++room_index) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                // Make the row selectable and keep track of the selected room
                char label[128];
                snprintf(label, sizeof(label), "%.80s##%05" PRIu16, g_sam2_rooms[room_index].name, g_sam2_rooms[room_index].peer_ids[SAM2_AUTHORITY_INDEX]);
                if (ImGui::Selectable(label, selected_room_index == room_index, ImGuiSelectableFlags_SpanAllColumns)) {
                    selected_room_index = room_index;
                }

                ImGui::TableNextColumn();
                ImGui::TextUnformatted(g_sam2_rooms[room_index].core_and_version);
                //char ports_str[65];
                //peer_ids_to_string(g_sam2_rooms[room_index].peer_ids, ports_str);
                //ImGui::Text("%s", ports_str);

                ImGui::TableNextColumn();
                ImGui::Text("%016" PRIx64, g_sam2_rooms[room_index].rom_hash_xxh64);
            }

            ImGui::EndTable();
        }

        ImGui::EndChild();

        if (selected_room_index != -1) {
            if (   0 == strcmp(g_sam2_rooms[selected_room_index].core_and_version, g_new_room_set_through_gui.core_and_version)
                && g_sam2_rooms[selected_room_index].rom_hash_xxh64 == g_new_room_set_through_gui.rom_hash_xxh64) {

                ImGui::SameLine();
                if (ImGui::Button("Spectate")) {
                    // Directly signaling the authority just means spectate
                    if (g_ulnet_session.frame_counter == ULNET_WAITING_FOR_SAVE_STATE_SENTINEL) {
                        // We are already in a room, so we need to leave it first
                        SAM2_LOG_INFO("We are already in a room, leaving it first");
                    } else {
                        ulnet_session_init_defaulted(&g_ulnet_session);
                        // Both of these methods should work
#if 0
                        g_ulnet_session.room_we_are_in = g_sam2_rooms[selected_room_index];
#else
                        g_ulnet_session.room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX] = g_sam2_rooms[selected_room_index].peer_ids[SAM2_AUTHORITY_INDEX];
#endif
                        g_ulnet_session.frame_counter = ULNET_WAITING_FOR_SAVE_STATE_SENTINEL;
                        ulnet_startup_ice_for_peer(
                            &g_ulnet_session,
                            g_sam2_rooms[selected_room_index].peer_ids[SAM2_AUTHORITY_INDEX],
                            SAM2_AUTHORITY_INDEX,
                            NULL
                        );
                    }
                }
            } else {
                ImGui::BeginDisabled();
                (void)ImGui::Button("Spectate");
                ImGui::EndDisabled();
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip(
                        "Core or ROM hash mismatch\n"
                        "server ROM hash: %016" PRIx64 " core: %s\n"
                        "client ROM hash: %016" PRIx64 " core: %s",
                        g_sam2_rooms[selected_room_index].rom_hash_xxh64, g_sam2_rooms[selected_room_index].core_and_version,
                        g_new_room_set_through_gui.rom_hash_xxh64, g_new_room_set_through_gui.core_and_version
                    );
                }
            }
        }
    }
finished_drawing_sam2_interface:
    ImGui::End();

    {
        ImGui::Begin("Libretro Core", NULL, ImGuiWindowFlags_AlwaysAutoResize);

        // Core picker
        {
            constexpr int max_n = 1024;
            static int n = 0;
            static char content_name_utf8[max_n][MAX_PATH];
            static int content_flag[max_n] = {0};
            static bool core_picker_open = false;

            if (core_picker_open) {
                int selected_index = imgui_file_picker("Core Picker", g_core_path, content_name_utf8, content_flag, n);
                if (selected_index >= 0) {
                    if (content_flag[selected_index] & NETARCH_CONTENT_FLAG_IS_DIRECTORY) {
                        n = list_folder_contents(g_core_path, content_name_utf8, content_flag, max_n);
                    } else {
                        core_picker_open = false;
                        g_core_needs_reload = true;
                    }
                }
            } else {
                if (ImGui::Button(portable_basename(g_core_path))) {
                    core_picker_open = true;
                    strip_last_path_component(g_core_path);
                    n = list_folder_contents(g_core_path, content_name_utf8, content_flag, max_n);
                }
            }
        }

        { // ROM picker
            constexpr int max_n = 1024;
            static int n = 0;
            static char content_name_utf8[max_n][MAX_PATH];
            static int content_flag[max_n] = {0};
            static bool rom_picker_open = false;

            if (rom_picker_open) {
                int selected_index = imgui_file_picker("ROM Picker", g_rom_path, content_name_utf8, content_flag, n);
                if (selected_index >= 0) {
                    if (content_flag[selected_index] & NETARCH_CONTENT_FLAG_IS_DIRECTORY) {
                        n = list_folder_contents(g_rom_path, content_name_utf8, content_flag, max_n);
                    } else {
                        rom_picker_open = false;
                        g_rom_needs_reload = true;
                    }
                }
            } else {
                if (ImGui::Button(portable_basename(g_rom_path))) {
                    rom_picker_open = true;
                    strip_last_path_component(g_rom_path);
                    n = list_folder_contents(g_rom_path, content_name_utf8, content_flag, max_n);
                }
            }
        }

        if (SDL_GetError()[0] != '\0') {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "SDL Error: %s", SDL_GetError());
            ImGui::SameLine();
            if (ImGui::Button("Clear")) {
                SDL_ClearError();
            }
        }

        // Display current ROM info
        ImGui::Text("Core: %s", g_new_room_set_through_gui.core_and_version);

        // Assuming g_argc and g_argv are defined and populated
        if (ImGui::CollapsingHeader("Command Line Arguments")) {
            for (int i = 0; i < g_argc; i++) {
                ImGui::Text("argv[%d]=%s", i, g_argv[i]);
            }
        }

        if (ImGui::CollapsingHeader("Serialization Quirks")) {
            if (g_retro.quirks & RETRO_SERIALIZATION_QUIRK_INCOMPLETE) {
                ImGui::BulletText("Incomplete: Serialization is usable but not reliable for frame-sensitive features like netplay or rerecording.");
            }
            if (g_retro.quirks & RETRO_SERIALIZATION_QUIRK_MUST_INITIALIZE) {
                ImGui::BulletText("Must Initialize: Core requires initialization time before serialization is supported.");
            }
            if (g_retro.quirks & RETRO_SERIALIZATION_QUIRK_CORE_VARIABLE_SIZE) {
                ImGui::BulletText("Core Variable Size: Serialization size may change within a session.");
            }
            if (g_retro.quirks & RETRO_SERIALIZATION_QUIRK_FRONT_VARIABLE_SIZE) {
                ImGui::BulletText("Frontend Variable Size: Frontend supports variable-sized states.");
            }
            if (g_retro.quirks & RETRO_SERIALIZATION_QUIRK_SINGLE_SESSION) {
                ImGui::BulletText("Single Session: Serialized state can only be loaded during the same session.");
            }
            if (g_retro.quirks & RETRO_SERIALIZATION_QUIRK_ENDIAN_DEPENDENT) {
                ImGui::BulletText("Endian Dependent: Serialization is dependent on the system's endianness.");
            }
            if (g_retro.quirks & RETRO_SERIALIZATION_QUIRK_PLATFORM_DEPENDENT) {
                ImGui::BulletText("Platform Dependent: Serialization cannot be loaded on a different platform due to platform-specific dependencies.");
            }
            if (!g_retro.quirks) {
                ImGui::BulletText("The core did not report any serialization quirks.");
            }
        }

        if (ImGui::CollapsingHeader("Core Options")) {
            static int option_modified_at_index = -1;

            for (int i = 0; g_ulnet_session.core_options[i].key[0] != '\0'; i++) {
                // Determine if the current option is editable
                ImGuiInputTextFlags flags = 0;
                if (option_modified_at_index > -1) {
                    flags |= (i == option_modified_at_index) ? 0 : ImGuiInputTextFlags_ReadOnly;
                }

                if (g_ulnet_session.our_peer_id != g_ulnet_session.room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX]) {
                    flags |= ImGuiInputTextFlags_ReadOnly;
                }

                if (ImGui::InputText(g_ulnet_session.core_options[i].key,
                                    g_ulnet_session.core_options[i].value,
                                    IM_ARRAYSIZE(g_ulnet_session.core_options[i].value),
                                    flags)) {
                    // Mark this option as modified
                    option_modified_at_index = i;
                }
            }

            if (option_modified_at_index != -1) {
                // Show and handle the Save button
                if (ImGui::Button("Save")) {
                    // @todo There are weird race conditions if you rapidly modify options using this gui since the authority is directly modifying the values in the buffer
                    //       This really just shouldn't matter unless we hook up a programmatic tester or something to this gui. Ideally the authority should modify it when it
                    //       hits the frame the option was modified on.
                    g_ulnet_session.next_core_option = g_ulnet_session.core_options[option_modified_at_index];

                    option_modified_at_index = -1;
                }
            }
        }

        {
            int64_t min_delay_frames = 0;
            int64_t max_delay_frames = ULNET_DELAY_BUFFER_SIZE/2-1;
            if (ImGui::SliderScalar("Network Buffered Frames", ImGuiDataType_S64, &g_ulnet_session.delay_frames, &min_delay_frames, &max_delay_frames, "%lld", ImGuiSliderFlags_None)) {
                strcpy(g_ulnet_session.next_core_option.key, "netplay_delay_frames");
                snprintf(g_ulnet_session.next_core_option.value, sizeof(g_ulnet_session.next_core_option.value), "%" PRIx64, g_ulnet_session.delay_frames);
            }
        }

        ImGui::Checkbox("Fuzz Input", &g_libretro_context.fuzz_input);

        static bool old_vsync_enabled = true;

        if (g_vsync_enabled != old_vsync_enabled) {
            SAM2_LOG_INFO("Toggled vsync %s", g_vsync_enabled ? "on" : "off");
            if (!SDL_GL_SetSwapInterval((int) g_vsync_enabled)) {
                SAM2_LOG_ERROR("Unable to set VSync off: %s", SDL_GetError());
                g_vsync_enabled = true;
            }
        }

        old_vsync_enabled = g_vsync_enabled;

        if (old_vsync_enabled) {
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255)); // Red color
        }
        ImGui::Checkbox("vsync", &g_vsync_enabled);
        if (old_vsync_enabled) {
            ImGui::PopStyleColor(); // Reset to default color
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(
                    "vsync can cause stuttering during netplay because it blocks and thus during that time we're not polling for input"
                    " ,\n ticking the core, etc."
                );
            }
        }

        if (g_ulnet_session.frame_counter == ULNET_WAITING_FOR_SAVE_STATE_SENTINEL) {
            ImGui::Text("Core ticks: waiting for save state...");
        } else {
            ImGui::Text("Core ticks: %" PRId64, g_ulnet_session.frame_counter);
        }
        ImGui::Text("Core tick time (ms)");

        float *frame_time_dataset[] = {
            g_frame_time_milliseconds,
            g_core_wants_tick_in_milliseconds,
        };

        for (int datasetIndex = 0; datasetIndex < SAM2_ARRAY_LENGTH(frame_time_dataset); datasetIndex++) {
            float temp[MAX_SAMPLE_SIZE];
            float maxVal, minVal, sum, avgVal;
            ImVec2 plotSize = ImVec2(ImGui::GetContentRegionAvail().x, 150);

            int64_t cyclic_offset[] = {
                g_ulnet_session.frame_counter % g_sample_size,
                g_main_loop_cyclic_offset,
            };

            maxVal = -FLT_MAX;
            minVal = FLT_MAX;
            sum = 0.0f;

            for (int i = 0; i < g_sample_size; ++i) {
                float value = frame_time_dataset[datasetIndex][(i + cyclic_offset[datasetIndex]) % g_sample_size];
                temp[i] = value;
                maxVal = (value > maxVal) ? value : maxVal;
                minVal = (value < minVal) ? value : minVal;
                sum += value;
            }
            avgVal = sum / g_sample_size;

            if (datasetIndex == 0) {
                ImGui::Text("Max: %.3f ms  Min: %.3f ms", maxVal, minVal);
                ImGui::Text("Average: %.3f ms  Ideal: %.3f ms", avgVal, 1000.0f / g_av.timing.fps);
            }

            // Set the axis limits before beginning the plot
            ImPlot::SetNextAxisLimits(ImAxis_X1, 0, g_sample_size, ImGuiCond_Always);
            ImPlot::SetNextAxisLimits(ImAxis_Y1, 0.0f, SAM2_MAX(50.0f, maxVal), ImGuiCond_Always);

            const char* plotTitles[] = {"Frame Time Plot", "Core Wants Tick Plot"};
            if (ImPlot::BeginPlot(plotTitles[datasetIndex], plotSize)) {
                // Plot the histogram
                const char* barTitles[] = {"Frame Times", "Time until core wants to tick"};
                ImPlot::PlotBars(barTitles[datasetIndex], temp, g_sample_size, 0.67f, -0.5f);

                // Plot max, min, and average lines without changing their colors
                ImPlot::PlotInfLines("Max", &maxVal, 1, ImPlotInfLinesFlags_Horizontal); // Red line for max
                ImPlot::PlotInfLines("Min", &minVal, 1, ImPlotInfLinesFlags_Horizontal); // Blue line for min
                ImPlot::PlotInfLines("Avg", &avgVal, 1, ImPlotInfLinesFlags_Horizontal); // Green line for avg

                // End the plot
                ImPlot::EndPlot();
            }
        }

        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
        ImGui::End();
    }

    // @todo Add more information through dualui instead of window title
    //       See usages of mpContextExtra in netImgui/Code/Sample/SampleDualUI/SampleDualUI.cpp for example.
    static bool netimgui_was_connected = true; // Initialization to true so that the first frame sets the window title
    if (netimgui_was_connected != NetImgui::IsConnected() && g_win) {
        netimgui_was_connected = !netimgui_was_connected;

        char window_title[255];
        if (NetImgui::IsConnected()) {
            snprintf(window_title, sizeof(window_title), "netarch %s %s - CONNECTED TO NETIMGUI SERVER",
                g_libretro_context.system_info.library_name,
                g_libretro_context.system_info.library_version);
        } else {
            snprintf(window_title, sizeof(window_title), "netarch %s %s",
                g_libretro_context.system_info.library_name,
                g_libretro_context.system_info.library_version);
        }

        SDL_SetWindowTitle(g_win, window_title);
    }

    static bool render_imgui_windows_locally = true;
    if (ImGui::IsKeyDown(ImGuiKey_LeftShift) && ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) {
        // If only Ctrl+Shift is pressed, show available shortcuts
        ImGui::Begin("Shortcuts Window", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav
            | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs);
        ImGui::Text("Ctrl+Shift+S: Toggle visibility"); render_imgui_windows_locally ^= ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_S);
        ImGui::Text("Ctrl+Shift+A: Toggle collapse/expand");
        ImGui::Text("Ctrl+Shift+D: Toggle input fuzzing"); g_libretro_context.fuzz_input ^= ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_D);
        ImGui::Text("Ctrl+Shift+P: Demo Window"); show_demo_window ^= ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_P);
        ImGui::End();
    }

    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_A)) {
        auto &windows = ImGui::GetCurrentContext()->Windows;

        bool all_collapsed = true;
        for (ImGuiWindow* window : windows) {
            if (!window->ParentWindow && !(window->Flags & ImGuiWindowFlags_NoCollapse)) {
                SAM2_LOG_DEBUG("Checking if window %s is collapsed", window->Name);
                all_collapsed &= window->Collapsed;
            }
        }

        for (ImGuiWindow* window : windows) {
            SAM2_LOG_DEBUG("Collapsing window %s", window->Name);
            window->Collapsed = !all_collapsed;
        }
    }

    ImGuiJank::EndFrame();

    if (!g_headless && !NetImgui::IsConnected() && render_imgui_windows_locally) {
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }
}

static void draw_core_frame() {
    int w = 0, h = 0;
    SDL_GetWindowSize(g_win, &w, &h);
    LogGLErrors(glViewport(0, 0, w, h));

    LogGLErrors(glClear(GL_COLOR_BUFFER_BIT));

    LogGLErrors(glUseProgram(g_shader.program));

    LogGLErrors(glActiveTexture(GL_TEXTURE0));
    LogGLErrors(glBindTexture(GL_TEXTURE_2D, g_video.tex_id));


    LogGLErrors(glBindVertexArray(g_shader.vao));

    {
        // Simplified helper functions
        auto ShowGLStateInt = [](const char* label, GLenum pname, GLint expectedValue) {
            GLint currentValue = -1;
            glGetIntegerv(pname, &currentValue);
            bool match = (currentValue == expectedValue);
            ImGui::Text("%s:", label);
            ImGui::SameLine();
            ImGui::TextColored(match ? ImVec4(0,1,0,1) : ImVec4(1,0,0,1),
                match ? "%d" : "%d (Expected: %d)", currentValue, expectedValue);
        };

        auto ShowGLStateBool = [](const char* label, GLenum cap, GLboolean expectedValue) {
            GLboolean currentValue = glIsEnabled(cap);
            bool match = (currentValue == expectedValue);
            ImGui::Text("%s:", label);
            ImGui::SameLine();
            ImGui::TextColored(match ? ImVec4(0,1,0,1) : ImVec4(1,0,0,1),
                "%s%s", currentValue ? "Enabled" : "Disabled",
                match ? "" : expectedValue ? " (Expected: Enabled)" : " (Expected: Disabled)");
        };

        auto ShowGLStateViewport = [](const char* label, GLint expectedX, GLint expectedY, GLint expectedW, GLint expectedH) {
            GLint viewport[4] = {-1, -1, -1, -1};
            glGetIntegerv(GL_VIEWPORT, viewport);
            bool match = (viewport[0] == expectedX && viewport[1] == expectedY &&
                         viewport[2] == expectedW && viewport[3] == expectedH);
            ImGui::Text("%s:", label);
            ImGui::SameLine();
            ImGui::TextColored(match ? ImVec4(0,1,0,1) : ImVec4(1,0,0,1),
                match ? "(%d, %d, %d, %d)" : "(%d, %d, %d, %d) (Expected: %d, %d, %d, %d)",
                viewport[0], viewport[1], viewport[2], viewport[3],
                expectedX, expectedY, expectedW, expectedH);
        };

        int w = 0, h = 0;
        if (g_win) {
            SDL_GetWindowSize(g_win, &w, &h);
        }

        ImGui::Begin("Libretro Core", NULL, ImGuiWindowFlags_AlwaysAutoResize);
        if (ImGui::CollapsingHeader("GL Debug")) {
            ImGui::Text("State before window framebuffer draw call");
            ImGui::Separator();

            ShowGLStateBool("Blend Enabled", GL_BLEND, GL_FALSE);
            ShowGLStateBool("Depth Test Enabled", GL_DEPTH_TEST, GL_FALSE);
            ShowGLStateBool("Scissor Test Enabled", GL_SCISSOR_TEST, GL_FALSE);
            ShowGLStateBool("Cull Face Enabled", GL_CULL_FACE, GL_FALSE);
            ShowGLStateViewport("Viewport", 0, 0, w, h);
            ShowGLStateInt("Draw Framebuffer", GL_DRAW_FRAMEBUFFER_BINDING, 0);

            ImGui::Separator();

            // Core Output Texture Preview
            ImGui::Text("Core Output Texture:");
            if (g_video.tex_id != 0) {
                ImVec2 uv0(0.0f, 0.0f);
                ImVec2 uv1((float)g_video.clip_w / g_video.tex_w,
                        (float)g_video.clip_h / g_video.tex_h);
                if (g_video.hw.bottom_left_origin) {
                    uv0.y = (float)g_video.clip_h / g_video.tex_h;
                    uv1.y = 0.0f;
                }
                float aspect = (g_video.clip_h > 0) ?
                            ((float)g_video.clip_w / (float)g_video.clip_h) : 1.0f;
                float display_width = ImGui::GetContentRegionAvail().x;
                float display_height = display_width / aspect;
                ImGui::Image((ImTextureID)g_video.tex_id,
                            ImVec2(display_width, display_height), uv0, uv1);
            } else {
                ImGui::Text("Core texture not initialized (tex_id is 0)");
            }
        }
        ImGui::End();
    }

    LogGLErrors(glDrawArrays(GL_TRIANGLE_STRIP, 0, 4));
    LogGLErrors(glBindVertexArray(0));

    LogGLErrors(glUseProgram(0));
}

static void video_refresh(const void *data, unsigned width, unsigned height, unsigned pitch) {
    if (g_video.clip_w != width || g_video.clip_h != height)
    {
        g_video.clip_h = height;
        g_video.clip_w = width;

        refresh_vertex_data();
    }

    LogGLErrors(glBindFramebuffer(GL_FRAMEBUFFER, 0));
    LogGLErrors(glBindTexture(GL_TEXTURE_2D, g_video.tex_id));

    if (pitch != g_video.pitch)
        g_video.pitch = pitch;

    if (data && data != RETRO_HW_FRAME_BUFFER_VALID) {
        LogGLErrors(glPixelStorei(GL_UNPACK_ROW_LENGTH, g_video.pitch / g_video.bpp));
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
                        g_video.pixtype, g_video.pixfmt, data);
    }
}

static void video_deinit() {
    if (g_video.fbo_id) {
        LogGLErrors(glDeleteFramebuffers(1, &g_video.fbo_id));
    }

    if (g_video.tex_id) {
        LogGLErrors(glDeleteTextures(1, &g_video.tex_id));
    }

    if (g_shader.vao) {
        LogGLErrors(glDeleteVertexArrays(1, &g_shader.vao));
    }

    if (g_shader.vbo) {
        LogGLErrors(glDeleteBuffers(1, &g_shader.vbo));
    }

    if (g_shader.program) {
        LogGLErrors(glDeleteProgram(g_shader.program));
    }

    g_video.fbo_id = 0;
    g_video.tex_id = 0;
    g_shader.vao = 0;
    g_shader.vbo = 0;
    g_shader.program = 0;

    SDL_GL_MakeCurrent(g_win, g_ctx);
    SDL_GL_DestroyContext(g_ctx);

    g_ctx = NULL;

    SDL_DestroyWindow(g_win);
}


static void audio_init(int frequency) {
    if (g_pcm) {
        SDL_CloseAudioDevice(SDL_GetAudioStreamDevice(g_pcm));
        SDL_DestroyAudioStream(g_pcm);
    }

    SDL_AudioSpec spec;
    spec.format = SDL_AUDIO_S16;
    spec.channels = 2;
    spec.freq = frequency;

    g_pcm = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
    if (!g_pcm) {
        SAM2_LOG_FATAL("Failed to open playback device: %s", SDL_GetError());
    }

    SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(g_pcm));

    // Let the core know that the audio device has been initialized.
    if (audio_callback.set_state) {
        audio_callback.set_state(true);
    }
}


static void audio_deinit() {
    SDL_CloseAudioDevice(SDL_GetAudioStreamDevice(g_pcm));
    SDL_DestroyAudioStream(g_pcm);
}

static size_t audio_write(const int16_t *buf, unsigned frames) {
    int16_t scaled_buf[4096];
    for (unsigned i = 0; i < frames * 2; i++) {
        scaled_buf[i] = (buf[i] * g_volume) / 100;
    }

    SDL_PutAudioStreamData(g_pcm, scaled_buf, sizeof(*buf) * frames * 2);
    return frames;
}

// MARK: Logging
#ifndef _WIN32
#include <unistd.h> // isatty
#endif
#include <time.h>

static int sam2__terminal_supports_ansi_colors() {
#if defined(_WIN32)
    // Windows
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) {
        return 0;
    }

    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) {
        return 0;
    }

    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (!SetConsoleMode(hOut, dwMode)) {
        return 0;
    }
#else
    // POSIX
    if (!isatty(STDOUT_FILENO)) {
        return 0;
    }

    const char *term = getenv("TERM");
    if (term == NULL || strcmp(term, "dumb") == 0) {
        return 0;
    }
#endif

    return 1;
}

#if defined(__APPLE__)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif
static int sam2__debugger_is_attached_to_us(void){
#if defined(_WIN32)              /* Windows ---------------- */
    return IsDebuggerPresent();

#elif defined(__APPLE__)         /* macOS / iOS ------------ */
    int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, (int)getpid() };
    struct kinfo_proc info;
    size_t sz = sizeof(info);
    if (sysctl(mib, 4, &info, &sz, NULL, 0) == 0 && sz == sizeof(info))
        return (info.kp_proc.p_flag & P_TRACED) != 0;
    return 0;

#elif defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) \
   || defined(__OpenBSD__)        /* proc-based OSes ------- */
    FILE *f = fopen("/proc/self/status", "r");
    if (!f) return 0;
    char line[64];
    while (fgets(line, sizeof line, f))
        if (strncmp(line, "TracerPid:", 10) == 0) {
            int pid = atoi(line + 10);
            fclose(f);
            return pid != 0;
        }
    fclose(f);
    return 0;

#else                             /* Fallback -------------- */
    return 0;
#endif
}

static int sam2__get_localtime(const time_t *t, struct tm *buf) {
#ifdef _WIN32
    // Windows does not have POSIX localtime_r...
    return localtime_s(buf, t) == 0 ? 0 : -1;
#else // POSIX
    return localtime_r(t, buf) != NULL ? 0 : -1;
#endif
}

// This function is resolved via external linkage
SAM2_LINKAGE void sam2_log_write(int level, const char *file, int line, const char *format, ...) {
    if (level < g_log_level) {
        return;
    }

    const char *filename = file + strlen(file);
    while (filename != file && *filename != '/' && *filename != '\\') {
        --filename;
    }
    if (filename != file) {
        ++filename;
    }

    time_t t = time(NULL);
    struct tm lt;
    char timestamp[16];
    if (sam2__get_localtime(&t, &lt) != 0 || strftime(timestamp, 16, "%H:%M:%S", &lt) == 0) {
        timestamp[0] = '\0';
    }

    const char *prefix_fmt;
    switch (level) {
    default: //          ANSI-color-escape-codes  HH:MM:SS level  filename:line     |
    case 4: prefix_fmt = "\x1B[97m" "\x1B[41m"    "%s "    "FATAL "  "%11s:%-5d"   "| "; break;
    case 0: prefix_fmt = "\x1B[90m"               "%s "    "DEBUG "  "%11s:%-5d"   "| "; break;
    case 1: prefix_fmt = "\x1B[39m"               "%s "    "INFO  "  "%11s:%-5d"   "| "; break;
    case 2: prefix_fmt = "\x1B[93m"               "%s "    "WARN  "  "%11s:%-5d"   "| "; break;
    case 3: prefix_fmt = "\x1B[91m"               "%s "    "ERROR "  "%11s:%-5d"   "| "; break;
    }

    bool print_color = sam2__terminal_supports_ansi_colors();
    if (!print_color) while (*prefix_fmt == '\x1B') prefix_fmt += 5; // Skip ANSI-color-escape-codes

    fprintf(stdout, prefix_fmt, timestamp, filename, line);

    va_list args;
    va_start(args, format);
    vfprintf(stdout, format, args);
    va_end(args);

    fprintf(stdout, print_color ? "\033[K\x1B[0m\n" : "\n");
    if (level >= 2) { // Flush anything that is at least as severe as WARN
        fflush(stdout);
    }

    if (level >= 4) {
        if (sam2__debugger_is_attached_to_us()) {
#if defined(_WIN32)
            __debugbreak();   // FATAL ERROR OCURRED
#elif defined(__has_builtin)
#if __has_builtin(__builtin_trap)
            __builtin_trap(); // FATAL ERROR OCURRED
#endif
#endif
        }

        abort();
    }
}

static void core_log(enum retro_log_level level, const char *fmt, ...) {
    char buffer[4096] = {0};

    va_list va;
    va_start(va, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, va);
    va_end(va);

    if (level < g_log_level)
        return;

    // This is duplicated code wrt SAM2_LOG_DEBUG, SAM2_LOG_INFO, etc. but it would be tricky to do a refactor
    time_t t = time(NULL);
    struct tm lt;
    char timestamp[16];
    if (sam2__get_localtime(&t, &lt) != 0 || strftime(timestamp, 16, "%H:%M:%S", &lt) == 0) {
        timestamp[0] = '\0';
    }
    switch (level) {
    default:
    case 0x0: fprintf(stdout, "\x1B[90m%s DEBUG %16s | %s", timestamp, g_libretro_context.system_info.library_name, buffer); break;
    case 0x1: fprintf(stdout, "\x1B[39m%s INFO  %16s | %s", timestamp, g_libretro_context.system_info.library_name, buffer); break;
    case 0x2: fprintf(stdout, "\x1B[93m%s WARN  %16s | %s", timestamp, g_libretro_context.system_info.library_name, buffer); break;
    case 0x3: fprintf(stdout, "\x1B[93m%s FATAL %16s | %s", timestamp, g_libretro_context.system_info.library_name, buffer); break;
    }

    if (level >= 3) {
        if (sam2__debugger_is_attached_to_us()) {
#if defined(_WIN32)
            __debugbreak();   // FATAL ERROR OCURRED
#elif defined(__has_builtin)
#if __has_builtin(__builtin_trap)
            __builtin_trap(); // FATAL ERROR OCURRED
#endif
#endif
        }

        exit(1);
    }
}

static uintptr_t core_get_current_framebuffer() {
    return g_video.fbo_id;
}

/**
 * cpu_features_get_time_usec:
 *
 * Gets time in microseconds.
 *
 * Returns: time in microseconds.
 **/
retro_time_t cpu_features_get_time_usec(void) {
    return ulnet__get_unix_time_microseconds();
}

/**
 * Get the CPU Features.
 *
 * @see retro_get_cpu_features_t
 * @return uint64_t Returns a bit-mask of detected CPU features (RETRO_SIMD_*).
 */
static uint64_t core_get_cpu_features() {
    uint64_t cpu = 0;
    if (SDL_HasAVX()) {
        cpu |= RETRO_SIMD_AVX;
    }
    if (SDL_HasAVX2()) {
        cpu |= RETRO_SIMD_AVX2;
    }
    if (SDL_HasMMX()) {
        cpu |= RETRO_SIMD_MMX;
    }
    if (SDL_HasSSE()) {
        cpu |= RETRO_SIMD_SSE;
    }
    if (SDL_HasSSE2()) {
        cpu |= RETRO_SIMD_SSE2;
    }
    if (SDL_HasSSE3()) {
        cpu |= RETRO_SIMD_SSE3;
    }
    if (SDL_HasSSE41()) {
        cpu |= RETRO_SIMD_SSE4;
    }
    if (SDL_HasSSE42()) {
        cpu |= RETRO_SIMD_SSE42;
    }
    return cpu;
}

/**
 * A simple counter. Usually nanoseconds, but can also be CPU cycles.
 *
 * @see retro_perf_get_counter_t
 * @return retro_perf_tick_t The current value of the high resolution counter.
 */
static retro_perf_tick_t core_get_perf_counter() {
    return (retro_perf_tick_t)ulnet__rdtsc();
}

/**
 * Register a performance counter.
 *
 * @see retro_perf_register_t
 */
static void core_perf_register(struct retro_perf_counter* counter) {
    g_retro.perf_counter_last = counter;
    counter->registered = true;
}

/**
 * Starts a registered counter.
 *
 * @see retro_perf_start_t
 */
static void core_perf_start(struct retro_perf_counter* counter) {
    if (counter->registered) {
        counter->start = core_get_perf_counter();
    }
}

/**
 * Stops a registered counter.
 *
 * @see retro_perf_stop_t
 */
static void core_perf_stop(struct retro_perf_counter* counter) {
    counter->total = core_get_perf_counter() - counter->start;
}

/**
 * Log and display the state of performance counters.
 *
 * @see retro_perf_log_t
 */
static void core_perf_log() {
    // TODO: Use a linked list of counters, and loop through them all.
    //core_log(RETRO_LOG_INFO, "[timer] %s: %i - %i", g_retro.perf_counter_last->ident, g_retro.perf_counter_last->start, g_retro.perf_counter_last->total); This resulted in a nullptr access at one point
}

static bool core_environment(unsigned cmd, void *data) {
    switch (cmd) {
    case RETRO_ENVIRONMENT_SET_VARIABLES: {
        const struct retro_variable *vars = (const struct retro_variable *)data;
        size_t num_vars = 0;

        for (const struct retro_variable *v = vars; v->key; ++v) {
            num_vars++;
        }

        g_vars = (struct retro_variable*)calloc(num_vars + 1, sizeof(*g_vars));
        for (unsigned i = 0; i < num_vars; ++i) {
            const struct retro_variable *invar = &vars[i];
            struct retro_variable *outvar = &g_vars[i];

            const char *semicolon = strchr(invar->value, ';');
            const char *first_pipe = strchr(invar->value, '|');

            SDL_assert(semicolon && *semicolon);
            semicolon++;
            while (isspace(*semicolon))
                semicolon++;

            if (first_pipe) {
                outvar->value = (const char *)malloc((first_pipe - semicolon) + 1);
                memcpy((char*)outvar->value, semicolon, first_pipe - semicolon);
                ((char*)outvar->value)[first_pipe - semicolon] = '\0';
            } else {
                outvar->value = strdup(semicolon);
            }

            outvar->key = strdup(invar->key);
            SDL_assert(outvar->key && outvar->value);
        }

        for (unsigned i = 0; i < num_vars; ++i) {
            if (strlen(g_vars[i].key) > sizeof(g_ulnet_session.core_options[i].key) - 1) continue;
            if (strlen(g_vars[i].value) > sizeof(g_ulnet_session.core_options[i].value) - 1) continue;

            strcpy(g_ulnet_session.core_options[i].key, g_vars[i].key);
            strcpy(g_ulnet_session.core_options[i].value, g_vars[i].value);
        }

        return true;
    }
    case RETRO_ENVIRONMENT_GET_VARIABLE: {
        struct retro_variable *var = (struct retro_variable *)data;

        if (!g_vars)
            return false;

        for (const struct retro_variable *v = g_vars; v->key; ++v) {
            if (strcmp(var->key, v->key) == 0) {
                var->value = v->value;
                break;
            }
        }

        return true;
    }
    case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE: {
        bool *bval = (bool*)data;
        *bval = g_ulnet_session.flags & ULNET_SESSION_FLAG_CORE_OPTIONS_DIRTY;
        g_ulnet_session.flags &= ~ULNET_SESSION_FLAG_CORE_OPTIONS_DIRTY;
        return true;
    }
    case RETRO_ENVIRONMENT_GET_LOG_INTERFACE: {
        struct retro_log_callback *cb = (struct retro_log_callback *)data;
        cb->log = core_log;
        return true;
    }
    case RETRO_ENVIRONMENT_GET_PERF_INTERFACE: {
        struct retro_perf_callback *perf = (struct retro_perf_callback *)data;
        perf->get_time_usec = cpu_features_get_time_usec;
        perf->get_cpu_features = core_get_cpu_features;
        perf->get_perf_counter = core_get_perf_counter;
        perf->perf_register = core_perf_register;
        perf->perf_start = core_perf_start;
        perf->perf_stop = core_perf_stop;
        perf->perf_log = core_perf_log;
        return true;
    }
    case RETRO_ENVIRONMENT_GET_CAN_DUPE: {
        bool *bval = (bool*)data;
        *bval = true;
        return true;
    }
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: {
        const enum retro_pixel_format *fmt = (enum retro_pixel_format *)data;

        if (*fmt > RETRO_PIXEL_FORMAT_RGB565)
            return false;

        return video_set_pixel_format(*fmt);
    }
    case RETRO_ENVIRONMENT_SET_HW_RENDER: {
        struct retro_hw_render_callback *hw = (struct retro_hw_render_callback*)data;
        hw->get_current_framebuffer = core_get_current_framebuffer;
        hw->get_proc_address = (retro_hw_get_proc_address_t)SDL_GL_GetProcAddress;
        g_video.hw = *hw;
        return true;
    }
    case RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK: {
        const struct retro_frame_time_callback *frame_time =
            (const struct retro_frame_time_callback*)data;
        runloop_frame_time = *frame_time;
        return true;
    }
    case RETRO_ENVIRONMENT_SET_AUDIO_CALLBACK: {
        struct retro_audio_callback *audio_cb = (struct retro_audio_callback*)data;
        audio_callback = *audio_cb;
        return true;
    }
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
    case RETRO_ENVIRONMENT_GET_CORE_ASSETS_DIRECTORY:
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY: {
        const char **dir = (const char**)data;
        *dir = "System";
        return true;
    }
    case RETRO_ENVIRONMENT_SET_GEOMETRY: {
        const struct retro_game_geometry *geom = (const struct retro_game_geometry *)data;
        g_video.clip_w = geom->base_width;
        g_video.clip_h = geom->base_height;

        // some cores call this before we even have a window
        if (g_win) {
            refresh_vertex_data();

            int ow = 0, oh = 0;
            resize_to_aspect(geom->aspect_ratio, geom->base_width, geom->base_height, &ow, &oh);

            ow *= g_scale;
            oh *= g_scale;

            SDL_SetWindowSize(g_win, ow, oh);
        }
        return true;
    }
    case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME: {
        g_retro.supports_no_game = *(bool*)data;
        return true;
    }
    case RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE: {
        int *value = (int*)data;
        *value = 1 << 0 | 1 << 1;
        return true;
    }
    case RETRO_ENVIRONMENT_SET_SERIALIZATION_QUIRKS: {
        uint64_t *quirks = (uint64_t*)data;
        g_retro.quirks = *quirks;
        return true;
    }
    case RETRO_ENVIRONMENT_GET_PREFERRED_HW_RENDER: {
        unsigned *hw = (unsigned*)data;
        *hw = RETRO_HW_CONTEXT_OPENGL;
        return true;
    }
    case RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE: {
        struct retro_rumble_interface *rumble = (struct retro_rumble_interface*)data;
        rumble->set_rumble_state = []   (unsigned port, enum retro_rumble_effect effect, uint16_t strength) -> bool {
            return false;
        };
        return true;
    }
    case RETRO_ENVIRONMENT_SET_HW_SHARED_CONTEXT: {
        g_use_shared_context = true;

        return true;
    }
    default:
        core_log(RETRO_LOG_WARN, "Unhandled env #%u\n", cmd);
        return false;
    }

    return false;
}


static void core_video_refresh(const void *data, unsigned width, unsigned height, size_t pitch) {
    if (!g_win) return;
    video_refresh(data, width, height, pitch);
}

int64_t byte_swap_int64(int64_t val) {
    int64_t swapped = ((val & 0x00000000000000ffLL) << 56) |
                      ((val & 0x000000000000ff00LL) << 40) |
                      ((val & 0x0000000000ff0000LL) << 24) |
                      ((val & 0x00000000ff000000LL) << 8)  |
                      ((val & 0x000000ff00000000LL) >> 8)  |
                      ((val & 0x0000ff0000000000LL) >> 24) |
                      ((val & 0x00ff000000000000LL) >> 40) |
                      ((val & 0xff00000000000000LL) >> 56);
    return swapped;
}

void FLibretroContext::core_input_poll() {
    memset(InputState, 0, sizeof(InputState));

    ulnet_input_poll(&ulnet_session, &g_libretro_context.InputState);
}

static bool g_we_ticked = false;
static void core_input_poll(void) {
    g_we_ticked = true;
    g_libretro_context.core_input_poll();
}


int16_t FLibretroContext::core_input_state(unsigned port, unsigned device, unsigned index, unsigned id) {
    // To get the core to poll for certain types of input sometimes requires setting particular controllers for compatible ports
    // or changing specific options related to the input you're trying to poll for. If it's not obvious your main resources are
    // forums, the libretro documentation, or looking through the core's code itself.
    // Also here are some pitfalls I've encountered:
    // - The core might need to poll for at least two frames to register an input
    // - Some cores will not poll for any input by default (I fix this by always binding the RETRO_DEVICE_JOYPAD)
    // - The RETRO_DEVICE_POINTER interface is generally preferred over the lightgun and mouse even for things like lightguns and mice although you still use some parts of the lightgun interface for handling lightgun input probably same goes for mouse

    switch (device) {
    case RETRO_DEVICE_JOYPAD:   return g_libretro_context.InputState[port][JoypadB     + id];
    case RETRO_DEVICE_LIGHTGUN: return g_libretro_context.InputState[port][LightgunX   + id];
    case RETRO_DEVICE_ANALOG:   return g_libretro_context.InputState[port][AnalogLeftX + 2 * index + (id % RETRO_DEVICE_ID_JOYPAD_L2)]; // The indexing logic is broken and might OOBs if we're queried for something that isn't an analog trigger or stick
    case RETRO_DEVICE_POINTER:  return g_libretro_context.InputState[port][PointerX    + 4 * index + id];
    case RETRO_DEVICE_MOUSE:
    case RETRO_DEVICE_KEYBOARD:
    default:                    return 0;
    }
}

static int16_t core_input_state(unsigned port, unsigned device, unsigned index, unsigned id) {
    return g_libretro_context.core_input_state(port, device, index, id);
}


static void core_audio_sample(int16_t left, int16_t right) {
    int16_t buf[2] = {left, right};
    audio_write(buf, 1);
}


static size_t core_audio_sample_batch(const int16_t *data, size_t frames) {
    return audio_write(data, frames);
}


static char* resolve_absolute_path(const char* path) {
    if (!path) return NULL;

    // Check if path is already absolute
#ifdef _WIN32
    if ((path[0] && path[1] == ':') || (path[0] == '\\' && path[1] == '\\')) {
        // Already absolute (C:\ or \\)
        return SDL_strdup(path);
    }
#else
    if (path[0] == '/') {
        // Already absolute
        return SDL_strdup(path);
    }
#endif

    // Get current working directory
    char* cwd = SDL_GetCurrentDirectory();
    if (!cwd) {
        SAM2_LOG_ERROR("Failed to get current directory: %s", SDL_GetError());
        return SDL_strdup(path); // fallback to original path
    }

    // Construct absolute path
    size_t cwd_len = strlen(cwd);
    size_t path_len = strlen(path);
    size_t total_len = cwd_len + 1 + path_len + 1; // +1 for separator, +1 for null terminator

    char* absolute_path = (char*)malloc(total_len);
    if (!absolute_path) {
        SDL_free(cwd);
        return SDL_strdup(path); // fallback to original path
    }

    SDL_snprintf(absolute_path, total_len, "%s%s%s", cwd, PATH_SEPARATOR, path);
    SDL_free(cwd);

    return absolute_path;
}

static void core_load(const char *sofile) {
    void (*set_environment)(retro_environment_t) = NULL;
    void (*set_video_refresh)(retro_video_refresh_t) = NULL;
    void (*set_input_poll)(retro_input_poll_t) = NULL;
    void (*set_input_state)(retro_input_state_t) = NULL;
    void (*set_audio_sample)(retro_audio_sample_t) = NULL;
    void (*set_audio_sample_batch)(retro_audio_sample_batch_t) = NULL;
    memset(&g_retro, 0, sizeof(g_retro));

    // Resolve relative paths to absolute paths
    char* absolute_sofile = resolve_absolute_path(sofile);
    g_retro.handle = SDL_LoadObject(absolute_sofile ? absolute_sofile : sofile);

    if (!g_retro.handle)
    {
        struct stat file_stat;
        const char* path_to_check = absolute_sofile ? absolute_sofile : sofile;
        if (stat(path_to_check, &file_stat) == 0) {
            SAM2_LOG_FATAL("Failed to load core: '%s' exists, but could not be loaded. "
                          "This may be due to an architecture mismatch or incompatible dependencies. SDL_Error: %s",
                          path_to_check, SDL_GetError());
        } else {
            SAM2_LOG_FATAL("Failed to load core: '%s' does not exist. SDL_Error: %s", path_to_check, SDL_GetError());
        }
    }

    // Clean up allocated memory
    if (absolute_sofile) {
        free(absolute_sofile);
    }

    load_retro_sym(retro_init);
    load_retro_sym(retro_deinit);
    load_retro_sym(retro_api_version);
    load_retro_sym(retro_get_system_info);
    load_retro_sym(retro_get_system_av_info);
    load_retro_sym(retro_set_controller_port_device);
    load_retro_sym(retro_reset);
    load_retro_sym(retro_run);
    load_retro_sym(retro_load_game);
    load_retro_sym(retro_unload_game);
    load_retro_sym(retro_serialize_size);
    load_retro_sym(retro_serialize);
    load_retro_sym(retro_unserialize);

    load_sym(set_environment, retro_set_environment);
    load_sym(set_video_refresh, retro_set_video_refresh);
    load_sym(set_input_poll, retro_set_input_poll);
    load_sym(set_input_state, retro_set_input_state);
    load_sym(set_audio_sample, retro_set_audio_sample);
    load_sym(set_audio_sample_batch, retro_set_audio_sample_batch);

    set_environment(core_environment);
    set_video_refresh(core_video_refresh);
    set_input_poll(core_input_poll);
    set_input_state(core_input_state);
    set_audio_sample(core_audio_sample);
    set_audio_sample_batch(core_audio_sample_batch);

    g_retro.retro_init();
    g_retro.initialized = true;

    SAM2_LOG_INFO("Core loaded");
}

static void core_load_game(const char *rom_path, void *rom_data, size_t rom_size) {
    struct retro_game_info info;

    info.path = rom_path;
    info.meta = "";
    info.data = rom_data;
    info.size = rom_size;

    g_retro.retro_get_system_info(&g_libretro_context.system_info);

    g_retro.game_loaded = g_retro.retro_load_game(&info);
    if (!g_retro.game_loaded) {
        SAM2_LOG_FATAL("The core failed to load the content.");
    }

    g_retro.retro_get_system_av_info(&g_av);

    if (!g_headless) {
        video_configure(&g_av.geometry);
        audio_init(g_av.timing.sample_rate);
    }

    // Update room configuration with new ROM info
    g_new_room_set_through_gui.rom_hash_xxh64 = ZSTD_XXH64(rom_data, rom_size, 0);
    sam2_format_core_version(
        &g_new_room_set_through_gui,
        g_libretro_context.system_info.library_name,
        g_libretro_context.system_info.library_version
    );
}

static void core_unload() {
    if (g_retro.initialized) {
        if (g_core_ctx) {
            SDL_GL_MakeCurrent(g_win, g_core_ctx);
        }
        g_retro.retro_deinit();
        if (g_core_ctx) {
            SDL_GL_DestroyContext(g_core_ctx);
            SDL_GL_MakeCurrent(g_win, g_ctx);
        }

        g_core_ctx = NULL;
        g_use_shared_context = false;
    }

    if (g_retro.handle) {
        SDL_UnloadObject(g_retro.handle);
    }
}

static void noop() {}

void receive_juice_log(juice_log_level_t level, const char *message) {
    static const char *log_level_names[] = {"VERBOSE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"};

    fprintf(stdout, "%s: %s\n", log_level_names[level], message);
    fflush(stdout);
    assert(level < JUICE_LOG_LEVEL_ERROR);
}

// Custom logging function for reliable library
int reliable_log_redirect(const char *fmt, ...) {
    char buffer[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    time_t t = time(NULL);
    struct tm lt;
    char timestamp[16];
    if (sam2__get_localtime(&t, &lt) != 0 || strftime(timestamp, 16, "%H:%M:%S", &lt) == 0) {
        timestamp[0] = '\0';
    }

    fprintf(stdout, "%s "    "UNK   "  "%11s:%-5s"   "| %s", timestamp, "reliable.c", "?", buffer);
    return 0;
}

void rle_encode32(void *input_typeless, size_t inputSize, void *output_typeless, size_t *outputSize) {
    size_t writeIndex = 0;
    size_t readIndex = 0;

    // The games where we need high compression of rle are 32-bit consoles
    int32_t *input = (int32_t *)input_typeless;
    int32_t *output = (int32_t *)output_typeless;

    while (readIndex < inputSize) {
        if (input[readIndex] == 0) {
            int32_t zeroCount = 0;
            while (readIndex < inputSize && input[readIndex] == 0) {
                zeroCount++;
                readIndex++;
            }

            output[writeIndex++] = 0; // write a zero to mark the start of a run
            output[writeIndex++] = zeroCount; // write the count of zeros
        } else {
            output[writeIndex++] = input[readIndex++];
        }
    }

    *outputSize = writeIndex;
}

// Byte swaps 4 byte words in place from big endian to little endian
// If the IEEE float32 encoding of the word is between 65536 and 1/65536 or -65536 and -1/65536, then the word is not swapped
static void heuristic_byte_swap(uint32_t *data, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        uint32_t word = data[i];
        uint32_t ieee_float_32_exponent = (word >> 23) & 0xFF;

        // Check if the value is within the range [1/65536, 65536] or (-65536, -1/65536]

        if ((ieee_float_32_exponent < 0x6F) || (ieee_float_32_exponent >= 0x8F)) {
            // Byte swap if the value is not within the specified range
            data[i] = ((word >> 24) & 0x000000FF) |
                      ((word >> 8) & 0x0000FF00) |
                      ((word << 8) & 0x00FF0000) |
                      ((word << 24) & 0xFF000000);
        }
    }
}

#if NETARCH_EXTRA_COMPRESSION_INVESTIGATION
#define BASE 257  // A prime base, slightly larger than the number of characters in the input set
#define MODULUS 1000000007  // A large prime modulus

// Function to compute the initial hash of a string of length `len`
uint32_t compute_initial_hash(const char *str, int len) {
    uint32_t hash = 0;
    for (int i = 0; i < len; ++i) {
        hash = (hash * BASE + str[i]) % MODULUS;
    }
    return hash;
}

// Function to update the hash when moving the window one character to the right
static inline uint32_t update_hash(uint32_t old_hash, char old_char, char new_char, uint32_t highest_base, int len) {
    uint32_t new_hash = old_hash;
    new_hash = (new_hash - highest_base * old_char % MODULUS + MODULUS) % MODULUS; // Remove the old char
    new_hash = (new_hash * BASE + new_char) % MODULUS;  // Add the new char
    return new_hash;
}

// Precompute the highest power of BASE modulo MODULUS for length `len`
uint32_t precompute_highest_base(int len) {
    uint32_t highest_base = 1;
    for (int i = 1; i < len; ++i) {
        highest_base = (highest_base * BASE) % MODULUS;
    }
    return highest_base;
}
#include <unordered_map>
#include <set>

extern "C" {
#include "fastcdc.h"
}
#endif

// I pulled this out of main because it kind of clutters the logic and to get back some indentation
void tick_compression_investigation(char *save_state, size_t save_state_size, char *rom_data, size_t rom_size) {
    uint64_t start = ulnet__rdtsc();
    unsigned char *buffer = g_savebuffer[g_save_state_index];
    static unsigned char g_savebuffer_delta[sizeof(g_savebuffer[0])];
    if (g_do_zstd_delta_compress) {
        buffer = g_savebuffer_delta;
        for (int i = 0; i < g_serialize_size; i++) {
            int delta_index = (g_save_state_index - g_save_state_used_for_delta_index_offset + MAX_SAVE_STATES) % MAX_SAVE_STATES;
            g_savebuffer_delta[i] = g_savebuffer[delta_index][i] ^ g_savebuffer[g_save_state_index][i];
        }
    }

    static unsigned char savebuffer_compressed[2 * sizeof(g_savebuffer[0])]; // Double the savestate size just cause degenerate run length encoding could make it about 1.5x I think
    if (g_use_rle) {
        // If we're 4 byte aligned use the 4-byte wordsize rle that gives us the highest gains in 32-bit consoles (where we need it the most)
        if (g_serialize_size % 4 == 0) {
            rle_encode32(buffer, g_serialize_size / 4, savebuffer_compressed, &g_zstd_compress_size[g_ulnet_session.frame_counter % g_sample_size]);
            g_zstd_compress_size[g_ulnet_session.frame_counter % g_sample_size] *= 4;
        } else {
            g_zstd_compress_size[g_ulnet_session.frame_counter % g_sample_size] = rle8_encode_capped(buffer, g_serialize_size, savebuffer_compressed, sizeof(savebuffer_compressed)); // @todo Technically this can overflow I don't really plan to use it though and I find the odds unlikely
        }
    } else {
        if (g_use_dictionary) {

            // There is a lot of ceremony to use the dictionary
            static ZSTD_CDict *cdict = NULL;
            if (g_dictionary_is_dirty) {
                size_t partition_size = rom_size / 8;
                size_t samples_sizes[8] = { partition_size, partition_size, partition_size, partition_size,
                                            partition_size, partition_size, partition_size, partition_size };
                size_t dictionary_size = ZDICT_optimizeTrainFromBuffer_cover(
                    g_dictionary, sizeof(g_dictionary),
                    rom_data, samples_sizes, sizeof(samples_sizes)/sizeof(samples_sizes[0]),
                    &g_parameters);

                if (cdict) {
                    ZSTD_freeCDict(cdict);
                }

                if (ZDICT_isError(dictionary_size)) {
                    fprintf(stderr, "Error optimizing dictionary: %s\n", ZDICT_getErrorName(dictionary_size));
                    cdict = NULL;
                } else {
                    cdict = ZSTD_createCDict(g_dictionary, sizeof(g_dictionary), g_zstd_compress_level);
                }

                g_dictionary_is_dirty = false;
            }

            static ZSTD_CCtx *cctx = NULL;
            if (cctx == NULL) {
                cctx = ZSTD_createCCtx();
            }

            ZSTD_CCtx_setParameter(cctx, ZSTD_c_compressionLevel, g_zstd_compress_level);
            //ZSTD_CCtx_setParameter(cctx, ZSTD_c_checksumFlag, 0);
            ZSTD_CCtx_setParameter(cctx, ZSTD_c_nbWorkers, g_zstd_thread_count);
            //ZSTD_CCtx_setParameter(cctx, ZSTD_c_jobSize, 0);
            //ZSTD_CCtx_setParameter(cctx, ZSTD_c_overlapLog, 0);
            //ZSTD_CCtx_setParameter(cctx, ZSTD_c_enableLdm, 0);
            //ZSTD_CCtx_setParameter(cctx, ZSTD_c_ldmHashLog, 0);

            if (cdict) {
                g_zstd_compress_size[g_ulnet_session.frame_counter % g_sample_size] = ZSTD_compress_usingCDict(cctx,
                                                                                       savebuffer_compressed, sizeof(savebuffer_compressed),
                                                                                       buffer, g_serialize_size,
                                                                                       cdict);
            }
        } else {
            g_zstd_compress_size[g_ulnet_session.frame_counter % g_sample_size] = ZSTD_compress(savebuffer_compressed,
                                                                        sizeof(savebuffer_compressed),
                                                                        buffer, g_serialize_size, g_zstd_compress_level);
        }

    }

    if (ZSTD_isError(g_zstd_compress_size[g_ulnet_session.frame_counter % g_sample_size])) {
        fprintf(stderr, "Error compressing: %s\n", ZSTD_getErrorName(g_zstd_compress_size[g_ulnet_session.frame_counter % g_sample_size]));
        g_zstd_compress_size[g_ulnet_session.frame_counter % g_sample_size] = 0;
    }

    g_zstd_cycle_count[g_ulnet_session.frame_counter % g_sample_size] = ulnet__rdtsc() - start;

    // I'm trying to compress the save state data by finding matching blocks that are in the ROM.
    // This doesn't seem to work. Here are my theories:
    // - Data is compressed in the rom and decompressed in the save state (very likely, this could be verified by comparing entropy)
    // - Endianess mismatch (unlikely)
    // - Coincidently bad alignment or fragmentation
    // - My code is broken
    // The FastCDC code is from https://github.com/sleepybishop/fastcdc
#if NETARCH_EXTRA_COMPRESSION_INVESTIGATION
    ImGui::Begin("Extra Compression Investigation");

    std::unordered_map<size_t, size_t> unique_blocks; // Hash -> Block Size

    // Lambda for processing blocks (no captures)
    auto process_block_func = [](void *arg, size_t offset, size_t len) -> int {
        // Cast arg back to the original data structure
        auto *data_info = static_cast<std::pair<std::unordered_map<size_t, size_t>*, void*>*>(arg);
        auto &unique_blocks = data_info->first;
        auto data = data_info->second;

        size_t hash = ZSTD_XXH64((uint8_t*)data + offset, len, 0);
        (*unique_blocks)[hash] = len;
        return 0;
    };

    // Process save state blocks
    fcdc_ctx ctx_copy_from = fastcdc_init(32, 64, 128);
    {
        std::pair<std::unordered_map<size_t, size_t>*, void*> data_info{&unique_blocks, save_state};
        fcdc_ctx ctx = ctx_copy_from;
        fastcdc_update(&ctx, (uint8_t*)save_state, save_state_size, 1, process_block_func, &data_info);
    }

    struct SharedSizeData {
        std::unordered_map<size_t, size_t>* unique_blocks;
        size_t shared_size;
        uint8_t* data; // Pointer to the actual data buffer
    } args = {&unique_blocks, 0, (uint8_t*)(rom_data)};

    // Process ROM blocks and calculate shared size
    auto shared_size_func = [](void *arg, size_t offset, size_t len) -> int {
        // Cast arg back to the original data structure
        auto *data_info = static_cast<SharedSizeData *>(arg);
        // Dereference the pointer to access the map
        auto &unique_blocks = *data_info->unique_blocks;  // Change here
        auto &shared_size = data_info->shared_size;

        size_t hash = ZSTD_XXH64(data_info->data + offset, len, 0);
        auto it = unique_blocks.find(hash);
        if (it != unique_blocks.end()) {
            shared_size += it->second;
        }
        return 0;
    };
    {
        fcdc_ctx ctx = ctx_copy_from;
        //fastcdc_update(&ctx, (uint8_t*)rom_data, rom_size, 1, shared_size_func, &data_info);
        fastcdc_update(&ctx, (uint8_t*)rom_data, rom_size, 1, shared_size_func, &args);
    }

    // Display results
    std::set<size_t> block_hashes;
    char unit_str[64] = "bits";
    double unit = format_unit_count(8 * args.shared_size, unit_str);
    ImGui::Text("FastCDC shared data size: %.3g %s", unit, unit_str);


    const size_t window_size = 64;  // Example window size
    uint32_t highest_base = precompute_highest_base(window_size);
    uint32_t hash = compute_initial_hash((char *)save_state, window_size);

    for (size_t i = 0; i <= save_state_size - window_size; ++i) {
        hash = update_hash(hash, save_state[i], save_state[i + window_size], highest_base, window_size);
        block_hashes.insert(hash);
    }

    hash = compute_initial_hash(rom_data, window_size);

    int shared_block_count = 0;
    for (size_t i = 0; i <= rom_size - window_size; ++i) {
        hash = update_hash(hash, rom_data[i], rom_data[i + window_size], highest_base, window_size);
        if (block_hashes.find(hash) != block_hashes.end()) {
            shared_block_count++;
        }
    }

    strcpy(unit_str, "bits");
    unit = format_unit_count(8 * window_size * shared_block_count, unit_str);
    ImGui::Text("Fixed window size: %d bytes", window_size);
    ImGui::Text("FastCDC shared data size: %g %s", unit, unit_str);

    ImGui::End();
#endif
}


int main(int argc, char *argv[]) {
    g_argc = argc;
    g_argv = argv;

    bool no_netimgui = false;
    for (int i = 2; i < argc; i++) {
        if (0 == strcmp("--headless", argv[i])) {
            g_headless = true;
        } else if (0 == strcmp("--no-netimgui", argv[i])) {
            no_netimgui = true;
        } else if (0 == strcmp("--test", argv[i])) {
            int num_failed_tests = 0;

            SAM2_LOG_INFO("Running tests...");
            num_failed_tests += sam2_test_all();
            num_failed_tests += ulnet_test_inproc(NULL, NULL);
            if (num_failed_tests > 0) {
                SAM2_LOG_ERROR("Failed to run all inproc tests, please fix them before running the core");
            } else {
                SAM2_LOG_INFO("Tests passed");
            }

            return num_failed_tests > 0;
        } else if (argv[i][0] == '-') {
            SAM2_LOG_FATAL("Unknown option: %s", argv[i]);
        }
    }

    if (argc < 2)
        SAM2_LOG_FATAL("Usage: %s <core> [game] [options...]", argv[0]);

    SDL_strlcpy(g_core_path, argv[1], sizeof(g_core_path));

    if (   0 == strcmp(g_sam2_address, "localhost")
        || 0 == strcmp(g_sam2_address, "127.0.0.1")
        || 0 == strcmp(g_sam2_address, "::1")) {
        SAM2_LOG_INFO("g_sam2_address=%s attempting to host signaling and match-making server ourselves", g_sam2_address);

        g_sam2_server = (sam2_server_t *) malloc(sizeof(sam2_server_t));
        int err = sam2_server_init(g_sam2_server, g_sam2_port);

        if (err) {
            SAM2_LOG_INFO("Couldn't initialize server ourselves probably we're already hosting one in another instance");
            free(g_sam2_server);
            g_sam2_server = NULL;
        }
    }

    if (sam2_client_connect(&g_sam2_socket, g_sam2_address, g_sam2_port)) {
        SAM2_LOG_WARN("Failed to connect to Signaling-Server and a Match-Maker\n");
    }

    juice_set_log_level(JUICE_LOG_LEVEL_WARN);

    g_parameters.d = 8;
    g_parameters.k = 256;
    g_parameters.steps = 4;
    g_parameters.nbThreads = g_zstd_thread_count;
    g_parameters.splitPoint = 0;
    g_parameters.zParams.compressionLevel = g_zstd_compress_level;

    if (!SDL_Init(g_headless ? 0 : SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_EVENTS|SDL_INIT_GAMEPAD)) {
        SAM2_LOG_FATAL("Failed to initialize SDL: %s", SDL_GetError());
    }

    // Setup Platform/Renderer backends
    // GL 3.0 + GLSL 130
    g_video.hw.version_major = 4;
    g_video.hw.version_minor = 1;
    g_video.hw.context_type  = RETRO_HW_CONTEXT_OPENGL_CORE;
    g_video.hw.context_reset   = noop;
    g_video.hw.context_destroy = noop;

    // Load the core.
    core_load(g_core_path);

    if (!g_retro.supports_no_game && argc < 3)
        SAM2_LOG_FATAL("This core requires a game in order to run");

    // Load the game.
    void *rom_data = NULL;
    size_t rom_size = 0;
    if (argc > 2) {
        SDL_strlcpy(g_rom_path, argv[2], sizeof(g_rom_path));
        g_rom_path[sizeof(g_rom_path)-1] = '\0';
        read_whole_file(g_rom_path, &rom_data, &rom_size);
    }
    core_load_game(g_rom_path, rom_data, rom_size);

    // Configure the player input devices.
    g_retro.retro_set_controller_port_device(0, RETRO_DEVICE_JOYPAD);

    // Initialize gamepad detection for already connected controllers
    SDL_JoystickID *gamepads = SDL_GetGamepads(NULL);
    if (gamepads && !g_gamepad) {
        g_gamepad = SDL_OpenGamepad(gamepads[0]);
        if (g_gamepad) {
            g_gamepad_instance_id = SDL_GetGamepadID(g_gamepad);
            SAM2_LOG_INFO("Gamepad detected at startup: %s", SDL_GetGamepadName(g_gamepad));
        }
        SDL_free(gamepads);
    }

    // GL 3.0 + GLSL 130
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, g_video.hw.version_major);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, g_video.hw.version_minor);

    IMGUI_CHECKVERSION();
    ImGui::SetCurrentContext(ImGui::CreateContext());
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.IniFilename = NULL; // Don't write an ini file that caches window positions
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    {
        // Doing font initialization manually is a special case for netImgui and using the ImGui "null" backend
        // Confusingly, you don't manually set the null backend see imgui/examples/example_null/main.cpp for details
        // You'll hit a variety of asserts if you don't execute the following boilerplate
        io.Fonts->AddFontDefault();
        io.Fonts->Build();
        io.Fonts->SetTexID(0);
        io.DisplaySize = ImVec2(8, 8);
    }

    if (!no_netimgui) {
        if (!NetImgui::Startup()) {
            SAM2_LOG_FATAL("Failed to initialize NetImgui");
        }

        for (g_netimgui_port = 8889; g_netimgui_port < 65535; g_netimgui_port++) {
            NetImgui::ConnectFromApp("netarch (ImGui " IMGUI_VERSION ")", g_netimgui_port);
            if (NetImgui::IsConnectionPending()) {
                SAM2_LOG_INFO("NetImgui " NETIMGUI_VERSION " is listening on port %i", g_netimgui_port);
                break;
            }
        }
    }

    g_ulnet_session.flags |= ULNET_SESSION_FLAG_DRAW_IMGUI;

    if (!g_headless) {
        // Setup Platform/Renderer backends
        ImGui_ImplSDL3_InitForOpenGL(g_win, g_ctx);
        ImGui_ImplOpenGL3_Init("#version 150");
    }

    SDL_Event ev;

    if (strcmp(g_libretro_context.system_info.library_name, "dolphin-emu") == 0) { // @todo Really we just should prevent saving before retro_run is called
        g_do_zstd_compress = false;
    }

    for (g_main_loop_cyclic_offset = 0; running; g_main_loop_cyclic_offset = (g_main_loop_cyclic_offset + 1) % MAX_SAMPLE_SIZE) {
        if (g_core_needs_reload) {
            g_core_needs_reload = false;

            core_unload();
            core_load(g_core_path);
        }

        if (g_rom_needs_reload) {
            g_rom_needs_reload = false;

            g_retro.retro_unload_game();

            if (rom_data) {
                SDL_free(rom_data);
            }

            read_whole_file(g_rom_path, &rom_data, &rom_size);
            core_load_game(g_rom_path, rom_data, rom_size);
        }

        // Update the game loop timer.
        if (runloop_frame_time.callback) {
            retro_time_t current = cpu_features_get_time_usec();
            retro_time_t delta = current - runloop_frame_time_last;

            if (!runloop_frame_time_last)
                delta = runloop_frame_time.reference;
            runloop_frame_time_last = current;
            runloop_frame_time.callback(delta);
        }

        // Ask the core to emit the audio.
        if (audio_callback.callback) {
            audio_callback.callback();
        }

        while (SDL_PollEvent(&ev)) {
            if (!g_headless && !NetImgui::IsConnected()) {
                ImGui_ImplSDL3_ProcessEvent(&ev);
            }

            switch (ev.type) {
            case SDL_EVENT_QUIT: running = false; break;
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED: running = false; break;
            case SDL_EVENT_WINDOW_RESIZED: resize_cb(ev.window.data1, ev.window.data2); break;
            case SDL_EVENT_GAMEPAD_ADDED:
                if (!g_gamepad) {
                    g_gamepad = SDL_OpenGamepad(ev.gdevice.which);
                    if (g_gamepad) {
                        g_gamepad_instance_id = SDL_GetGamepadID(g_gamepad);
                        SAM2_LOG_INFO("Gamepad connected: %s", SDL_GetGamepadName(g_gamepad));
                    }
                }
                break;
            case SDL_EVENT_GAMEPAD_REMOVED:
                if (g_gamepad && ev.gdevice.which == g_gamepad_instance_id) {
                    SDL_CloseGamepad(g_gamepad);
                    g_gamepad = NULL;
                    g_gamepad_instance_id = 0;
                    SAM2_LOG_INFO("Gamepad disconnected");
                }
                break;
            }
        }

        if (!g_headless) {
            LogGLErrors(glBindFramebuffer(GL_FRAMEBUFFER, 0));
        }
#if 0
        // Timing for frame rate
        struct timespec start_time, end_time;
        clock_gettime(CLOCK_MONOTONIC, &start_time);
#endif

        g_core_wants_tick_in_milliseconds[g_main_loop_cyclic_offset] = core_wants_tick_in_seconds(g_ulnet_session.core_wants_tick_at_unix_usec) * 1000.0;

        if (!g_headless && !NetImgui::IsConnected()) {
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplSDL3_NewFrame();
        }

        ImGuiJank::NewFrame();

        g_kbd = SDL_GetKeyboardState(NULL);

        if (g_kbd[SDL_SCANCODE_ESCAPE]) {
            running = false;
        }

        for (int i = 0; g_binds[i].k || g_binds[i].rk; ++i) {
            g_ulnet_session.next_input_state[0][g_binds[i].rk] = (int16_t) g_kbd[g_binds[i].k];
        }

        // Handle gamepad digital buttons
        for (int i = 0; g_gamepad_binds[i].button || g_gamepad_binds[i].retro_id; ++i) {
            if (g_gamepad && SDL_GetGamepadButton(g_gamepad, g_gamepad_binds[i].button)) {
                g_ulnet_session.next_input_state[0][g_gamepad_binds[i].retro_id] = 1;
            }
        }

        // Nice for testing network code I found a bug in nestopia using this
        if (g_libretro_context.fuzz_input) {
            for (int i = 0; i < 16; ++i) {
                g_ulnet_session.next_input_state[0][i] = rand() & 0x0001;
            }
        }

        if (g_do_zstd_compress) {
            g_serialize_size = g_retro.retro_serialize_size();
            if (g_serialize_size > sizeof(g_savebuffer[g_save_state_index])) {
                SAM2_LOG_ERROR("Save state buffer is too small (%zu > %zu)", g_serialize_size, sizeof(g_savebuffer[g_save_state_index]));
                g_do_zstd_compress = false;
            }
        }

        double max_sleeping_allowed_when_polling_network_seconds = g_headless ? 1.0 : 0.0; // We just use vertical sync for frame-pacing when we have a head
        g_ulnet_session.retro_run = [](void *user_ptr) {
            if (g_use_shared_context) {
                SDL_GL_MakeCurrent(g_win, g_core_ctx);
            }
            g_retro.retro_run();
            if (g_use_shared_context) {
                SDL_GL_MakeCurrent(g_win, g_ctx);
            }
        };
        g_ulnet_session.retro_serialize_size = [](void *user_ptr) {
            return g_retro.retro_serialize_size();
        };
        g_ulnet_session.retro_serialize = [](void *user_ptr, void *data, size_t size) {
            return g_retro.retro_serialize(data, size);
        };
        g_ulnet_session.retro_unserialize = [](void *user_ptr, const void *data, size_t size) {
            return g_retro.retro_unserialize(data, size);
        };
        int status = ulnet_poll_session(&g_ulnet_session, g_do_zstd_compress, g_savebuffer[g_save_state_index], sizeof(g_savebuffer[g_save_state_index]),
            g_av.timing.fps, max_sleeping_allowed_when_polling_network_seconds);

        if (status & ULNET_POLL_SESSION_BUFFERED_INPUT) {
            memset(&g_ulnet_session.next_core_option, 0, sizeof(g_ulnet_session.next_core_option));
        }

        if (g_do_zstd_compress && (status & ULNET_POLL_SESSION_SAVED_STATE)) {
            tick_compression_investigation((char *)g_savebuffer[g_save_state_index], g_serialize_size, (char*)rom_data, rom_size);

            g_save_state_index = (g_save_state_index + 1) % MAX_SAVE_STATES;
        }

        if (status & ULNET_POLL_SESSION_TICKED) {
            // Keep track of frame-times for plotting purposes
            static int64_t last_tick_usec = ulnet__get_unix_time_microseconds();
            int64_t current_time_usec = ulnet__get_unix_time_microseconds();
            int64_t elapsed_time_milliseconds = (current_time_usec - last_tick_usec) / 1000;
            g_frame_time_milliseconds[g_ulnet_session.frame_counter % g_sample_size] = elapsed_time_milliseconds;
            last_tick_usec = current_time_usec;
        }

        if (!g_headless) {
            // The imgui frame is updated at the monitor refresh cadence
            // So the core frame needs to be redrawn at the same cadence or you'll get the Windows XP infinite window thing
            draw_core_frame();
        }

        // Slight performance save if no one is looking at the imgui
        if (!g_headless || NetImgui::IsConnected()) {
            draw_imgui();
        } else {
            ImGuiJank::EndFrame();
        }

        if (!g_headless) {
            // We hope vsync is disabled or else this will block
            // I think you have to write platform specific code / not use OpenGL if you want this to be non-blocking
            // and still try to update on vertical sync or use another thread, but I don't like threads
            SDL_GL_SwapWindow(g_win);
        }

        if (g_sam2_server) {
            for (int i = 0; i < 128; i++) {
                if (sam2_server_poll(g_sam2_server) == 0) break;
            }
        }

        if (   (g_sam2_socket != SAM2_SOCKET_INVALID)
            && (g_connected_to_sam2 || (g_connected_to_sam2 = sam2_client_poll_connection(g_sam2_socket, 0)))) {
            for (int _prevent_infinite_loop_counter = 0; _prevent_infinite_loop_counter < 64; _prevent_infinite_loop_counter++) {
                static sam2_message_u latest_sam2_message; // This is gradually buffered so it has to be static

                int status = sam2_client_poll(g_sam2_socket, &latest_sam2_message);

                if (status < 0) {
                    SAM2_LOG_ERROR("Error polling sam2 server: %d", status);
                    sam2_client_disconnect(g_sam2_socket);
                    g_sam2_socket = SAM2_SOCKET_INVALID;
                    g_connected_to_sam2 = false;
                    break;
                } else if (status == 0) {
                    break;
                } else {
                    if (g_libretro_context.message_history_length < SAM2_ARRAY_LENGTH(g_libretro_context.message_history)) {
                        g_libretro_context.message_history[g_libretro_context.message_history_length++] = latest_sam2_message;
                    }

                    g_ulnet_session.zstd_compress_level = g_zstd_compress_level;
                    g_ulnet_session.user_ptr = (void *) &g_libretro_context;
                    g_ulnet_session.sam2_send_callback = [](void *user_ptr, char *response) {
                        // We delegate sends to us so we have a single location of debug bookkeeping + error checking of sent messages
                        FLibretroContext *LibretroContext = (FLibretroContext *) user_ptr;
                        return LibretroContext->SAM2Send(response);
                    };

                    status = ulnet_process_message(
                        &g_ulnet_session,
                        (const char *) &latest_sam2_message
                    );

                    if (sam2_header_matches((const char*)&latest_sam2_message, sam2_fail_header)) {
                        g_last_sam2_error = latest_sam2_message.error_message;
                        SAM2_LOG_ERROR("Received error response from SAM2 (%" PRId64 "): %s", g_last_sam2_error.code, g_last_sam2_error.description);
                    } else if (sam2_header_matches((const char*)&latest_sam2_message, sam2_list_header)) {
                        sam2_room_list_message_t *room_list = (sam2_room_list_message_t *) &latest_sam2_message;

                        if (room_list->room.peer_ids[SAM2_AUTHORITY_INDEX] == 0) {
                            g_is_refreshing_rooms = false;
                        } else {
                            if (g_sam2_room_count < SAM2_ARRAY_LENGTH(g_sam2_rooms)) {
                                g_sam2_rooms[g_sam2_room_count++] = room_list->room;
                            }
                        }
                    } else if (sam2_header_matches((const char*)&latest_sam2_message, sam2_conn_header)) {
                        g_new_room_set_through_gui.peer_ids[SAM2_AUTHORITY_INDEX] = latest_sam2_message.connect_message.peer_id;
                    }
                }
            }
        }
    }
//cleanup:
    if (g_retro.game_loaded) {
        g_retro.retro_unload_game();
    }

    core_unload();
    audio_deinit();
    video_deinit();

    // Destroy agent
    for (int p = 0; p < SAM2_PORT_MAX+1; p++) {
        if (g_ulnet_session.agent[p]) {
            juice_destroy(g_ulnet_session.agent[p]);
        }
    }

    if (g_vars) {
        for (const struct retro_variable *v = g_vars; v->key; ++v) {
            free((char*)v->key);
            free((char*)v->value);
        }
        free(g_vars);
    }

    NetImgui::Shutdown(); //ImGui::Shutdown();

    // Cleanup gamepad
    if (g_gamepad) {
        SDL_CloseGamepad(g_gamepad);
        g_gamepad = NULL;
    }

    SDL_Quit();

    return EXIT_SUCCESS;
}
