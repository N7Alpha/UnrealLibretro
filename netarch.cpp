#include <stdbool.h>
#include <stdint.h>
#ifdef _WIN32
#define NOMINMAX  1
#endif

#define SAM2_IMPLEMENTATION
#define SAM2_SERVER
//#define SAM2_LOG_WRITE(level, file, line, ...) do { printf(__VA_ARGS__); printf("\n"); } while (0); // Ex. Use print
#define SAM2_LOG_WRITE_DEFINITION
#define SAM2_LOG_WRITE(level, file, line, ...) do { if (level >= g_log_level) { sam2__log_write(level, __FILE__, __LINE__, __VA_ARGS__); } } while (0)
int g_log_level = 1; // Info

#define ULNET_IMGUI
#define ULNET_IMPLEMENTATION
#include "ulnet.h"
#include "sam2.h"

#define MAX_SAMPLE_SIZE ULNET_MAX_SAMPLE_SIZE

#define NETIMGUI_IMPLEMENTATION
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
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_loadso.h>
#include <SDL3/SDL_video.h>
#endif
#include "libretro.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#else
#include <unistd.h> // for sleep
#endif

// Hide OpenGL functions SDL declares with external linkage we just load all of them dynamically to support headless operation
#define glActiveTexture RENAMED_BY_NETARCH_CPP_glActiveTexture
#define glBindTexture RENAMED_BY_NETARCH_CPP_glBindTexture
#define glClear RENAMED_BY_NETARCH_CPP_glClear
#define glClearColor RENAMED_BY_NETARCH_CPP_glClearColor
#define glDeleteTextures RENAMED_BY_NETARCH_CPP_glDeleteTextures
#define glEnable RENAMED_BY_NETARCH_CPP_glEnable
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
#include <SDL3/SDL_opengl.h>
#undef glActiveTexture
#undef glBindTexture
#undef glClear
#undef glClearColor
#undef glDeleteTextures
#undef glEnable
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
        EnumMacro(PFNGLENABLEPROC, glEnable) \
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


#define DEFINE_GL_PROCEDURES(Type,Func) Type Func = NULL;
ENUM_GL_PROCEDURES(DEFINE_GL_PROCEDURES);

static SDL_Window *g_win = NULL;
static SDL_GLContext g_ctx = NULL;
static SDL_AudioStream *g_pcm = NULL;
static struct retro_frame_time_callback runloop_frame_time;
static retro_usec_t runloop_frame_time_last = 0;
static const uint8_t *g_kbd = NULL;
struct retro_system_av_info g_av = {0};
static struct retro_audio_callback audio_callback;

static float g_scale = 3;
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
    void *handle;
    bool initialized;
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

enum class ERetroDeviceID : uint8_t
{
    // RETRO_DEVICE_ID_JOYPAD
    JoypadB,
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

#include <type_traits>
// std alchemy that converts enum class variables to their integer type
template<typename E>
static constexpr auto to_integral(E e) -> typename std::underlying_type<E>::type
{
    return static_cast<typename std::underlying_type<E>::type>(e);
}

static_assert(to_integral(ERetroDeviceID::Size) < sizeof(ulnet_input_state_t) / sizeof((*(ulnet_input_state_t *) (0x0))[0]), "ulnet_input_state_t is too small");



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
        if (memcmp(message, sam2_sign_header, SAM2_HEADER_TAG_SIZE) == 0) {
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

    bool Spectating() {
        return ulnet_is_spectator(&ulnet_session, ulnet_session.our_peer_id);
    }

    juice_agent_t *AuthorityAgent() {
        return ulnet_session.agent[SAM2_AUTHORITY_INDEX];
    }

    juice_agent_t *LocateAgent(uint64_t peer_id) {
        for (int p = 0; p < SAM2_ARRAY_LENGTH(ulnet_session.agent); p++) {
            if (ulnet_session.agent[p] && ulnet_session.room_we_are_in.peer_ids[p] == peer_id) {
                return ulnet_session.agent[p];
            }
        }

        return NULL;
    }

    bool IsAuthority() const {
        return ulnet_session.our_peer_id == ulnet_session.room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX];
    }

    int OurPort() {
        return ulnet_our_port(&ulnet_session);
    }

    int16_t core_input_state(unsigned port, unsigned device, unsigned index, unsigned id);
    void core_input_poll();
};

ulnet_core_option_t g_core_option_for_next_frame = {0};

static struct FLibretroContext g_libretro_context;
auto &g_ulnet_session = g_libretro_context.ulnet_session;
static bool g_headless = 0;
static int g_netimgui_port = 0;

struct keymap {
    unsigned k;
    unsigned rk;
};

static struct keymap g_binds[] = {
    { SDL_SCANCODE_X, to_integral(ERetroDeviceID::JoypadA) },
    { SDL_SCANCODE_Z, to_integral(ERetroDeviceID::JoypadB) },
    { SDL_SCANCODE_A, to_integral(ERetroDeviceID::JoypadY) },
    { SDL_SCANCODE_S, to_integral(ERetroDeviceID::JoypadX) },
    { SDL_SCANCODE_UP, to_integral(ERetroDeviceID::JoypadUp) },
    { SDL_SCANCODE_DOWN, to_integral(ERetroDeviceID::JoypadDown) },
    { SDL_SCANCODE_LEFT, to_integral(ERetroDeviceID::JoypadLeft) },
    { SDL_SCANCODE_RIGHT, to_integral(ERetroDeviceID::JoypadRight) },
    { SDL_SCANCODE_RETURN, to_integral(ERetroDeviceID::JoypadStart) },
    { SDL_SCANCODE_BACKSPACE, to_integral(ERetroDeviceID::JoypadSelect) },
    { SDL_SCANCODE_Q, to_integral(ERetroDeviceID::JoypadL) },
    { SDL_SCANCODE_W, to_integral(ERetroDeviceID::JoypadR) },
    { 0, 0 }
};


#define load_sym(V, S) do {\
    if (!((*(SDL_FunctionPointer*)&V) = SDL_LoadFunction(g_retro.handle, #S))) \
        SAM2_LOG_FATAL("Failed to load symbol '" #S "'': %s", SDL_GetError()); \
    } while (0)
#define load_retro_sym(S) load_sym(g_retro.S, S)

static void logical_partition(int sz, int redundant, int *n, int *out_k, int *packet_size, int *packet_groups);

// This is a little confusing since the lower byte of sequence corresponds to the largest stride
static int64_t logical_partition_offset_bytes(uint8_t sequence_hi, uint8_t sequence_lo, int block_size_bytes, int block_stride);

static GLuint compile_shader(unsigned type, unsigned count, const char **strings) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, count, strings, NULL);
    glCompileShader(shader);

    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

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

    glAttachShader(program, vshader);
    glAttachShader(program, fshader);
    glLinkProgram(program);

    glDeleteShader(vshader);
    glDeleteShader(fshader);

    glValidateProgram(program);

    GLint status;
    glGetProgramiv(program, GL_LINK_STATUS, &status);

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

    glGenVertexArrays(1, &g_shader.vao);
    glGenBuffers(1, &g_shader.vbo);

    glUseProgram(g_shader.program);

    glUniform1i(g_shader.u_tex, 0);

    float m[4][4];
    if (g_video.hw.bottom_left_origin)
        ortho2d(m, -1, 1, 1, -1);
    else
        ortho2d(m, -1, 1, -1, 1);

    glUniformMatrix4fv(g_shader.u_mvp, 1, GL_FALSE, (float*)m);

    glUseProgram(0);
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

    glBindVertexArray(g_shader.vao);

    glBindBuffer(GL_ARRAY_BUFFER, g_shader.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_data), vertex_data, GL_STREAM_DRAW);

    glEnableVertexAttribArray(g_shader.i_pos);
    glEnableVertexAttribArray(g_shader.i_coord);
    glVertexAttribPointer(g_shader.i_pos, 2, GL_FLOAT, GL_FALSE, sizeof(float)*4, 0);
    glVertexAttribPointer(g_shader.i_coord, 2, GL_FLOAT, GL_FALSE, sizeof(float)*4, (void*)(2 * sizeof(float)));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

static void init_framebuffer(int width, int height)
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


static void resize_cb(int w, int h) {
    glViewport(0, 0, w, h);
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

    SAM2_LOG_INFO("GL_SHADING_LANGUAGE_VERSION: %s", glGetString(GL_SHADING_LANGUAGE_VERSION));
    SAM2_LOG_INFO("GL_VERSION: %s", glGetString(GL_VERSION));


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
        *dw = *dh * ratio;
    else
        *dh = *dw / ratio;
}


static void video_configure(const struct retro_game_geometry *geom) {
    int nwidth, nheight;

    resize_to_aspect(geom->aspect_ratio, geom->base_width * 1, geom->base_height * 1, &nwidth, &nheight);

    nwidth *= g_scale;
    nheight *= g_scale;

    if (!g_win)
        create_window(nwidth, nheight);

    if (g_video.tex_id)
        glDeleteTextures(1, &g_video.tex_id);

    g_video.tex_id = 0;

    if (!g_video.pixfmt)
        g_video.pixfmt = GL_UNSIGNED_SHORT_5_5_5_1;

    SDL_SetWindowSize(g_win, nwidth, nheight);

    glGenTextures(1, &g_video.tex_id);

    if (!g_video.tex_id)
        SAM2_LOG_FATAL("Failed to create the video texture");

    g_video.pitch = geom->max_width * g_video.bpp;

    glBindTexture(GL_TEXTURE_2D, g_video.tex_id);

//	glPixelStorei(GL_UNPACK_ALIGNMENT, s_video.pixfmt == GL_UNSIGNED_INT_8_8_8_8_REV ? 4 : 2);
//	glPixelStorei(GL_UNPACK_ROW_LENGTH, s_video.pitch / s_video.bpp);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, geom->max_width, geom->max_height, 0,
            g_video.pixtype, g_video.pixfmt, NULL);

    glBindTexture(GL_TEXTURE_2D, 0);

    init_framebuffer(geom->max_width, geom->max_height);

    g_video.tex_w = geom->max_width;
    g_video.tex_h = geom->max_height;
    g_video.clip_w = geom->base_width;
    g_video.clip_h = geom->base_height;

    refresh_vertex_data();

    g_video.hw.context_reset();
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
    if (   strcmp(postfix, "bits") == 0
        || strcmp(postfix, "bytes") == 0) {
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

static int g_volume = 3;
static bool g_vsync_enabled = true;

static ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
static bool g_connected_to_sam2 = false;
static sam2_error_message_t g_last_sam2_error = { SAM2_RESPONSE_SUCCESS };

static void peer_ids_to_string(uint64_t peer_ids[], char *output) {
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

    auto show_room = [=](const sam2_room_t& room) {
        ImGui::Text("Room: %s", room.name);
        ImGui::Text("Flags: %016" PRIx64, room.flags);
        ImGui::Text("Core: %s", room.core_and_version);
        ImGui::Text("ROM Hash: %016" PRIx64, room.rom_hash_xxh64);
        
        for (int p = 0; p < SAM2_PORT_MAX+1; p++) {
            ImVec4 color = g_ulnet_session.our_peer_id == room.peer_ids[p] ? GOLD : WHITE;
            if (p == SAM2_AUTHORITY_INDEX) {
                ImGui::Text("Authority Peer ID: ");
            } else {
                ImGui::Text("Port %d Peer ID: ", p);
            }

            ImGui::SameLine();
            if (room.peer_ids[p] == SAM2_PORT_AVAILABLE) {
                ImGui::Text("Available");
            } else if (room.peer_ids[p] == SAM2_PORT_UNAVAILABLE) {
                ImGui::Text("Unavailable");
            } else {
                ImGui::TextColored(color, "%016" PRIx64, room.peer_ids[p]);
            }
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

        for (int i = 0; i < g_ulnet_session.sample_size; i++) {
            avg_cycle_count += g_ulnet_session.save_state_execution_time_cycles[i];
            avg_zstd_compress_size += g_zstd_compress_size[i];
            avg_zstd_cycle_count += g_zstd_cycle_count[i];
            if (g_zstd_compress_size[i] > max_compress_size) {
                max_compress_size = g_zstd_compress_size[i];
            }
        }
        avg_cycle_count        /= g_ulnet_session.sample_size;
        avg_zstd_compress_size /= g_ulnet_session.sample_size;
        avg_zstd_cycle_count   /= g_ulnet_session.sample_size;

        strcpy(unit, "bits");
        double display_count = format_unit_count(8 * g_retro.retro_serialize_size(), unit);
        ImGui::Text("retro_serialize_size: %g %s", display_count, unit);
        strcpy(unit, "cycles");
        display_count = format_unit_count(avg_cycle_count, unit);
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

            ImGui::Text("Remote Savestate hash: %" PRIx64 "", g_remote_savestate_hash);
        }

        ImGui::SliderInt("Sample size", &g_ulnet_session.sample_size, 1, MAX_SAMPLE_SIZE);
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
                for (int i = 0; i < g_ulnet_session.sample_size; ++i) {
                    temp[i] = static_cast<float>(g_ulnet_session.save_state_execution_time_cycles[(i+g_ulnet_session.frame_counter)%g_ulnet_session.sample_size]);
                }
            } else if (current_item == 1) {
                for (int i = 0; i < g_ulnet_session.sample_size; ++i) {
                    temp[i] = static_cast<float>(g_zstd_cycle_count[(i+g_ulnet_session.frame_counter)%g_ulnet_session.sample_size]);
                }
            } else if (current_item == 2) {
                for (int i = 0; i < g_ulnet_session.sample_size; ++i) {
                    temp[i] = static_cast<float>(g_zstd_compress_size[(i+g_ulnet_session.frame_counter)%g_ulnet_session.sample_size]);
                }
            }
        }

        // Slider to select the current save state index
        ImGui::SliderInt("Save State Index (saved every frame)", &g_save_state_index, 0, MAX_SAVE_STATES-1);
        ImGui::SliderInt("Delta compression frame offset", &g_save_state_used_for_delta_index_offset, 0, MAX_SAVE_STATES-1);

        ImGui::End();
    }

    {
        ImGui::Begin("Signaling Server and a Match Maker", NULL, ImGuiWindowFlags_AlwaysAutoResize);

        if (g_sam2_server) {
            ImGui::SeparatorText("Server");
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "We're listening on [::]:%d (IPv4 tunneling is OS dependent)", g_sam2_port);

            if (ImGui::CollapsingHeader("Server Information")) {

                char label[64];
                snprintf(label, 64, "Rooms: %" PRId64, g_sam2_server->room_count);
                if (ImGui::CollapsingHeader(label)) {
                    for (int i = 0; i < g_sam2_server->room_count; i++) {
                        show_room(g_sam2_server->rooms[i]);
                    }
                }

                ImGui::Text("Messages allocated: %" PRId64, g_sam2_server->_debug_allocated_messages);
            }

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

            if (is_ipv6) ImGui::TextColored(ImVec4(0, 1, 0, 1), "Connected to [" "%s" "]:%d", g_sam2_address, g_sam2_port);
            else         ImGui::TextColored(ImVec4(0, 1, 0, 1), "Connected to "  "%s"  ":%d", g_sam2_address, g_sam2_port);

            ImGui::SameLine();
            if (ImGui::Button("Disconnect")) {
                if (sam2_client_disconnect(&g_libretro_context.sam2_socket)) {
                    SAM2_LOG_FATAL("Couldn't disconnect socket");
                }
                g_libretro_context.sam2_socket = SAM2_SOCKET_INVALID;
            }
        } else {
            if (g_libretro_context.sam2_socket == SAM2_SOCKET_INVALID) {
                char port_str[64]; // Buffer to store the input text
                sprintf(port_str, "%d", g_sam2_port);
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
                    if (sam2_client_disconnect(&g_libretro_context.sam2_socket)) {
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
        if (ImGui::CollapsingHeader("Messages")) {
            if (ImGui::BeginTable("MessagesTable", 1, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupColumn("Header", ImGuiTableColumnFlags_WidthFixed, 150.0f);
                ImGui::TableHeadersRow();

                for (int i = 0; i < g_libretro_context.message_history_length; ++i) {
                    sam2_message_u *message = &g_libretro_context.message_history[i];
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);

                    ImGui::Text("%.8s", (char *) message);
                    ImGui::SameLine();
                    char button_label[64] = {0};
                    snprintf(button_label, sizeof(button_label), "Show##%d", i);

                    if (ImGui::Button(button_label)) {
                        selected_message_index = i;
                        isWindowOpen = true;
                    }
                }

                ImGui::EndTable();
            }
        }

        if (ImGui::CollapsingHeader("Tests")) {
            if (ImGui::Button("Stress test message framing")) {
                for (int i = 0; i < 1000; ++i) {
                    sam2_room_make_message_t message = {SAM2_MAKE_HEADER};
                    snprintf((char *) &message.room.name, sizeof(message.room.name), "Test message %d", i);
                    sam2_client_send(g_libretro_context.sam2_socket, (char *) &message);
                }
            }
        }

        const char* levelNames[] = {"Debug", "Info", "Warn", "Error", "Fatal"};
        ImGui::SliderInt("Log Level", &g_log_level, 0, 4, levelNames[g_log_level]);

        if (isWindowOpen && selected_message_index != -1) {
            ImGui::Begin("Messages", &isWindowOpen); // Use isWindowOpen to allow closing the window

            char *message = (char *) &g_libretro_context.message_history[selected_message_index];

            ImGui::Text("Header: %.8s", (char *) message);

            if (memcmp(message, sam2_sign_header, SAM2_HEADER_TAG_SIZE) == 0) {
                sam2_signal_message_t *signal_message = (sam2_signal_message_t *) message;
                ImGui::Text("Peer ID: %016" PRIx64, signal_message->peer_id);
                ImGui::InputTextMultiline("ICE SDP", signal_message->ice_sdp, sizeof(signal_message->ice_sdp), ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 16), ImGuiInputTextFlags_ReadOnly);
            } else if (memcmp(message, sam2_make_header, SAM2_HEADER_TAG_SIZE) == 0) {
                sam2_room_make_message_t *make_message = (sam2_room_make_message_t *) message;
                ImGui::Separator();
                show_room(make_message->room);
            } else if (memcmp(message, sam2_list_header, SAM2_HEADER_TAG_SIZE) == 0) {
                if (message[7] == 'r') {
                    // Request
                    ImGui::Text("Room List Request");
                } else {
                    // Response
                    sam2_room_list_message_t *list_response = (sam2_room_list_message_t *) message;
                    ImGui::Separator();
                    show_room(list_response->room);
                }
            } else if (memcmp(message, sam2_join_header, SAM2_HEADER_TAG_SIZE) == 0) {
                sam2_room_join_message_t *join_message = (sam2_room_join_message_t *) message;
                ImGui::Text("Peer ID: %016" PRIx64, join_message->peer_id);
                ImGui::Separator();
                show_room(join_message->room);
            } else if (memcmp(message, sam2_conn_header, SAM2_HEADER_TAG_SIZE) == 0) {
                sam2_connect_message_t *connect_message = (sam2_connect_message_t *) message;
                ImGui::Text("Peer ID: %016" PRIx64, connect_message->peer_id);
                ImGui::Text("Flags: %016" PRIx64, connect_message->flags);
            } else if (memcmp(message, sam2_fail_header, SAM2_HEADER_TAG_SIZE) == 0) {
                sam2_error_message_t *error_response = (sam2_error_message_t *) message;
                ImGui::Text("Code: %" PRId64, error_response->code);
                ImGui::Text("Description: %s", error_response->description);
                ImGui::Text("Peer ID: %016" PRIx64, error_response->peer_id);
            }

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
        ImGui::Text("Our Peer ID:");
        ImGui::SameLine();
        ImGui::TextColored(GOLD, "%" PRIx64, g_ulnet_session.our_peer_id);

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
                    g_libretro_context.SAM2Send((char *) &request);
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

                ImGui::TextColored(color, "%" PRIx64, g_ulnet_session.room_we_are_in.peer_ids[p]);
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
                ImGui::TextColored(color, "Queue: %s", buffer_depth);
            }
        }

        if (g_ulnet_session.room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX] == g_ulnet_session.our_peer_id) {

            ImGui::BeginChild("SpectatorsTableWindow", 
                ImVec2(
                    ImGui::GetContentRegionAvail().x,
                    ImGui::GetWindowContentRegionMax().y / 4
                    ), true);

            ImGui::SeparatorText("Spectators");
            if (ImGui::BeginTable("SpectatorsTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
                ImGui::TableSetupColumn("Peer ID");
                ImGui::TableSetupColumn("ICE Connection");
                ImGui::TableHeadersRow();

                for (int s = 0; s < g_ulnet_session.spectator_count; s++) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);

                    // Display peer ID
                    ImGui::Text("%" PRIx64, g_ulnet_session.spectator_peer_ids[s]);

                    ImGui::TableSetColumnIndex(1);
                    // Display ICE connection status
                    // Assuming g_ulnet_session.agent[] is an array of juice_agent_t* representing the ICE agents
                    juice_agent_t *spectator_agent = g_ulnet_session.agent[SAM2_PORT_MAX+1 + s];
                    if (spectator_agent) {
                        juice_state_t connection_state = juice_get_state(spectator_agent);

                        if (connection_state >= JUICE_STATE_CONNECTED) {
                            ImGui::Text("%s", juice_state_to_string(connection_state));
                        } else {
                            ImGui::TextColored(GREY, "%s %c", juice_state_to_string(connection_state), spinnerGlyph);
                        }

                    } else {
                        ImGui::Text("ICE agent not created %c", spinnerGlyph);
                    }
                }

                ImGui::EndTable();
            }

            ImGui::EndChild();
        }

            sam2_room_join_message_t message = { SAM2_JOIN_HEADER };
            message.room = g_ulnet_session.room_we_are_in;
            if (ulnet_is_authority(&g_ulnet_session)) {
                if (ImGui::Button("Abandon")) {
                    message.room = g_ulnet_session.room_we_are_in;
                    message.room.flags &= ~SAM2_FLAG_ROOM_IS_NETWORK_HOSTED;
                    message.peer_id = g_ulnet_session.our_peer_id;
                    ulnet_process_message(&g_ulnet_session, &message); // *Send* a message to ourselves
                }
            } else if (ulnet_is_spectator(&g_ulnet_session, g_ulnet_session.our_peer_id)) {
                if (   ImGui::Button("Exit")
                    && sam2_get_port_of_peer(&g_ulnet_session.room_we_are_in, g_ulnet_session.our_peer_id) == -1 /* Can't disconnect before leaving the room */) {
                    sam2_signal_message_t response = { SAM2_SIGX_HEADER };
                    response.peer_id = g_ulnet_session.room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX];
                    g_libretro_context.SAM2Send((char *) &response);
                    ulnet_disconnect_peer(&g_ulnet_session, SAM2_AUTHORITY_INDEX);
                    memcpy(&g_ulnet_session.room_we_are_in, &g_new_room_set_through_gui, sizeof(sam2_room_t));
                    g_ulnet_session.room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX] = g_ulnet_session.our_peer_id;
                    g_ulnet_session.frame_counter = 0;
                    g_ulnet_session.state[SAM2_AUTHORITY_INDEX].frame = 0;
                }
            } else {
                if (ImGui::Button("Detach Port")) {
#if 1
                    message.room.peer_ids[ulnet_our_port(&g_ulnet_session)] = SAM2_PORT_AVAILABLE;
                    g_libretro_context.SAM2Send((char *) &message);
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
            ), true);

        static int selected_room_index = -1;  // Initialize as -1 to indicate no selection
        // Table
        if (ImGui::BeginTable("Rooms", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY)) {
            ImGui::TableSetupColumn("Room Name");
            ImGui::TableSetupColumn("Core Hash");
            ImGui::TableSetupColumn("ROM Hash");
            ImGui::TableHeadersRow();

            for (int room_index = 0; room_index < g_sam2_room_count; ++room_index) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                // Make the row selectable and keep track of the selected room
                char label[128];
                sprintf(label, "%s##%016" PRIx64, g_sam2_rooms[room_index].name, g_sam2_rooms[room_index].peer_ids[SAM2_AUTHORITY_INDEX]);
                if (ImGui::Selectable(label, selected_room_index == room_index, ImGuiSelectableFlags_SpanAllColumns)) {
                    selected_room_index = room_index;
                }

                ImGui::TableNextColumn();
                char ports_str[65];
                peer_ids_to_string(g_sam2_rooms[room_index].peer_ids, ports_str);
                ImGui::Text("%s", ports_str);

                ImGui::TableNextColumn();
                ImGui::TextUnformatted(g_sam2_rooms[room_index].core_and_version);

                ImGui::TableNextColumn();
                ImGui::Text("%" PRIx64, g_sam2_rooms[room_index].rom_hash_xxh64);
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
                    ulnet_session_init_defaulted(&g_ulnet_session);
                    g_ulnet_session.room_we_are_in = g_sam2_rooms[selected_room_index];
                    g_ulnet_session.frame_counter = ULNET_WAITING_FOR_SAVE_STATE_SENTINEL;
                    ulnet_startup_ice_for_peer(
                        &g_ulnet_session,
                         g_sam2_rooms[selected_room_index].peer_ids[SAM2_AUTHORITY_INDEX],
                         NULL
                    );
                }
            } else {
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
                ImGui::Button("Spectate");
                ImGui::PopStyleVar();
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

                if (!g_libretro_context.IsAuthority()) {
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
                    g_core_option_for_next_frame = g_ulnet_session.core_options[option_modified_at_index];

                    option_modified_at_index = -1;
                    g_ulnet_session.flags |= ULNET_SESSION_FLAG_CORE_OPTIONS_DIRTY;
                }
            }
        }

        {
            int64_t min_delay_frames = 0;
            int64_t max_delay_frames = ULNET_DELAY_BUFFER_SIZE/2-1;
            if (ImGui::SliderScalar("Network Buffered Frames", ImGuiDataType_S64, &g_ulnet_session.delay_frames, &min_delay_frames, &max_delay_frames, "%lld", ImGuiSliderFlags_None)) {
                strcpy(g_core_option_for_next_frame.key, "netplay_delay_frames");
                sprintf(g_core_option_for_next_frame.value, "%" PRIx64, g_ulnet_session.delay_frames);
            }
        }

        ImGui::Checkbox("Fuzz Input", &g_libretro_context.fuzz_input);
        
        static bool old_vsync_enabled = true;

        if (g_vsync_enabled != old_vsync_enabled) {
            printf("Toggled vsync\n");
            if (SDL_GL_SetSwapInterval((int) g_vsync_enabled) < 0) {
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
                g_ulnet_session.frame_counter % g_ulnet_session.sample_size,
                g_main_loop_cyclic_offset,
            };

            maxVal = -FLT_MAX;
            minVal = FLT_MAX;
            sum = 0.0f;

            for (int i = 0; i < g_ulnet_session.sample_size; ++i) {
                float value = frame_time_dataset[datasetIndex][(i + cyclic_offset[datasetIndex]) % g_ulnet_session.sample_size];
                temp[i] = value;
                maxVal = (value > maxVal) ? value : maxVal;
                minVal = (value < minVal) ? value : minVal;
                sum += value;
            }
            avgVal = sum / g_ulnet_session.sample_size;

            if (datasetIndex == 0) {
                ImGui::Text("Max: %.3f ms  Min: %.3f ms", maxVal, minVal);
                ImGui::Text("Average: %.3f ms  Ideal: %.3f ms", avgVal, 1000.0f / g_av.timing.fps);
            }

            // Set the axis limits before beginning the plot
            ImPlot::SetNextAxisLimits(ImAxis_X1, 0, g_ulnet_session.sample_size, ImGuiCond_Always);
            ImPlot::SetNextAxisLimits(ImAxis_Y1, 0.0f, SAM2_MAX(50.0f, maxVal), ImGuiCond_Always);

            const char* plotTitles[] = {"Frame Time Plot", "Core Wants Tick Plot"};
            if (ImPlot::BeginPlot(plotTitles[datasetIndex], plotSize)) {
                // Plot the histogram
                const char* barTitles[] = {"Frame Times", "Time until core wants to tick"};
                ImPlot::PlotBars(barTitles[datasetIndex], temp, g_ulnet_session.sample_size, 0.67f, -0.5f);

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

static int g_h, g_w;
static void draw_core_frame() {
    int w = 0, h = 0;
    SDL_GetWindowSize(g_win, &w, &h);
    glViewport(0, 0, w, h);

    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(g_shader.program);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_video.tex_id);


    glBindVertexArray(g_shader.vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    glUseProgram(0);
}

static void video_refresh(const void *data, unsigned width, unsigned height, unsigned pitch) {
    if (g_video.clip_w != width || g_video.clip_h != height)
    {
        g_video.clip_h = height;
        g_video.clip_w = width;

        refresh_vertex_data();
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, g_video.tex_id);

    if (pitch != g_video.pitch)
        g_video.pitch = pitch;

    if (data && data != RETRO_HW_FRAME_BUFFER_VALID) {
        glPixelStorei(GL_UNPACK_ROW_LENGTH, g_video.pitch / g_video.bpp);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
                        g_video.pixtype, g_video.pixfmt, data);
    }
}

static void video_deinit() {
    if (g_video.fbo_id)
        glDeleteFramebuffers(1, &g_video.fbo_id);

    if (g_video.tex_id)
        glDeleteTextures(1, &g_video.tex_id);

    if (g_shader.vao)
        glDeleteVertexArrays(1, &g_shader.vao);

    if (g_shader.vbo)
        glDeleteBuffers(1, &g_shader.vbo);

    if (g_shader.program)
        glDeleteProgram(g_shader.program);

    g_video.fbo_id = 0;
    g_video.tex_id = 0;
    g_shader.vao = 0;
    g_shader.vbo = 0;
    g_shader.program = 0;

    SDL_GL_MakeCurrent(g_win, g_ctx);
    SDL_GL_DeleteContext(g_ctx);

    g_ctx = NULL;

    SDL_DestroyWindow(g_win);
}


static void audio_init(int frequency) {
    SDL_AudioSpec spec;
    spec.format = SDL_AUDIO_S16;
    spec.channels = 2;
    spec.freq = frequency;

    g_pcm = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_OUTPUT, &spec, NULL, NULL);
    if (!g_pcm)
        SAM2_LOG_FATAL("Failed to open playback device: %s", SDL_GetError());

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
    case 0: fprintf(stdout, SAM2__GREY    "%s "    "DEBUG "  "%16s | %s", timestamp, g_libretro_context.system_info.library_name, buffer); break;
    case 1: fprintf(stdout, SAM2__DEFAULT "%s "    "INFO  "  "%16s | %s", timestamp, g_libretro_context.system_info.library_name, buffer); break;
    case 2: fprintf(stdout, SAM2__YELLOW  "%s "    "WARN  "  "%16s | %s", timestamp, g_libretro_context.system_info.library_name, buffer); break;
    case 3: SAM2_LOG_WRITE(4, __FILE__, __LINE__, fmt, va); // calls exit(1)
    }
}

static uintptr_t core_get_current_framebuffer() {
    return g_video.fbo_id;
}

int64_t get_unix_time_microseconds();
/**
 * cpu_features_get_time_usec:
 *
 * Gets time in microseconds.
 *
 * Returns: time in microseconds.
 **/
retro_time_t cpu_features_get_time_usec(void) {
    return get_unix_time_microseconds();
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
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY: {
        const char **dir = (const char**)data;
        *dir = ".";
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
    default:
        core_log(RETRO_LOG_DEBUG, "Unhandled env #%u", cmd);
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
    case RETRO_DEVICE_JOYPAD:   return g_libretro_context.InputState[port][to_integral(ERetroDeviceID::JoypadB)     + id];
    case RETRO_DEVICE_LIGHTGUN: return g_libretro_context.InputState[port][to_integral(ERetroDeviceID::LightgunX)   + id];
    case RETRO_DEVICE_ANALOG:   return g_libretro_context.InputState[port][to_integral(ERetroDeviceID::AnalogLeftX) + 2 * index + (id % RETRO_DEVICE_ID_JOYPAD_L2)]; // The indexing logic is broken and might OOBs if we're queried for something that isn't an analog trigger or stick
    case RETRO_DEVICE_POINTER:  return g_libretro_context.InputState[port][to_integral(ERetroDeviceID::PointerX)    + 4 * index + id];
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


static void core_load(const char *sofile) {
    void (*set_environment)(retro_environment_t) = NULL;
    void (*set_video_refresh)(retro_video_refresh_t) = NULL;
    void (*set_input_poll)(retro_input_poll_t) = NULL;
    void (*set_input_state)(retro_input_state_t) = NULL;
    void (*set_audio_sample)(retro_audio_sample_t) = NULL;
    void (*set_audio_sample_batch)(retro_audio_sample_batch_t) = NULL;
    memset(&g_retro, 0, sizeof(g_retro));
    g_retro.handle = SDL_LoadObject(sofile);

    if (!g_retro.handle)
        SAM2_LOG_FATAL("Failed to load core: %s", SDL_GetError()); // @todo I've got to add a x86_64,x86/aarch64,arm32 check here. It does not give reasonable errors on its own

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

static void core_load_game(const char *filename) {
    struct retro_game_info info = { filename };

    info.path = filename;
    info.meta = "";
    info.data = NULL;
    info.size = 0;

    if (filename) {
        g_retro.retro_get_system_info(&g_libretro_context.system_info);

        if (!g_libretro_context.system_info.need_fullpath) {
            SDL_IOStream *file = SDL_IOFromFile(filename, "rb");

            read_whole_file(filename, (void **)&info.data, &info.size);
        }
    }

    if (!g_retro.retro_load_game(&info))
        SAM2_LOG_FATAL("The core failed to load the content.");

    g_retro.retro_get_system_av_info(&g_av);

    if (!g_headless) {
        video_configure(&g_av.geometry);
        audio_init(g_av.timing.sample_rate);
    }

    if (info.data)
        SDL_free((void*)info.data);
}

static void core_unload() {
    if (g_retro.initialized)
        g_retro.retro_deinit();

    if (g_retro.handle)
        SDL_UnloadObject(g_retro.handle);
}

static void noop() {}

void receive_juice_log(juice_log_level_t level, const char *message) {
    static const char *log_level_names[] = {"VERBOSE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"};

    fprintf(stdout, "%s: %s\n", log_level_names[level], message);
    fflush(stdout);
    assert(level < JUICE_LOG_LEVEL_ERROR);
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
            rle_encode32(buffer, g_serialize_size / 4, savebuffer_compressed, &g_zstd_compress_size[g_ulnet_session.frame_counter % g_ulnet_session.sample_size]);
            g_zstd_compress_size[g_ulnet_session.frame_counter % g_ulnet_session.sample_size] *= 4;
        } else {
            g_zstd_compress_size[g_ulnet_session.frame_counter % g_ulnet_session.sample_size] = rle8_encode(buffer, g_serialize_size, savebuffer_compressed); // @todo Technically this can overflow I don't really plan to use it though and I find the odds unlikely
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
                g_zstd_compress_size[g_ulnet_session.frame_counter % g_ulnet_session.sample_size] = ZSTD_compress_usingCDict(cctx, 
                                                                                       savebuffer_compressed, COMPRESSED_SAVE_STATE_BOUND_BYTES,
                                                                                       buffer, g_serialize_size, 
                                                                                       cdict);
            }
        } else {
            g_zstd_compress_size[g_ulnet_session.frame_counter % g_ulnet_session.sample_size] = ZSTD_compress(savebuffer_compressed,
                                                                        COMPRESSED_SAVE_STATE_BOUND_BYTES,
                                                                        buffer, g_serialize_size, g_zstd_compress_level);
        }

    }

    if (ZSTD_isError(g_zstd_compress_size[g_ulnet_session.frame_counter % g_ulnet_session.sample_size])) {
        fprintf(stderr, "Error compressing: %s\n", ZSTD_getErrorName(g_zstd_compress_size[g_ulnet_session.frame_counter % g_ulnet_session.sample_size]));
        g_zstd_compress_size[g_ulnet_session.frame_counter % g_ulnet_session.sample_size] = 0;
    }

    g_zstd_cycle_count[g_ulnet_session.frame_counter % g_ulnet_session.sample_size] = ulnet__rdtsc() - start;

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
        if (strcmp("--headless", argv[i]) == 0) {
            g_headless = true;
        } else if (strcmp("--no-netimgui", argv[i]) == 0) {
            no_netimgui = true;
        } else if (argv[i][0] == '-') {
            SAM2_LOG_FATAL("Unknown option: %s\n", argv[i]);
        }
    }

    if (argc < 2)
        SAM2_LOG_FATAL("Usage: %s <core> [game] [options...]", argv[0]);

    if (   strcmp(g_sam2_address, "localhost")
        || strcmp(g_sam2_address, "127.0.0.1")
        || strcmp(g_sam2_address, "::1")) {
        SAM2_LOG_INFO("g_sam2_address=%s attempting to host signaling and match-making server ourselves", g_sam2_address);
        int server_size_bytes = sam2_server_create(NULL, 0);

        // Create server
        g_sam2_server = (sam2_server_t *) malloc(server_size_bytes);
        int err = sam2_server_create(g_sam2_server, g_sam2_port);
        if (err) {
            SAM2_LOG_INFO("Couldn't create server ourselves probably we're already hosting one in another instance");
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

    void *rom_data = NULL;
    size_t rom_size = 0;
    if (argc > 2) {
        read_whole_file(g_argv[2], &rom_data, &rom_size);
    }

    if (SDL_Init(g_headless ? 0 : SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_EVENTS) < 0)
        SAM2_LOG_FATAL("Failed to initialize SDL");

    // Setup Platform/Renderer backends
    // GL 3.0 + GLSL 130
    g_video.hw.version_major = 4;
    g_video.hw.version_minor = 1;
    g_video.hw.context_type  = RETRO_HW_CONTEXT_OPENGL_CORE;
    g_video.hw.context_reset   = noop;
    g_video.hw.context_destroy = noop;

    // Load the core.
    core_load(argv[1]);

    if (!g_retro.supports_no_game && argc < 3)
        SAM2_LOG_FATAL("This core requires a game in order to run");

    // Load the game.
    core_load_game(argc > 2 ? argv[2] : NULL);

    g_new_room_set_through_gui.rom_hash_xxh64 = ZSTD_XXH64(rom_data, rom_size, 0);
    sam2_format_core_version(
        &g_new_room_set_through_gui,
        g_libretro_context.system_info.library_name,
        g_libretro_context.system_info.library_version
    );

    // Configure the player input devices.
    g_retro.retro_set_controller_port_device(0, RETRO_DEVICE_JOYPAD);

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
                SAM2_LOG_INFO("NetImgui is listening on port %i", g_netimgui_port);
                break;
            }
        }
    }

    if (!g_headless) {
        // Setup Platform/Renderer backends
        ImGui_ImplSDL3_InitForOpenGL(g_win, g_ctx);
        ImGui_ImplOpenGL3_Init("#version 150");
    }

    SDL_Event ev;

    g_ulnet_session.sample_size = ULNET_MAX_SAMPLE_SIZE / 2;

    for (g_main_loop_cyclic_offset = 0; running; g_main_loop_cyclic_offset = (g_main_loop_cyclic_offset + 1) % MAX_SAMPLE_SIZE) {

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
            }
        }

        if (!g_headless) {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
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

        if (g_kbd[SDL_SCANCODE_ESCAPE])
            running = false;

        auto &next_input_state = *ulnet_query_generate_next_input(&g_ulnet_session, &g_core_option_for_next_frame);

        if (next_input_state) {

            for (int i = 0; g_binds[i].k || g_binds[i].rk; ++i) {
                next_input_state[0][g_binds[i].rk] = g_kbd[g_binds[i].k];
            }

            if (g_libretro_context.fuzz_input) {
                for (int i = 0; i < 16; ++i) {
                    next_input_state[0][i] = rand() & 0x0001;
                }
            }
        }


        g_serialize_size = g_retro.retro_serialize_size();
        if (g_serialize_size > sizeof(g_savebuffer[g_save_state_index])) {
            SAM2_LOG_FATAL("Save state buffer is too small (%zu > %zu)", g_serialize_size, sizeof(g_savebuffer[g_save_state_index]));
        }
        int status = ulnet_poll_session(&g_ulnet_session, g_do_zstd_compress, g_savebuffer[g_save_state_index], g_serialize_size, g_av.timing.fps,
            g_retro.retro_run, g_retro.retro_serialize, g_retro.retro_unserialize);

        if (g_do_zstd_compress && (status & ULNET_POLL_SESSION_SAVED_STATE)) {
            tick_compression_investigation((char *)g_savebuffer[g_save_state_index], g_serialize_size, (char*)rom_data, rom_size);

            g_save_state_index = (g_save_state_index + 1) % MAX_SAVE_STATES;
        }

        if (status & ULNET_POLL_SESSION_TICKED) {
            // Keep track of frame-times for plotting purposes
            static int64_t last_tick_usec = get_unix_time_microseconds();
            int64_t current_time_usec = get_unix_time_microseconds();
            int64_t elapsed_time_milliseconds = (current_time_usec - last_tick_usec) / 1000;
            g_frame_time_milliseconds[g_ulnet_session.frame_counter % g_ulnet_session.sample_size] = elapsed_time_milliseconds;
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
                if (uv_run(&g_sam2_server->loop, UV_RUN_NOWAIT) == 0) break;
            }
        }

        g_connected_to_sam2 &= g_sam2_socket != SAM2_SOCKET_INVALID;
        if (g_connected_to_sam2 || (g_connected_to_sam2 = sam2_client_poll_connection(g_sam2_socket, 0))) {
            for (int _prevent_infinite_loop_counter = 0; _prevent_infinite_loop_counter < 64; _prevent_infinite_loop_counter++) {
                static sam2_message_u latest_sam2_message; // This is gradually buffered so it has to be static
                static char buffer[sizeof(sam2_message_u)];
                static int buffer_length = 0;

                int status = sam2_client_poll(
                    g_sam2_socket,
                    &latest_sam2_message,
                    buffer,
                    &buffer_length
                );

                if (status < 0) {
                    SAM2_LOG_ERROR("Error polling sam2 server: %d", status);
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
                        &latest_sam2_message
                    );

                    if (memcmp(&latest_sam2_message, sam2_fail_header, SAM2_HEADER_TAG_SIZE) == 0) {
                        g_last_sam2_error = latest_sam2_message.error_response;
                        SAM2_LOG_ERROR("Received error response from SAM2 (%" PRId64 "): %s", g_last_sam2_error.code, g_last_sam2_error.description);
                    } else if (memcmp(&latest_sam2_message, sam2_list_header, SAM2_HEADER_TAG_SIZE) == 0) {
                        sam2_room_list_message_t *room_list = (sam2_room_list_message_t *) &latest_sam2_message;

                        if (room_list->room.peer_ids[SAM2_AUTHORITY_INDEX] == 0) {
                            g_is_refreshing_rooms = false;
                        } else {
                            if (g_sam2_room_count < SAM2_ARRAY_LENGTH(g_sam2_rooms)) {
                                g_sam2_rooms[g_sam2_room_count++] = room_list->room;
                            }
                        }
                    }
                }
            }
        }
    }
//cleanup:
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

    SDL_Quit();

    return EXIT_SUCCESS;
}
