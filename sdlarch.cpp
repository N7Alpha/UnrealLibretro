#ifdef _WIN32
#define NOMINMAX  1
#endif

#include "sam2.c"
#undef LOG_VERBOSE
#define LOG_VERBOSE(...) do {} while(0)
#include "glad.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include "implot.h"

#include "juice/juice.h"
#include "zstd.h"

#define ZDICT_STATIC_LINKING_ONLY
#include "zdict.h"
#include "rs.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <atomic>
#include <time.h>

#ifdef _WIN32
#else
#include <unistd.h> // for sleep
#endif

#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_opengl.h>
#include "libretro.h"

void startup_ice_for_peer(juice_agent_t **agent, sam2_signal_message_t *signal_message, uint64_t peer_id, bool start_candidate_gathering = true);
// The payload here is regarding the max payload that *we* can use
// We don't want to exceed the MTU because that can result in guranteed lost packets under certain conditions
// Considering various things like UDP/IP headers, STUN/TURN headers, and additional junk 
// load-balancers/routers might add I keep this conservative
#define PACKET_MTU_PAYLOAD_SIZE_BYTES 1408

#define JUICE_CONCURRENCY_MODE JUICE_CONCURRENCY_MODE_USER

#include <chrono>
#include <thread>

void usleep_busy_wait(unsigned int usec) {
    if (usec >= 500) {
        std::this_thread::sleep_for(std::chrono::microseconds(usec));
    } else {
        auto start = std::chrono::high_resolution_clock::now();
        auto end = start + std::chrono::microseconds(usec);
        while (std::chrono::high_resolution_clock::now() < end) {
            // Yield resources to other thread on our core for ~40 * 10 cycles
            // This also saves a little energy
            _mm_pause();
            _mm_pause();
            _mm_pause();
            _mm_pause();
            _mm_pause();
            _mm_pause();
            _mm_pause();
            _mm_pause();
            _mm_pause();
            _mm_pause();
        }
    }
}

template <typename T, std::size_t N>
int locate(const T (&array)[N], const T& value) {
    for (std::size_t i = 0; i < N; ++i) {
        if (array[i] == value) {
            return i;
        }
    }
    return -1;
}

static SDL_Window *g_win = NULL;
static SDL_GLContext g_ctx = NULL;
static SDL_AudioDeviceID g_pcm = 0;
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

static struct retro_variable *g_vars = NULL;

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
    "void main() {\n"
        "gl_FragColor = texture2D(u_tex, o_coord);\n"
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
//	void retro_cheat_reset(void);
//	void retro_cheat_set(unsigned index, bool enabled, const char *code);
	bool (*retro_load_game)(const struct retro_game_info *game);
//	bool retro_load_game_special(unsigned game_type, const struct retro_game_info *info, size_t num_info);
	void (*retro_unload_game)(void);
//	unsigned retro_get_region(void);
//	void *retro_get_memory_data(unsigned id);
//	size_t retro_get_memory_size(unsigned id);
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

#define SPECTATOR_MAX 64
constexpr int PortCount = 4;

typedef int16_t FLibretroInputState[64]; // This must be a POD for putting into packets

static_assert(to_integral(ERetroDeviceID::Size) < sizeof(FLibretroInputState) / sizeof((*(FLibretroInputState *) (0x0))[0]), "FLibretroInputState is too small");

#define CHANNEL_MASK                    0b11110000
#define CHANNEL_VOID                    0b00000000
#define CHANNEL_INPUT                   0b00010000
#define CHANNEL_INPUT_AUDIT_CONSISTENCY 0b00100000
#define CHANNEL_SAVESTATE_TRANSFER      0b00110000
#define CHANNEL_DESYNC_DEBUG            0b11110000

// This is 1 larger than the max frame delay we can handle before blocking
#define INPUT_DELAY_FRAMES_MAX 2

typedef struct {
    uint8_t channel_and_flags;
    uint8_t spacing[7];
    int64_t frame;
    FLibretroInputState input_state[INPUT_DELAY_FRAMES_MAX][PortCount];
} input_packet_t;

typedef struct {
    uint8_t channel_and_flags;
    uint8_t spacing[7];

    int64_t frame;
    int64_t save_state_hash[INPUT_DELAY_FRAMES_MAX];
    int64_t input_state_hash[INPUT_DELAY_FRAMES_MAX];
} desync_debug_packet_t;

struct FLibretroContext {
    juice_agent_t        *agent              [SAM2_PORT_MAX + 1 /* Plus Authority */ + SPECTATOR_MAX] = {0};
    sam2_signal_message_t signal_message     [SAM2_PORT_MAX + 1 /* Plus Authority */ + SPECTATOR_MAX] = {0};
    int64_t               peer_desynced_frame[SAM2_PORT_MAX + 1 /* Plus Authority */ + SPECTATOR_MAX] = {0};
    juice_agent_t **spectator_agent = agent + SAM2_PORT_MAX + 1;
    int64_t spectator_count = 0;

    sam2_signal_message_t *spectator_signal_message = signal_message + SAM2_PORT_MAX + 1;

    sam2_socket_t sam2_socket = 0;
    sam2_room_t room_we_are_in = {0};
    uint64_t our_peer_id = 0;

    input_packet_t input_packet[SAM2_PORT_MAX+1] = {
        { CHANNEL_INPUT }, { CHANNEL_INPUT }, { CHANNEL_INPUT }, { CHANNEL_INPUT },
        { CHANNEL_INPUT }, { CHANNEL_INPUT }, { CHANNEL_INPUT }, { CHANNEL_INPUT }, { CHANNEL_INPUT }
    };
    desync_debug_packet_t desync_debug_packet = { CHANNEL_DESYNC_DEBUG };

    int64_t frame_counter = 0;
    int64_t updated_input_frame = 0;
    FLibretroInputState InputState[PortCount] = {0};
    bool fuzz_input = false;

    uint64_t peer_ready_to_join_bitfield = 0b0;
    int64_t peer_joining_on_frame[SAM2_PORT_MAX+1] = {0};

    void AuthoritySendSaveState(juice_agent_t *agent);

    bool Spectating() const {
        return    room_we_are_in.flags & SAM2_FLAG_ROOM_IS_INITIALIZED 
               && locate(room_we_are_in.peer_ids, our_peer_id) == -1;
    }

    juice_agent_t *AuthorityAgent() {
        return agent[SAM2_AUTHORITY_INDEX];
    }

    juice_agent_t *LocateAgent(uint64_t peer_id) {
        for (int p = 0; p < SAM2_ARRAY_LENGTH(agent); p++) {
            if (agent[p] && signal_message[p].peer_id == peer_id) {
                return agent[p];
            }
        }
    }

    bool FindPeer(juice_agent_t** peer_agent, int* peer_existing_port, uint64_t peer_id) {
        for (int p = 0; p < SAM2_ARRAY_LENGTH(agent); p++) {
            if (agent[p] && signal_message[p].peer_id == peer_id) {
                *peer_agent = agent[p];
                *peer_existing_port = p;
                return true;
            }
        }
        return false;
    }

    void MovePeer(int peer_existing_port, int peer_new_port) {
        assert(peer_existing_port != peer_new_port);
        assert(agent[peer_new_port] == NULL);
        assert(signal_message[peer_new_port].peer_id == 0);
        assert(agent[peer_existing_port] != NULL);
        assert(signal_message[peer_existing_port].peer_id != 0);

        agent[peer_new_port] = agent[peer_existing_port];
        signal_message[peer_new_port] = signal_message[peer_existing_port];
        agent[peer_existing_port] = NULL;
        memset(&signal_message[peer_existing_port], 0, sizeof(signal_message[peer_existing_port]));
    }

    void DisconnectPeer(int peer_port) {
        assert(agent[peer_port] != NULL);
        assert(signal_message[peer_port].peer_id != 0);

        juice_destroy(agent[peer_port]);
        agent[peer_port] = NULL;
        memset(&signal_message[peer_port], 0, sizeof(signal_message[peer_port]));
    }

    void MarkPeerReadyForOtherPeer(uint64_t sender_peer_id, uint64_t joiner_peer_id) {
        int sender_port = locate(room_we_are_in.peer_ids, sender_peer_id);
        int joiner_port = locate(room_we_are_in.peer_ids, joiner_peer_id);
        assert(sender_port != -1);
        assert(joiner_port != -1);
        assert(sender_port != joiner_port);

        peer_ready_to_join_bitfield |= ((1ULL << sender_port) << (8 * joiner_port));
        peer_joining_on_frame[joiner_port] = frame_counter;
    }

    bool IsAuthority() const {
        return our_peer_id == room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX];
    }

    bool AllPeersReadyForPeerToJoin(uint64_t joiner_peer_id) {
        int joiner_port = locate(room_we_are_in.peer_ids, joiner_peer_id);
        assert(joiner_port != -1);

        if (joiner_port == SAM2_AUTHORITY_INDEX) {
            return true;
        }

        return 0xFFULL == (0xFFULL & (peer_ready_to_join_bitfield >> (8 * joiner_port)));
    }

    int OurPort() const {
        int port = locate(room_we_are_in.peer_ids, our_peer_id);

        if (port == -1) {
            return 0; // @todo This should be handled differently I just don't want to out-of-bounds right now
        } else {
            return port;
        }
    }

    int16_t core_input_state(unsigned port, unsigned device, unsigned index, unsigned id);
    void core_input_poll();
};

static struct FLibretroContext g_libretro_context; 
static int64_t &frame_counter = g_libretro_context.frame_counter;

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
    if (!((*(void**)&V) = SDL_LoadFunction(g_retro.handle, #S))) \
        die("Failed to load symbol '" #S "'': %s", SDL_GetError()); \
	} while (0)
#define load_retro_sym(S) load_sym(g_retro.S, S)

static void logical_partition(int sz, int redundant, int *n, int *out_k, int *packet_size, int *packet_groups);

// This is a little confusing since the lower byte of sequence corresponds to the largest stride
static int64_t logical_partition_offset_bytes(uint8_t sequence_hi, uint8_t sequence_lo, int block_size_bytes, int block_stride);

static void die(const char *fmt, ...) {
    char buffer[4096];
    va_list va;
    va_start(va, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, va);
    va_end(va);

    fputs(buffer, stderr);
    fputc('\n', stderr);
    fflush(stderr);

    // Check for a debugger
#ifdef _WIN32
    if (IsDebuggerPresent()) {
        __debugbreak(); // Break into the debugger on Windows
    }
#else
    if (signal(SIGTRAP, SIG_IGN) != SIG_IGN) {
        __builtin_trap(); // Break into the debugger on POSIX systems
    }
#endif

    exit(EXIT_FAILURE);
}

static GLuint compile_shader(unsigned type, unsigned count, const char **strings) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, count, strings, NULL);
    glCompileShader(shader);

    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

    if (status == GL_FALSE) {
        char buffer[4096];
        glGetShaderInfoLog(shader, sizeof(buffer), NULL, buffer);
        die("Failed to compile %s shader: %s", type == GL_VERTEX_SHADER ? "vertex" : "fragment", buffer);
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
        die("Failed to link shader program: %s", buffer);
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
        die("Unsupported hw context %i. (only OPENGL, OPENGL_CORE and OPENGLES2 supported)", g_video.hw.context_type);
    }

    g_win = SDL_CreateWindow("sdlarch", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_OPENGL);

	if (!g_win)
        die("Failed to create window: %s", SDL_GetError());

    g_ctx = SDL_GL_CreateContext(g_win);

    SDL_GL_MakeCurrent(g_win, g_ctx);

    if (!g_ctx)
        die("Failed to create OpenGL context: %s", SDL_GetError());

    if (g_video.hw.context_type == RETRO_HW_CONTEXT_OPENGLES2) {
        if (!gladLoadGLES2Loader((GLADloadproc)SDL_GL_GetProcAddress))
            die("Failed to initialize glad.");
    } else {
        if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress))
            die("Failed to initialize glad.");
    }

    fprintf(stderr, "GL_SHADING_LANGUAGE_VERSION: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
    fprintf(stderr, "GL_VERSION: %s\n", glGetString(GL_VERSION));


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
		die("Failed to create the video texture");

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
		die("Unknown pixel type %u", format);
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

#define MAX_ROOMS 1024

static_assert(sizeof(input_packet_t) == sizeof(int64_t) + sizeof(input_packet_t::frame) + sizeof(input_packet_t::input_state), "Input packet is not packed");
#define SAVESTATE_TRANSFER_FLAG_K_IS_239         0b0001
#define SAVESTATE_TRANSFER_FLAG_SEQUENCE_HI_IS_0 0b0010

typedef struct {
    uint8_t channel_and_flags;
    union {
        uint8_t reed_solomon_k;
        uint8_t packet_groups;
        uint8_t sequence_hi;
    };

    uint8_t sequence_lo;

    uint8_t payload[]; // Variable size; at most PACKET_MTU_PAYLOAD_SIZE_BYTES-3
} savestate_transfer_packet_t;

typedef struct {
    uint8_t channel_and_flags;
    union {
        uint8_t reed_solomon_k;
        uint8_t packet_groups;
        uint8_t sequence_hi;
    };

    uint8_t sequence_lo;

    uint8_t payload[PACKET_MTU_PAYLOAD_SIZE_BYTES-3]; // Variable size; at most PACKET_MTU_PAYLOAD_SIZE_BYTES-3
} savestate_transfer_packet2_t;
static_assert(sizeof(savestate_transfer_packet2_t) == PACKET_MTU_PAYLOAD_SIZE_BYTES, "Savestate transfer is the wrong size");

typedef struct {
    int64_t total_size_bytes; // @todo This isn't necessary
    int64_t frame_counter;
    uint64_t encoding_chain; // @todo probably won't use this
    uint64_t xxhash;

    int64_t zipped_savestate_size;
    uint8_t zipped_savestate[];
} savestate_transfer_payload_t;

int g_argc; 
char **g_argv;

static sam2_room_t g_new_room_set_through_gui = { 
    "My Room Name",
    "TURN host",
    { SAM2_PORT_UNAVAILABLE, SAM2_PORT_AVAILABLE, SAM2_PORT_AVAILABLE, SAM2_PORT_AVAILABLE },
};
sam2_room_t &g_room_we_are_in = g_libretro_context.room_we_are_in;
static sam2_room_t g_sam2_rooms[MAX_ROOMS];
static int64_t g_sam2_room_count = 0;

static bool g_waiting_for_savestate = false;

static juice_agent_t **g_agent = g_libretro_context.agent;
static sam2_signal_message_t *g_signal_message = g_libretro_context.signal_message;

static const char *g_sam2_address = "sam2.cornbass.com";
static sam2_socket_t &g_sam2_socket = g_libretro_context.sam2_socket;
static sam2_request_u g_sam2_request;


static int g_zstd_compress_level = 0;
#define MAX_SAMPLE_SIZE 128
static int g_sample_size = MAX_SAMPLE_SIZE/2;
static uint64_t g_save_cycle_count[MAX_SAMPLE_SIZE] = {0};
static uint64_t g_zstd_cycle_count[MAX_SAMPLE_SIZE] = {1}; // The 1 is so we don't divide by 0
static uint64_t g_zstd_compress_size[MAX_SAMPLE_SIZE] = {0};
static uint64_t g_reed_solomon_encode_cycle_count[MAX_SAMPLE_SIZE] = {0};
static uint64_t g_reed_solomon_decode_cycle_count[MAX_SAMPLE_SIZE] = {0};
static float g_frame_time_milliseconds[MAX_SAMPLE_SIZE];
static uint64_t g_frame_cyclic_offset = 0; // Between 0 and g_sample_size-1
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


#define MAX_REDUNDANT_PACKETS 32
static bool g_do_reed_solomon = false;
static int g_redundant_packets = MAX_REDUNDANT_PACKETS - 1;
static int g_lost_packets = 0;

uint64_t g_remote_savestate_hash = 0x0; // 0x6AEBEEF1EDADD1E5;

#define MAX_SAVE_STATES 64
static unsigned char g_savebuffer[MAX_SAVE_STATES][20 * 1024 * 1024] = {0};

#define SAVE_STATE_COMPRESSED_BOUND_BYTES ZSTD_COMPRESSBOUND(sizeof(g_savebuffer[0]))
#define SAVE_STATE_COMPRESSED_BOUND_WITH_REDUNDANCY_BYTES (255 * SAVE_STATE_COMPRESSED_BOUND_BYTES / (255 - MAX_REDUNDANT_PACKETS))
static unsigned char g_savestate_transfer_payload_untyped[sizeof(savestate_transfer_payload_t) + SAVE_STATE_COMPRESSED_BOUND_WITH_REDUNDANCY_BYTES];
static savestate_transfer_payload_t *g_savestate_transfer_payload = (savestate_transfer_payload_t *) g_savestate_transfer_payload_untyped;
static uint8_t *g_savebuffer_compressed = g_savestate_transfer_payload->zipped_savestate;

static int g_save_state_index = 0;
static int g_save_state_used_for_delta_index_offset = 1;

#define FEC_PACKET_GROUPS_MAX 16
#define FEC_REDUNDANT_BLOCKS 16 // ULNET is hardcoded based on this value so it can't really be changed
const static int savestate_transfer_max_payload = FEC_PACKET_GROUPS_MAX * (GF_SIZE - FEC_REDUNDANT_BLOCKS) * PACKET_MTU_PAYLOAD_SIZE_BYTES;
static void* g_fec_packet[FEC_PACKET_GROUPS_MAX][GF_SIZE - FEC_REDUNDANT_BLOCKS];
static int g_fec_index[FEC_PACKET_GROUPS_MAX][GF_SIZE - FEC_REDUNDANT_BLOCKS];
static int g_fec_index_counter[FEC_PACKET_GROUPS_MAX] = {0}; // Counts packets received in each "packet group"
static unsigned char g_remote_savestate_transfer_packets[SAVE_STATE_COMPRESSED_BOUND_WITH_REDUNDANCY_BYTES + FEC_PACKET_GROUPS_MAX * (GF_SIZE - FEC_REDUNDANT_BLOCKS) * sizeof(savestate_transfer_packet_t)];
static int64_t g_remote_savestate_transfer_offset = 0;
uint8_t g_remote_packet_groups = FEC_PACKET_GROUPS_MAX; // This is used to bookkeep how much data we actually need to receive to reform the complete savestate
static bool g_send_savestate_next_frame = false;

static bool g_is_refreshing_rooms = false;
uint64_t &g_our_peer_id = g_libretro_context.our_peer_id;

static int g_volume = 3;
static bool g_vsync_enabled = true;

static ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
static bool g_connected_to_sam2 = false;
static sam2_response_u g_received_response[2048];
static int64_t g_num_received_response = 0;
static sam2_error_response_t g_last_sam2_error = {SAM2_RESPONSE_SUCCESS};

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


void draw_imgui() {
    static int spinnerIndex = 0;
    char spinnerFrames[4] = { '|', '/', '-', '\\' };
    char spinnerGlyph = spinnerFrames[(spinnerIndex++/4)%4];

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    static bool show_demo_window = false;

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
        double max_reed_solomon_decode_cycle_count = 0;

        for (int i = 0; i < g_sample_size; i++) {
            avg_cycle_count += g_save_cycle_count[i];
            avg_zstd_compress_size += g_zstd_compress_size[i];
            avg_zstd_cycle_count += g_zstd_cycle_count[i];
            if (g_zstd_compress_size[i] > max_compress_size) {
                max_compress_size = g_zstd_compress_size[i];
            }
            if (g_reed_solomon_decode_cycle_count[i] > max_reed_solomon_decode_cycle_count) {
                max_reed_solomon_decode_cycle_count = g_reed_solomon_decode_cycle_count[i];
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

            ImGui::Checkbox("Do Reed Solomon", &g_do_reed_solomon);
            if (g_do_reed_solomon) {
                ImGui::SliderInt("Lost Packets", &g_lost_packets, 0, MAX_REDUNDANT_PACKETS);

                strcpy(unit, "cycles");
                display_count = format_unit_count(max_reed_solomon_decode_cycle_count, unit);
                ImGui::Text("Reed solomon max decode cycle count %.2f %s", display_count, unit);
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

            g_send_savestate_next_frame = ImGui::Button("Send Savestate");
            ImGui::Text("Remote Savestate hash: %llx", g_remote_savestate_hash);
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
                    temp[i] = static_cast<float>(g_save_cycle_count[(i+g_frame_cyclic_offset)%g_sample_size]);
                }
            } else if (current_item == 1) {
                for (int i = 0; i < g_sample_size; ++i) {
                    temp[i] = static_cast<float>(g_zstd_cycle_count[(i+g_frame_cyclic_offset)%g_sample_size]);
                }
            } else if (current_item == 2) {
                for (int i = 0; i < g_sample_size; ++i) {
                    temp[i] = static_cast<float>(g_zstd_compress_size[(i+g_frame_cyclic_offset)%g_sample_size]);
                }
            }
        }

        // Slider to select the current save state index
        ImGui::SliderInt("Save State Index (saved every frame)", &g_save_state_index, 0, MAX_SAVE_STATES-1);
        ImGui::SliderInt("Delta compression frame offset", &g_save_state_used_for_delta_index_offset, 0, MAX_SAVE_STATES-1);
        // Button to add a new save state
        //if (ImGui::Button("Add Save State For Training")) {
        //    if (g_save_state_count < MAX_SAVE_STATES) {
        //        g_save_state_count++;
        //        g_save_state_index = g_save_state_count - 1;
        //    }
        //}
        
        ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
       // ImGui::Checkbox("Another Window", &show_another_window);
        
        ImGui::End();
        {
            ImGui::Begin("Signaling Server and a Match Maker", NULL, ImGuiWindowFlags_AlwaysAutoResize);

            if (g_connected_to_sam2) {
                ImGui::TextColored(ImVec4(0, 1, 0, 1), "Connected to %s:%d", g_sam2_address, SAM2_SERVER_DEFAULT_PORT);
            } else {
                ImGui::TextColored(ImVec4(0.5, 0.5, 0.5, 1), "Connecting to %s:%d %c", g_sam2_address, SAM2_SERVER_DEFAULT_PORT, spinnerGlyph);
                goto finished_drawing_sam2_interface;
            }

            if (g_last_sam2_error.code) {
                ImGui::TextColored(ImVec4(1, 0, 0, 1), "Last error: %s", g_last_sam2_error.description);
            }

            if (ImGui::CollapsingHeader("Responses")) {
                // Create a table with one column for the headers
                if (ImGui::BeginTable("MessagesTable", 1, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                    ImGui::TableSetupColumn("Header", ImGuiTableColumnFlags_WidthFixed, 150.0f);
                    ImGui::TableHeadersRow();

                    for (int64_t i = 0; i < g_num_received_response; ++i) {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);

                        ImGui::Text("%.8s", g_received_response[i]);
                    }

                    ImGui::EndTable();
                }
            }

            if (g_libretro_context.room_we_are_in.flags & SAM2_FLAG_ROOM_IS_INITIALIZED) {
                ImGui::SeparatorText("In Room");
            } else {
                ImGui::SeparatorText("Create a Room");
            }

            sam2_room_t *display_room = g_our_peer_id ? &g_room_we_are_in : &g_new_room_set_through_gui;

            // Editable text fields for room name and TURN host
            ImGui::InputText("##name", display_room->name, sizeof(display_room->name),
                g_our_peer_id ? ImGuiInputTextFlags_ReadOnly : ImGuiInputTextFlags_None);
            ImGui::SameLine();
            ImGui::InputText("##turn_hostname", display_room->turn_hostname, sizeof(display_room->turn_hostname),
                g_our_peer_id ? ImGuiInputTextFlags_ReadOnly : ImGuiInputTextFlags_None);

            // Fixed text fields to display binary values
            //char ports_str[65] = {0};
            char flags_str[65] = {0};

            // Convert the integer values to binary strings
            for (int i = 0; i < 64; i+=4) {
                //ports_str[i/4] = '0' + ((g_room.ports >> (60 - i)) & 0xF);
                flags_str[i/4] = '0' + ((display_room->flags >> (60 - i)) & 0xF);
            }

            ImGui::Text("Flags bitfield: %s", flags_str);
        }

        static bool is_open[SAM2_ARRAY_LENGTH(FLibretroContext::agent)] = {0};

        if (g_libretro_context.room_we_are_in.flags & SAM2_FLAG_ROOM_IS_INITIALIZED) {
            ImGui::Text("Our Peer ID:");
            ImGui::SameLine();
            //0xe0 0xc9 0x1b
            const ImVec4 WHITE(1.0f, 1.0f, 1.0f, 1.0f);
            const ImVec4 GREY(0.5f, 0.5f, 0.5f, 1.0f);
            const ImVec4 GOLD(1.0f, 0.843f, 0.0f, 1.0f);
            const ImVec4 RED(1.0f, 0.0f, 0.0f, 1.0f);
            ImGui::TextColored(GOLD, "%" PRIx64, g_our_peer_id);

            ImGui::SeparatorText("Connection Status");
            for (int p = 0; p < SAM2_PORT_MAX+1; p++) {
                if (p != SAM2_AUTHORITY_INDEX) {
                    ImGui::Text("Port %d:", p);
                } else {
                    ImGui::Text("Authority:");
                }

                ImGui::SameLine();

                if      (g_room_we_are_in.peer_ids[p] == SAM2_PORT_UNAVAILABLE) { ImGui::Text("Unavailable"); }
                else if (g_room_we_are_in.peer_ids[p] == SAM2_PORT_AVAILABLE)   {
                    ImGui::Text("Available");
                    if (g_libretro_context.Spectating()) {
                        ImGui::SameLine();
                        if (ImGui::Button("Join")) {
                            // Send a join room request
                            sam2_room_join_message_t request = {0};
                            request.room = g_room_we_are_in;
                            request.room.peer_ids[p] = g_our_peer_id;
                            g_libretro_context.peer_joining_on_frame[p] = g_libretro_context.frame_counter; // Lower bound
                            sam2_client_send(g_sam2_socket, (char *) &request, SAM2_EMESSAGE_JOIN);
                        }
                    }
                }
                else if (g_room_we_are_in.peer_ids[p] == g_our_peer_id) {
                    ImGui::TextColored(GOLD, "%" PRIx64, g_room_we_are_in.peer_ids[p]);
                }
                else {
                    if (g_agent[p]) {
                        juice_state_t connection_state = juice_get_state(g_agent[p]);

                        ImVec4 color = WHITE;
                        if (   g_room_we_are_in.flags & (SAM2_FLAG_PORT0_PEER_IS_INACTIVE << p)
                            || connection_state != JUICE_STATE_COMPLETED) {
                            color = GREY;
                        } else if (g_libretro_context.peer_desynced_frame[p]) {
                            color = RED;
                        }

                        ImGui::TextColored(color, "%" PRIx64, g_room_we_are_in.peer_ids[p]);

                        ImGui::SameLine();
                        if (connection_state == JUICE_STATE_COMPLETED) {
                            if (g_libretro_context.peer_desynced_frame[p]) {
                                ImGui::TextColored(color, "Peer desynced (frame %" PRId64 ")", g_libretro_context.peer_desynced_frame[p]);
                            } else {
                                char buffer_depth[INPUT_DELAY_FRAMES_MAX+1] = {0};

                                int64_t peer_num_frames_ahead = g_libretro_context.input_packet[p].frame - g_libretro_context.frame_counter;
                                for (int f = 0; f < sizeof(buffer_depth)-1; f++) {
                                    buffer_depth[f] = f < peer_num_frames_ahead ? 'X' : 'O';
                                }

                                ImGui::TextColored(color, "Queue: %s", buffer_depth);
                            }
                        } else {
                            ImGui::TextColored(color, "%s %c", juice_state_to_string(connection_state), spinnerGlyph);
                        }
                    } else {
                        ImGui::Text("ICE agent not created %c", spinnerGlyph);
                    }
                }

                if (g_agent[p]) {
                    ImGui::SameLine();

                    char button_label[32] = {0};
                    snprintf(button_label, sizeof(button_label), "Signal Msg##%d", p);

                    is_open[p] |= ImGui::Button(button_label);
                }
            }

            if (g_room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX] == g_our_peer_id) {

                ImGui::BeginChild("SpectatorsTableWindow", 
                    ImVec2(
                        ImGui::GetContentRegionAvail().x,
                        ImGui::GetWindowContentRegionMax().y / 4
                     ), true);

                ImGui::SeparatorText("Spectators");
                if (ImGui::BeginTable("SpectatorsTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
                    ImGui::TableSetupColumn("Peer ID");
                    ImGui::TableSetupColumn("ICE Connection");
                    ImGui::TableSetupColumn("Signal Msg");
                    ImGui::TableHeadersRow();

                    for (int s = 0; s < g_libretro_context.spectator_count; s++) {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);

                        // Display peer ID
                        ImGui::Text("%" PRIx64, g_libretro_context.signal_message[SAM2_PORT_MAX + 1 + s].peer_id);

                        ImGui::TableSetColumnIndex(1);
                        // Display ICE connection status
                        // Assuming g_agent[] is an array of juice_agent_t* representing the ICE agents
                        if (g_libretro_context.spectator_agent[s]) {
                            juice_state_t connection_state = juice_get_state(g_libretro_context.spectator_agent[s]);

                            if (connection_state >= JUICE_STATE_CONNECTED) {
                                ImGui::Text("%s", juice_state_to_string(connection_state));
                            } else {
                                ImGui::TextColored(GREY, "%s %c", juice_state_to_string(connection_state), spinnerGlyph);
                            }

                        } else {
                            ImGui::Text("ICE agent not created %c", spinnerGlyph);
                        }

                        ImGui::TableSetColumnIndex(2);

                        is_open[SAM2_PORT_MAX + 1 + s] |= ImGui::Button("Signal Msg");
                    }

                    ImGui::EndTable();
                }

                ImGui::EndChild();
            }

            for (int p = 0; p < SAM2_ARRAY_LENGTH(is_open); p++) {
                if (is_open[p]) {
                    ImGui::Begin("Sam2 Signal Message", &is_open[p], ImGuiInputTextFlags_ReadOnly);

                    ImGui::Text("Port: %d", p);
                    // Display header
                    ImGui::Text("Header: %.8s", g_signal_message[p].header);

                    // Display peer_id
                    ImGui::Text("Peer ID: %" PRIx64, g_signal_message[p].peer_id);

                    // Display ice_sdp
                    ImGui::InputTextMultiline("ICE SDP", g_signal_message[p].ice_sdp, sizeof(g_signal_message[p].ice_sdp),
                        ImVec2(0, 0), ImGuiInputTextFlags_ReadOnly);

                    ImGui::End();
                }
            }


            if (ImGui::Button("Exit")) {
                // Send an exit room request
                sam2_room_join_message_t request = {0};
                request.room = g_room_we_are_in;

                for (int p = 0; p < SAM2_PORT_MAX+1; p++) {
                    if (request.room.peer_ids[p] == g_our_peer_id) {
                        request.room.peer_ids[p] = SAM2_PORT_AVAILABLE;
                        break;
                    }
                }

                sam2_client_send(g_sam2_socket, (char *) &request, SAM2_EMESSAGE_JOIN);

                g_our_peer_id = 0;
            }
        } else {
            // Create a "Make" button that sends a make room request when clicked
            if (ImGui::Button("Make")) {
                // Send a make room request
                sam2_room_make_message_t *request = &g_sam2_request.room_make_request;
                request->room = g_new_room_set_through_gui;
                // Fill in the rest of the request fields appropriately...
                sam2_client_send(g_sam2_socket, (char *) &g_sam2_request, SAM2_EMESSAGE_MAKE);
            }
            if (ImGui::Button(g_is_refreshing_rooms ? "Stop" : "Refresh")) {
                // Toggle the state
                g_is_refreshing_rooms = !g_is_refreshing_rooms;

                if (g_is_refreshing_rooms) {
                    g_sam2_room_count = 0;
                    // The list message is only a header
                    sam2_client_send(g_sam2_socket, (char *) &g_sam2_request, SAM2_EMESSAGE_LIST);
                } else {

                }
            }

            // If we're in the "Stop" state
            if (g_is_refreshing_rooms) {
                // Run your "Stop" code here
            }

            ImGui::BeginChild("TableWindow",
                ImVec2(
                    ImGui::GetContentRegionAvail().x,
                    ImGui::GetWindowContentRegionMax().y/2
                ), true);

            static int selected_room_index = -1;  // Initialize as -1 to indicate no selection
            // Table
            if (ImGui::BeginTable("Rooms", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
                ImGui::TableSetupColumn("Room Name");
                ImGui::TableSetupColumn("TURN Host Name");
                ImGui::TableSetupColumn("Peers");
                ImGui::TableHeadersRow();

                for (int room_index = 0; room_index < g_sam2_room_count; ++room_index) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    // Make the row selectable and keep track of the selected room
                    ImGuiSelectableFlags selectable_flags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick;
                    if (ImGui::Selectable(g_sam2_rooms[room_index].name, selected_room_index == room_index, selectable_flags)) {
                        selected_room_index = room_index;
                    }
                    
                    ImGui::TableNextColumn();
                    ImGui::Text("%s", g_sam2_rooms[room_index].turn_hostname);

                    ImGui::TableNextColumn();
                    char ports_str[65];
                    peer_ids_to_string(g_new_room_set_through_gui.peer_ids, ports_str);
                    ImGui::Text("%s", ports_str);
                }

                ImGui::EndTable();
            }

            ImGui::EndChild();

            if (selected_room_index != -1) {

                if (ImGui::Button("Join")) {
                    // Send a join room request
                    sam2_room_join_message_t request = {0};
                    request.room = g_sam2_rooms[selected_room_index];

                    int p = 0;
                    for (p = 0; p < SAM2_PORT_MAX; p++) {
                        if (request.room.peer_ids[p] == SAM2_PORT_AVAILABLE) {
                            request.room.peer_ids[p] = g_libretro_context.our_peer_id;
                            break;
                        }
                    }

                    if (p == SAM2_PORT_MAX) {
                        die("No available ports in the room");
                    }

                    g_libretro_context.peer_joining_on_frame[p] = g_libretro_context.frame_counter; // Lower bound

                    sam2_client_send(g_sam2_socket, (char *) &request, SAM2_EMESSAGE_JOIN);
                }

                ImGui::SameLine();
                if (ImGui::Button("Spectate")) {
                    // Directly signaling the authority just means spectate... @todo I probably should add the room as a field as well though incase I decide on multiple rooms per authority in the future
                    startup_ice_for_peer(
                        &g_agent[SAM2_AUTHORITY_INDEX],
                        &g_signal_message[SAM2_AUTHORITY_INDEX],
                         g_sam2_rooms[selected_room_index].peer_ids[SAM2_AUTHORITY_INDEX]
                    );

                    g_room_we_are_in = g_sam2_rooms[selected_room_index];
                    //sam2_client_send(g_sam2_socket, (char *) &request, SAM2_EMESSAGE_JOIN);
                }
            }
        }
finished_drawing_sam2_interface:
        ImGui::End();
    }

    {
        ImGui::Begin("Timing", NULL, ImGuiWindowFlags_AlwaysAutoResize);

        // Assuming g_argc and g_argv are defined and populated
        if (ImGui::CollapsingHeader("Command Line Arguments")) {
            for (int i = 0; i < g_argc; i++) {
                ImGui::Text("argv[%d]=%s", i, g_argv[i]);
            }
        }
        
        ImGui::Checkbox("Fuzz Input", &g_libretro_context.fuzz_input);
        
        static bool old_vsync_enabled = true;

        if (g_vsync_enabled != old_vsync_enabled) {
            printf("Toggled vsync\n");
            if (SDL_GL_SetSwapInterval((int) g_vsync_enabled) < 0) {
                SDL_Log("Unable to set VSync off: %s", SDL_GetError());
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
                ImGui::SetTooltip("vsync causes stuttering in the core because it blocks and we're single-threaded");
            }
        }

        {
            float temp[MAX_SAMPLE_SIZE];

            for (int i = 0; i < g_sample_size; ++i) {
                temp[i] = static_cast<float>(g_frame_time_milliseconds[(i+g_frame_cyclic_offset)%g_sample_size]);
            }

            ImGui::Text("Core ticks %" PRId64, frame_counter);
            ImGui::Text("Core tick time (ms)");

            ImVec2 plotSize = ImVec2(ImGui::GetContentRegionAvail().x, 150); // Adjust the height as needed

            // Set the axis limits before beginning the plot
            ImPlot::SetNextAxisLimits(ImAxis_X1, 0, g_sample_size, ImGuiCond_Always);
            ImPlot::SetNextAxisLimits(ImAxis_Y1, 0.0f, 33.33f, ImGuiCond_Always);

            if (ImPlot::BeginPlot("Frame Time Plot", plotSize)) {
                // Plot the histogram
                ImPlot::PlotBars("Frame Times", temp, g_sample_size, 0.67f, -0.5f);

                // End the plot
                ImPlot::EndPlot();
            }
        }


        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
        ImGui::End();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
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
    SDL_AudioSpec desired;
    SDL_AudioSpec obtained;

    SDL_zero(desired);
    SDL_zero(obtained);

    desired.format = AUDIO_S16;
    desired.freq   = frequency;
    desired.channels = 2;
    desired.samples = 4096;

    g_pcm = SDL_OpenAudioDevice(NULL, 0, &desired, &obtained, 0);
    if (!g_pcm)
        die("Failed to open playback device: %s", SDL_GetError());

    SDL_PauseAudioDevice(g_pcm, 0);

    // Let the core know that the audio device has been initialized.
    if (audio_callback.set_state) {
        audio_callback.set_state(true);
    }
}


static void audio_deinit() {
    SDL_CloseAudioDevice(g_pcm);
}

static size_t audio_write(const int16_t *buf, unsigned frames) {
    int16_t scaled_buf[4096];
    for (unsigned i = 0; i < frames * 2; i++) {
        scaled_buf[i] = (buf[i] * g_volume) / 100;
    }
    SDL_QueueAudio(g_pcm, scaled_buf, sizeof(*buf) * frames * 2);
    return frames;
}


static void core_log(enum retro_log_level level, const char *fmt, ...) {
	char buffer[4096] = {0};
	static const char * levelstr[] = { "dbg", "inf", "wrn", "err" };
	va_list va;

	va_start(va, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, va);
	va_end(va);

	if (level == 0)
		return;

	fprintf(stderr, "[%s] %s", levelstr[level], buffer);
	fflush(stderr);

	if (level == RETRO_LOG_ERROR)
		exit(EXIT_FAILURE);
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
    return (retro_time_t)SDL_GetTicks() * 1000;
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
    return (retro_perf_tick_t)SDL_GetPerformanceCounter();
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
		*bval = false;
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

// @todo Handle partial join races... I didn't know where to put this todo
void FLibretroContext::core_input_poll() {
    memset(InputState, 0, sizeof(InputState));

    // In the intervening time after we've received a save state, but we haven't joined the room we don't apply our inputs
    // This is so we don't desync since other peers won't apply our inputs either
    // @todo You can barely just not handle this as a special case. My offline input handling just needs more consideration
    if (   frame_counter >= peer_joining_on_frame[OurPort()] 
        && !Spectating()) {
        memcpy(InputState, input_packet[OurPort()].input_state[frame_counter % INPUT_DELAY_FRAMES_MAX][0], sizeof(InputState));
    }

    FLibretroInputState &g_joy = g_libretro_context.InputState[0];
    g_kbd = SDL_GetKeyboardState(NULL);

    if (!Spectating()) {
        for (int p = 0; p < SAM2_PORT_MAX+1; p++) {
            if (room_we_are_in.peer_ids[p] <= SAM2_PORT_SENTINELS_MAX) continue;

            // If the peer was supposed to join on this frame
            if (frame_counter >= g_libretro_context.peer_joining_on_frame[p]) {

                assert(g_libretro_context.AllPeersReadyForPeerToJoin(room_we_are_in.peer_ids[p]));
                assert(input_packet[p].frame <= frame_counter + (INPUT_DELAY_FRAMES_MAX-1));
                assert(input_packet[p].frame >= frame_counter);
                for (int i = 0; i < 16; i++) {
                    g_joy[i] |= input_packet[p].input_state[frame_counter % INPUT_DELAY_FRAMES_MAX][0][i];
                }
            }
        }
    } else {
        // @todo I broke spectating when I changed the protocol this should be easy to add back though just by relaying packets from the authority
#if 1
        assert(!"Spectating not implemented");
#elif
        int timeout_milliseconds = 400;
        int max_polls = 100;
        int i = 0;
        for (; i < max_polls; i++) {
            int64_t smallest_frame = INT64_MAX;
            for (input_packet_t &packet : debug_input_packet_queue.buffer) {
                smallest_frame = std::min(smallest_frame, packet.frame);

                if (packet.frame == frame_counter) {
                    memcpy(InputState, packet.input_state, sizeof(packet.input_state));

                    goto exit_loop;
                }
            }

            char buffer[4096];
            while (juice_user_poll(g_agent[SAM2_AUTHORITY_INDEX], buffer, sizeof(buffer))) {}

            usleep_busy_wait((1000 * timeout_milliseconds) / max_polls);

            if (frame_counter < smallest_frame) {
                die("We can't go back in time!");
            }
        }

        if (i == max_polls) {
            die("Timed out waiting for authority to give frame input while spectating");
        }
exit_loop:;
#endif
    }
}

static void core_input_poll(void) {
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
        die("Failed to load core: %s", SDL_GetError());

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

	puts("Core loaded");
}

static void core_load_game(const char *filename) {
	struct retro_system_info system = {0};
	struct retro_game_info info = { filename, 0 };

    info.path = filename;
    info.meta = "";
    info.data = NULL;
    info.size = 0;

    if (filename) {
        g_retro.retro_get_system_info(&system);

        if (!system.need_fullpath) {
            SDL_RWops *file = SDL_RWFromFile(filename, "rb");
            Sint64 size;

            if (!file)
                die("Failed to load %s: %s", filename, SDL_GetError());

            size = SDL_RWsize(file);

            if (size < 0)
                die("Failed to query game file size: %s", SDL_GetError());

            info.size = size;
            info.data = SDL_malloc(info.size);

            if (!info.data)
                die("Failed to allocate memory for the content");

            if (!SDL_RWread(file, (void*)info.data, info.size, 1))
                die("Failed to read file data: %s", SDL_GetError());

            SDL_RWclose(file);
        }
    }

	if (!g_retro.retro_load_game(&info))
		die("The core failed to load the content.");

	g_retro.retro_get_system_av_info(&g_av);

	video_configure(&g_av.geometry);
	audio_init(g_av.timing.sample_rate);

    if (info.data)
        SDL_free((void*)info.data);

    // Now that we have the system info, set the window title.
    char window_title[255];
    snprintf(window_title, sizeof(window_title), "netplayarch %s %s", system.library_name, system.library_version);
    SDL_SetWindowTitle(g_win, window_title);
}

static void core_unload() {
	if (g_retro.initialized)
		g_retro.retro_deinit();

	if (g_retro.handle)
        SDL_UnloadObject(g_retro.handle);
}

static void noop() {}

static void on_state_changed(juice_agent_t *agent, juice_state_t state, void *user_ptr);
static void on_candidate(juice_agent_t *agent, const char *sdp, void *user_ptr);
static void on_gathering_done(juice_agent_t *agent, void *user_ptr);
static void on_recv(juice_agent_t *agent, const char *data, size_t size, void *user_ptr);

void receive_juice_log(juice_log_level_t level, const char *message) {
    static const char *log_level_names[] = {"VERBOSE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"};

    fprintf(stdout, "%s: %s\n", log_level_names[level], message);
    fflush(stdout);
    assert(level < JUICE_LOG_LEVEL_ERROR);
}

void FLibretroContext::AuthoritySendSaveState(juice_agent_t *agent) {
    size_t serialize_size = g_retro.retro_serialize_size();
    void *savebuffer = malloc(serialize_size);

    LOG_INFO("Before serialize\n");
    fflush(stdout);
    if (!g_retro.retro_serialize(savebuffer, serialize_size)) {
        die("Failed to serialize\n");
    }
    LOG_INFO("After serialize\n");
    fflush(stdout);

    int packet_payload_size_bytes = PACKET_MTU_PAYLOAD_SIZE_BYTES - sizeof(savestate_transfer_packet_t);
    int n, k, packet_groups;
    logical_partition(sizeof(savestate_transfer_payload_t) /* Header */ + ZSTD_COMPRESSBOUND(serialize_size),
                      FEC_REDUNDANT_BLOCKS, &n, &k, &packet_payload_size_bytes, &packet_groups);

    size_t savestate_transfer_payload_plus_parity_bound_bytes = packet_groups * n * packet_payload_size_bytes;

    // This points to the savestate transfer payload, but also the remaining bytes at the end hold our parity blocks
    // Having this data in a single contiguous buffer makes indexing easier
    savestate_transfer_payload_t *savestate_transfer_payload = (savestate_transfer_payload_t *) malloc(savestate_transfer_payload_plus_parity_bound_bytes);

    savestate_transfer_payload->zipped_savestate_size = ZSTD_compress(savestate_transfer_payload->zipped_savestate,
                                                                      SAVE_STATE_COMPRESSED_BOUND_BYTES,
                                                                      savebuffer, serialize_size, g_zstd_compress_level);

    if (ZSTD_isError(savestate_transfer_payload->zipped_savestate_size)) {
        die("ZSTD_compress failed: %s\n", ZSTD_getErrorName(savestate_transfer_payload->zipped_savestate_size));
    }

    logical_partition(sizeof(savestate_transfer_payload_t) /* Header */ + savestate_transfer_payload->zipped_savestate_size,
                      FEC_REDUNDANT_BLOCKS, &n, &k, &packet_payload_size_bytes, &packet_groups);
    SDL_assert(savestate_transfer_payload_plus_parity_bound_bytes >= packet_groups * n * packet_payload_size_bytes); // If this fails my logic calculating the bounds was just wrong

    savestate_transfer_payload->frame_counter = frame_counter;
    savestate_transfer_payload->zipped_savestate_size = savestate_transfer_payload->zipped_savestate_size;
    savestate_transfer_payload->total_size_bytes = sizeof(savestate_transfer_payload_t) + savestate_transfer_payload->zipped_savestate_size;

    uint64_t hash = fnv1a_hash(savestate_transfer_payload, savestate_transfer_payload->total_size_bytes);
    printf("Sending savestate payload with hash: %llx size: %llu bytes\n", hash, savestate_transfer_payload->total_size_bytes);

    // Create parity blocks for Reed-Solomon. n - k in total for each packet group
    // We have "packet grouping" because pretty much every implementation of Reed-Solomon doesn't support more than 255 blocks
    // and unfragmented UDP packets over ethernet are limited to PACKET_MTU_PAYLOAD_SIZE_BYTES
    // This makes the code more complicated and the error correcting properties slightly worse but it's a practical tradeoff
    void *rs_code = fec_new(k, n);
    for (int j = 0; j < packet_groups; j++) {
        void *data[255];
        
        for (int i = 0; i < n; i++) {
            data[i] = (unsigned char *) savestate_transfer_payload + logical_partition_offset_bytes(j, i, packet_payload_size_bytes, packet_groups);
        }

        for (int i = k; i < n; i++) {
            fec_encode(rs_code, (void **)data, data[i], i, packet_payload_size_bytes);
        }
    }
    fec_free(rs_code);

    // Send original data blocks and parity blocks
    // @todo I wrote this in such a way that you can do a zero-copy when creating the packets to send
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < packet_groups; j++) {
            savestate_transfer_packet2_t packet;
            packet.channel_and_flags = CHANNEL_SAVESTATE_TRANSFER;
            if (k == 239) {
                packet.channel_and_flags |= SAVESTATE_TRANSFER_FLAG_K_IS_239;
                if (j == 0) {
                    packet.channel_and_flags |= SAVESTATE_TRANSFER_FLAG_SEQUENCE_HI_IS_0;
                    packet.packet_groups = packet_groups;
                } else {
                    packet.sequence_hi = j;
                }
            } else {
                packet.reed_solomon_k = k;
            }

            packet.sequence_lo = i;

            memcpy(packet.payload, (unsigned char *) savestate_transfer_payload + logical_partition_offset_bytes(j, i, packet_payload_size_bytes, packet_groups), packet_payload_size_bytes);

            int status = juice_send(agent, (char *) &packet, sizeof(savestate_transfer_packet_t) + packet_payload_size_bytes);
            SDL_assert(status == 0);
        }
    }

    free(savebuffer);
    free(savestate_transfer_payload);
}

// On state changed
static void on_state_changed(juice_agent_t *agent, juice_state_t state, void *user_ptr) {
    printf("State: %s\n", juice_state_to_string(state));
    fflush(stdout);
    FLibretroContext *LibretroContext = (FLibretroContext *) user_ptr;

    if (   state == JUICE_STATE_CONNECTED
        && g_our_peer_id == g_room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX]) {
        printf("Sending savestate to peer\n");

        LibretroContext->AuthoritySendSaveState(agent);
    }
}

// On local candidate gathered
static void on_candidate(juice_agent_t *agent, const char *sdp, void *user_ptr) {
    printf("Candidate: %s\n", sdp);

    int p = 0;
    // @todo Factor this out to find_first_occurence_linear_search maybe
    for (; p < SAM2_ARRAY_LENGTH(FLibretroContext::agent); p++) if (agent == g_agent[p]) break;
    if (p == SAM2_ARRAY_LENGTH(FLibretroContext::agent)) {
        printf("No agent found\n");
        return;
    }

    sam2_signal_message_t *response = &g_signal_message[p];

    strcat(response->ice_sdp, sdp);
    strcat(response->ice_sdp, "\n");
}

// On local candidates gathering done
static void on_gathering_done(juice_agent_t *agent, void *user_ptr) {
    printf("Gathering done\n");

    int p = 0;
    for (; p < SAM2_ARRAY_LENGTH(FLibretroContext::agent); p++) if (agent == g_agent[p]) break;
    if (p == SAM2_ARRAY_LENGTH(FLibretroContext::agent)) {
        printf("No agent found\n");
        return;
    }

    sam2_client_send(g_sam2_socket, (char *) &g_signal_message[p], SAM2_EMESSAGE_SIGNAL);
}

// On message received
static void on_recv(juice_agent_t *agent, const char *data, size_t size, void *user_ptr) {
    FLibretroContext *LibretroContext = (FLibretroContext *) user_ptr;

    int p = 0;
    for (; p < SAM2_ARRAY_LENGTH(FLibretroContext::agent); p++) if (agent == g_agent[p]) break;
    if (p == SAM2_ARRAY_LENGTH(FLibretroContext::agent)) {
        die("No agent associated for packet on channel 0x%" PRIx8 "\n", data[0] & 0xF0);
    }

    if (p >= SAM2_PORT_MAX+1) {
        LOG_WARN("A spectator sent us a UDP packet for some reason");
        return;
    }

    uint8_t channel_and_flags = data[0];
    switch (channel_and_flags & CHANNEL_MASK) {
    case CHANNEL_INPUT: {
        assert(size == sizeof(input_packet_t));

        input_packet_t input_packet;
        memcpy(&input_packet, data, sizeof(input_packet_t)); // Strict-aliasing

        LOG_VERBOSE("Recv input packet for frame %" PRId64 " from peer_ids[%d]=%" PRIx64 "\n", input_packet.frame, p, g_room_we_are_in.peer_ids[p]);

        if (   LibretroContext->IsAuthority()
            && input_packet.frame < g_libretro_context.peer_joining_on_frame[p]) {
            LOG_WARN("Received input packet for frame %" PRId64 " but we agreed the client would start sending input on frame %" PRId64 "\n",
                input_packet.frame, LibretroContext->peer_joining_on_frame[p]);
        } else if (input_packet.frame < LibretroContext->input_packet[p].frame) {
            // UDP packets can arrive out of order this is normal
            LOG_VERBOSE("Received outdated input packet for frame %" PRId64 ". We are already on frame %" PRId64 ". Dropping it\n", input_packet.frame, frame_counter);
        } else {
            memcpy(&LibretroContext->input_packet[p], data, sizeof(input_packet_t));
        }

        break;
    }
    case CHANNEL_DESYNC_DEBUG: {
        // @todo This channel doesn't receive messages reliably, but I think it should be changed to in the same manner as the input channel
        assert(size == sizeof(desync_debug_packet_t));

        desync_debug_packet_t their_desync_debug_packet;
        memcpy(&their_desync_debug_packet, data, sizeof(desync_debug_packet_t)); // Strict-aliasing

        desync_debug_packet_t &our_desync_debug_packet = LibretroContext->desync_debug_packet;

        int64_t latest_common_frame = SAM2_MIN(our_desync_debug_packet.frame, their_desync_debug_packet.frame);
        int64_t frame_difference = SAM2_ABS(our_desync_debug_packet.frame - their_desync_debug_packet.frame);
        int64_t total_frames_to_compare = INPUT_DELAY_FRAMES_MAX - frame_difference;
        for (int f = total_frames_to_compare-1; f >= 0 ; f--) {
            int64_t frame_to_compare = latest_common_frame - f;
            if (frame_to_compare < LibretroContext->peer_joining_on_frame[p]) continue; // Gets rid of false postives when the client is joining
            if (frame_to_compare < LibretroContext->peer_joining_on_frame[LibretroContext->OurPort()]) continue; // Gets rid of false postives when we are joining
            int64_t frame_index = frame_to_compare % INPUT_DELAY_FRAMES_MAX;

            if (our_desync_debug_packet.input_state_hash[frame_index] != their_desync_debug_packet.input_state_hash[frame_index]) {
                LOG_ERROR("Input state hash mismatch for frame %" PRId64 " Our hash: %" PRIx64 " Their hash: %" PRIx64 "\n", 
                    frame_to_compare, our_desync_debug_packet.input_state_hash[frame_index], their_desync_debug_packet.input_state_hash[frame_index]);
            } else if (   our_desync_debug_packet.save_state_hash[frame_index]
                       && their_desync_debug_packet.save_state_hash[frame_index]) {

                if (our_desync_debug_packet.save_state_hash[frame_index] != their_desync_debug_packet.save_state_hash[frame_index]) {
                    if (!LibretroContext->peer_desynced_frame[p]) {
                        LibretroContext->peer_desynced_frame[p] = frame_to_compare;
                    }

                    LOG_ERROR("Save state hash mismatch for frame %" PRId64 " Our hash: %" PRIx64 " Their hash: %" PRIx64 "\n",
                        frame_to_compare, our_desync_debug_packet.save_state_hash[frame_index], their_desync_debug_packet.save_state_hash[frame_index]);
                } else if (LibretroContext->peer_desynced_frame[p]) {
                    LibretroContext->peer_desynced_frame[p] = 0;
                    LOG_INFO("Peer resynced frame on frame %" PRId64 "\n", frame_to_compare);
                }
            }
        }

        break;
    }
    case CHANNEL_SAVESTATE_TRANSFER: {
        if (g_agent[SAM2_AUTHORITY_INDEX] != agent) {
            printf("Received savestate transfer packet from non-authority agent\n");
            break;
        }

        if (size < sizeof(savestate_transfer_packet_t)) {
            LOG_WARN("Recv savestate transfer packet with size smaller than header\n");
            break;
        }

        if (size > PACKET_MTU_PAYLOAD_SIZE_BYTES) {
            LOG_WARN("Recv savestate transfer packet potentially larger than MTU\n");
        }

        savestate_transfer_packet_t savestate_transfer_header;
        memcpy(&savestate_transfer_header, data, sizeof(savestate_transfer_packet_t)); // Strict-aliasing

        uint8_t sequence_hi = 0;
        int k = 239;
        if (channel_and_flags & SAVESTATE_TRANSFER_FLAG_K_IS_239) {
            if (channel_and_flags & SAVESTATE_TRANSFER_FLAG_SEQUENCE_HI_IS_0) {
                g_remote_packet_groups = savestate_transfer_header.packet_groups;
            } else {
                sequence_hi = savestate_transfer_header.sequence_hi;
            }
        } else {
            k = savestate_transfer_header.reed_solomon_k;
            g_remote_packet_groups = 1; // k != 239 => 1 packet group
        }

        if (g_fec_index_counter[sequence_hi] == k) {
            // We already have received enough Reed-Solomon blocks to decode the payload; we can ignore this packet
            break;
        }

        uint8_t sequence_lo = savestate_transfer_header.sequence_lo;

        LOG_VERBOSE("Received savestate packet sequence_hi: %hhu sequence_lo: %hhu\n", sequence_hi, sequence_lo);
        fflush(stdout);

        uint8_t *copied_packet_ptr = (uint8_t *) memcpy(g_remote_savestate_transfer_packets + g_remote_savestate_transfer_offset, data, size);
        g_fec_packet[sequence_hi][sequence_lo] = copied_packet_ptr + sizeof(savestate_transfer_packet_t);
        g_remote_savestate_transfer_offset += size;

        g_fec_index[sequence_hi][g_fec_index_counter[sequence_hi]++] = sequence_lo;

        if (g_fec_index_counter[sequence_hi] == k) {
            LOG_VERBOSE("Received all the savestate data for packet group: %hhu\n", sequence_hi);

            int redudant_blocks_sent = k * FEC_REDUNDANT_BLOCKS / (GF_SIZE - FEC_REDUNDANT_BLOCKS);
            void *rs_code = fec_new(k, k + redudant_blocks_sent);
            int rs_block_size = (int) (size - sizeof(savestate_transfer_packet_t));
            int status = fec_decode(rs_code, g_fec_packet[sequence_hi], g_fec_index[sequence_hi], rs_block_size);
            assert(status == 0);
            fec_free(rs_code);

            bool all_data_decoded = true;
            for (int i = 0; i < g_remote_packet_groups; i++) {
                all_data_decoded &= g_fec_index_counter[i] >= k;
            }

            if (all_data_decoded) { 
                uint8_t *savestate_transfer_payload_untyped = (uint8_t *) g_savestate_transfer_payload;
                int64_t remote_payload_size = 0;
                // The last packet contains some number of garbage bytes probably add the size thing back?
                for (int i = 0; i < k; i++) {
                    for (int j = 0; j < g_remote_packet_groups; j++) {
                        memcpy(savestate_transfer_payload_untyped + remote_payload_size, g_fec_packet[j][i], rs_block_size);
                        remote_payload_size += rs_block_size;
                    }
                }

                LOG_INFO("Received savestate transfer payload for frame %" PRId64 "\n", g_savestate_transfer_payload->frame_counter);
                // @todo Check the zipped savestate size against the decoded size so we don't OoBs

                g_remote_savestate_hash = fnv1a_hash(g_savestate_transfer_payload, g_savestate_transfer_payload->total_size_bytes);
                printf("Received savestate payload with hash: %llx size: %llu bytes\n", g_remote_savestate_hash, g_savestate_transfer_payload->total_size_bytes);

                g_zstd_compress_size[0] = ZSTD_decompress(g_savebuffer[0], 
                                sizeof(g_savebuffer[0]),
                                g_savestate_transfer_payload->zipped_savestate, 
                                g_savestate_transfer_payload->zipped_savestate_size);
                
                if (ZSTD_isError(g_zstd_compress_size[0])) {
                    fprintf(stderr, "Error decompressing savestate: %s\n", ZSTD_getErrorName(g_zstd_compress_size[0]));
                    break;
                } else {
                    if (!g_retro.retro_unserialize(g_savebuffer[0], g_zstd_compress_size[0])) {
                        fprintf(stderr, "Failed to load savestate\n");
                        // goto savestate_transfer_failed;
                    } else {
                        LOG_VERBOSE("Save state loaded\n");
                        LibretroContext->frame_counter = g_savestate_transfer_payload->frame_counter;

//                        int64_t max_frames_we_can_advance = INT64_MAX;
//                        for (int p = 0; p < SAM2_PORT_MAX+1; p++) {
//                            if (!g_agent[p]) continue;
//
//                            CircularQueue &peer_input_queue = g_input_packet_queue[p];
//                            if (peer_input_queue.isEmpty()) {
//                                LOG_INFO("We had no input buffered for peer_ids[%d]=%" PRIx64 " after we finished receiving the savestate from the authority\n", p, g_room_we_are_in.peer_ids[p]);
//                                max_frames_we_can_advance = 0;
//                                break;
//                            }
//
//                            // Initialize the frame variable with the frame of the first element in the queue
//                            int64_t last_frame = peer_input_queue.buffer[peer_input_queue.begin].frame;
//
//                            // @todo This loop guards against a dumb assumption that UDP packets arrive in order. The solution without this assumption requires more refactoring I don't want to do currently
//                            for (int i = (peer_input_queue.begin + 1) % peer_input_queue.buffer_size; i != peer_input_queue.end; i = (i + 1) % peer_input_queue.buffer_size) {
//                                // Check if the frame field is increasing by 1
//                                SDL_assert(peer_input_queue.buffer[i].frame == last_frame + 1);
//                                last_frame = peer_input_queue.buffer[i].frame;
//                            }
//
//                            // Toss out any input that is older than the frame we're on this is needed or it breaks the polling logic @todo That should be fixable
//                            while (peer_input_queue.buffer[peer_input_queue.begin].frame < LibretroContext->frame_counter) {
//                                peer_input_queue.dequeue();
//                            }
//
//                            max_frames_we_can_advance = SAM2_MIN(max_frames_we_can_advance, last_frame - LibretroContext->frame_counter);
//                        }

#if 0
                        // @todo Fix the max frames we can advance changes dynamically since we'll be receiving packets while we're advancing
                        // Probably we should be polling and sending out our own input even though we can't see the results of it yet...
                        // Not doing this doesn't break logic but it creates potential for peers to stall while we're catching up
                        LibretroContext->ignore_user_input = true; // Implicitly while we were connecting all our input is considered to be zero
                        for (int f = 0; f < max_frames_we_can_advance; f++) {
                            g_retro.retro_run();
                        }
                        LibretroContext->ignore_user_input = false;
#endif
                    }
                }

                // Reset the savestate transfer state
                g_remote_packet_groups = FEC_PACKET_GROUPS_MAX;
                g_remote_savestate_transfer_offset = 0;
                memset(g_fec_index_counter, 0, sizeof(g_fec_index_counter));
                g_waiting_for_savestate = false;
            }
        }
        break;
    }
    default:
        fprintf(stderr, "Unknown channel: %d\n", channel_and_flags);
        break;
    }

    //printf("Received with size: %lld\nReceived:", size);
    //for (unsigned i = 0; i < sizeof(g_joy_remote) / sizeof(g_joy_remote[0]); i++) {
    //  printf("%x ", g_joy_remote[i]);
    //}
    //printf("\n");
    //fflush(stdout);
}

uint64_t rdtsc() {
#if defined(_MSC_VER)   /* MSVC compiler */
    return __rdtsc();
#elif defined(__GNUC__) /* GCC compiler */
    unsigned int lo, hi;
    __asm__ __volatile__ (
      "rdtsc" : "=a" (lo), "=d" (hi)  
    );
    return ((uint64_t)hi << 32) | lo;
#else
#error "Unsupported compiler"
#endif
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

void rle_encode8(void *input_typeless, size_t inputSize, void *output_typeless, uint64_t *outputUsed) {
    size_t writeIndex = 0;
    size_t readIndex = 0;

    unsigned char *input = (unsigned char *)input_typeless;
    unsigned char *output = (unsigned char *)output_typeless;

    while (readIndex < inputSize) {
        if (input[readIndex] == 0) {
            while (readIndex < inputSize && input[readIndex] == 0) {
                unsigned char zeroCount = 0;
                while (zeroCount < 255 && readIndex < inputSize && input[readIndex] == 0) {
                    zeroCount++;
                    readIndex++;
                }

                output[writeIndex++] = 0; // write a zero to mark the start of a run
                output[writeIndex++] = zeroCount; // write the count of zeros
            }
        } else {
            output[writeIndex++] = input[readIndex++];
        }
    }

    *outputUsed = writeIndex;
}

static void logical_partition(int sz, int redundant, int *n, int *out_k, int *packet_size, int *packet_groups) {
    int k_max = GF_SIZE - redundant;
    *packet_groups = 1;
    int k = (sz - 1) / (*packet_groups * *packet_size) + 1;

    if (k > k_max) {
        *packet_groups = (k - 1) / k_max + 1;
        *packet_size = (sz - 1) / (k_max * *packet_groups) + 1;
        k = (sz - 1) / (*packet_groups * *packet_size) + 1;
    }

    *n = k + k * redundant / k_max;
    *out_k = k;
}

// This is a little confusing since the lower byte of sequence corresponds to the largest stride
static int64_t logical_partition_offset_bytes(uint8_t sequence_hi, uint8_t sequence_lo, int block_size_bytes, int block_stride) {
    return (int64_t) sequence_hi * block_size_bytes + sequence_lo * block_size_bytes * block_stride;
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


void startup_ice_for_peer(juice_agent_t **agent, sam2_signal_message_t *signal_message, uint64_t peer_id, bool start_candidate_gathering) {
    juice_config_t config;
    memset(&config, 0, sizeof(config));

    // STUN server example*
    config.concurrency_mode = JUICE_CONCURRENCY_MODE;
    config.stun_server_host = "stun2.l.google.com"; // @todo Put a bad url here to test how to handle that
    config.stun_server_port = 19302;
    //config.bind_address = "127.0.0.1";

    config.cb_state_changed = on_state_changed;
    config.cb_candidate = on_candidate;
    config.cb_gathering_done = on_gathering_done;
    config.cb_recv = on_recv;

    config.user_ptr = (void *) &g_libretro_context;

    *agent = juice_create(&config);

    memset(signal_message, 0, sizeof(*signal_message));

    signal_message->peer_id = peer_id;

    if (start_candidate_gathering) {
        juice_get_local_description(*agent, signal_message->ice_sdp, sizeof(signal_message->ice_sdp));

        // This call starts an asynchronous task that requires periodic polling via juice_user_poll to complete
        // it will call the on_gathering_done callback once it's finished
        juice_gather_candidates(*agent);
    }
}

// I pulled this out of main because it kind of clutters the logic and to get back some indentation
void tick_compression_investigation(void *rom_data, size_t rom_size) {
    g_serialize_size = g_retro.retro_serialize_size();
    if (sizeof(g_savebuffer[0]) >= g_serialize_size) {
        uint64_t start = rdtsc();
        g_retro.retro_serialize(g_savebuffer[g_save_state_index], sizeof(g_savebuffer[0]));
        g_save_cycle_count[g_frame_cyclic_offset] = rdtsc() - start;
    } else {
        fprintf(stderr, "Save buffer too small to save state\n");
    }

    uint64_t start = rdtsc();
    unsigned char *buffer = g_savebuffer[g_save_state_index];
    static unsigned char g_savebuffer_delta[sizeof(g_savebuffer[0])];
    if (g_do_zstd_delta_compress) {
        buffer = g_savebuffer_delta;
        for (int i = 0; i < g_serialize_size; i++) {
            int delta_index = (g_save_state_index - g_save_state_used_for_delta_index_offset + MAX_SAVE_STATES) % MAX_SAVE_STATES;
            g_savebuffer_delta[i] = g_savebuffer[delta_index][i] ^ g_savebuffer[g_save_state_index][i];
        } 
    }

    if (g_use_rle) {
        // If we're 4 byte aligned use the 4-byte wordsize rle that gives us the highest gains in 32-bit consoles (where we need it the most)
        if (g_serialize_size % 4 == 0) {
            rle_encode32(buffer, g_serialize_size / 4, g_savebuffer_compressed, g_zstd_compress_size + g_frame_cyclic_offset);
            g_zstd_compress_size[g_frame_cyclic_offset] *= 4;
        } else {
            rle_encode8(buffer, g_serialize_size, g_savebuffer_compressed, g_zstd_compress_size + g_frame_cyclic_offset);
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
                g_zstd_compress_size[g_frame_cyclic_offset] = ZSTD_compress_usingCDict(cctx, 
                                                                                       g_savebuffer_compressed, SAVE_STATE_COMPRESSED_BOUND_BYTES,
                                                                                       buffer, g_serialize_size, 
                                                                                       cdict);
            }
        } else {
            g_zstd_compress_size[g_frame_cyclic_offset] = ZSTD_compress(g_savebuffer_compressed,
                                                                        SAVE_STATE_COMPRESSED_BOUND_BYTES,
                                                                        buffer, g_serialize_size, g_zstd_compress_level);
        }

    }

    if (ZSTD_isError(g_zstd_compress_size[g_frame_cyclic_offset])) {
        fprintf(stderr, "Error compressing: %s\n", ZSTD_getErrorName(g_zstd_compress_size[g_frame_cyclic_offset]));
        g_zstd_compress_size[g_frame_cyclic_offset] = 0;
    }

    g_zstd_cycle_count[g_frame_cyclic_offset] = rdtsc() - start;

    // Reed Solomon
    int packet_payload_size = PACKET_MTU_PAYLOAD_SIZE_BYTES - sizeof(savestate_transfer_packet_t);

    int n, k, packet_groups;
    logical_partition(sizeof(savestate_transfer_payload_t) + g_zstd_compress_size[g_frame_cyclic_offset], FEC_REDUNDANT_BLOCKS, &n, &k, &packet_payload_size, &packet_groups);
    //int k = g_zstd_compress_size[g_frame_cyclic_offset]/PACKET_MTU_PAYLOAD_SIZE_BYTES+1;
    //int n = k + g_redundant_packets;
    if (g_do_reed_solomon) {
        #if 0
        char *data[(SAVE_STATE_COMPRESSED_BOUND_BYTES/PACKET_MTU_PAYLOAD_SIZE_BYTES+1+MAX_REDUNDANT_PACKETS)];
        uint64_t remaining = g_zstd_compress_size[g_frame_cyclic_offset];
        int i = 0;
        for (; i < k; i++) {
            uint64_t consume = SAM2_MIN(PACKET_MTU_PAYLOAD_SIZE_BYTES, remaining);
            memcpy(g_savebuffer_compressed_packetized[i], &g_savebuffer_compressed[i * PACKET_MTU_PAYLOAD_SIZE_BYTES], consume);
            remaining -= consume;
            data[i] = (char *) g_savebuffer_compressed_packetized[i];
        }

        memset(&g_savebuffer_compressed_packetized[i + PACKET_MTU_PAYLOAD_SIZE_BYTES - remaining], 0, remaining);

        for (; i < n; i++) {
            data[i] = (char *) g_savebuffer_compressed_packetized[i];
        }

        uint64_t start = rdtsc();
        rs_encode2(k, n, data, PACKET_MTU_PAYLOAD_SIZE_BYTES);
        g_reed_solomon_encode_cycle_count[g_frame_cyclic_offset] = rdtsc() - start;

        for (i = 0; i < g_lost_packets; i++) {
            data[i] = NULL;
        }

        start = rdtsc();
        rs_decode2(k, n, data, PACKET_MTU_PAYLOAD_SIZE_BYTES);
        g_reed_solomon_decode_cycle_count[g_frame_cyclic_offset] = rdtsc() - start;
        #else

        #endif
    }

    if (g_send_savestate_next_frame) {
        g_send_savestate_next_frame = false;

        for (int p = 0; p < SAM2_PORT_MAX+1; p++) {
            if (!g_libretro_context.agent[p]) continue;
            g_libretro_context.AuthoritySendSaveState(g_libretro_context.agent[p]);
        }
    }
}

int main(int argc, char *argv[]) {
    g_argc = argc;
    g_argv = argv;

	if (argc < 2)
		die("usage: %s <core> [game]", argv[0]);

    SDL_SetMainReady();
    juice_set_log_level(JUICE_LOG_LEVEL_INFO);

    g_parameters.d = 8;
    g_parameters.k = 256;
    g_parameters.steps = 4;
    g_parameters.nbThreads = g_zstd_thread_count;
    g_parameters.splitPoint = 0;
    g_parameters.zParams.compressionLevel = g_zstd_compress_level;

    void *rom_data = NULL;
    size_t rom_size = 0;
    if (argc > 2) {
        SDL_RWops *file = SDL_RWFromFile(argv[2], "rb");

        if (!file)
            die("Failed to load %s: %s", argv[2], SDL_GetError());

        rom_size = SDL_RWsize(file);

        if (rom_size < 0)
            die("Failed to query game file size: %s", SDL_GetError());

        rom_data = SDL_malloc(rom_size);

        if (!rom_data)
            die("Failed to allocate memory for the content");

        if (!SDL_RWread(file, (void*)rom_data, rom_size, 1))
            die("Failed to read file data: %s", SDL_GetError());

        //heuristic_byte_swap((uint32_t *)rom_data, rom_size / 4);
        //std::reverse((uint8_t *)rom_data, (uint8_t *)rom_data + rom_size);
        SDL_RWclose(file);
    }

    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_EVENTS) < 0)
        die("Failed to initialize SDL");

    // Setup Platform/Renderer backends
    // GL 3.0 + GLSL 130
    g_video.hw.version_major = 4;
    g_video.hw.version_minor = 5;
    g_video.hw.context_type  = RETRO_HW_CONTEXT_OPENGL_CORE;
    g_video.hw.context_reset   = noop;
    g_video.hw.context_destroy = noop;

    // Load the core.
    core_load(argv[1]);

    if (!g_retro.supports_no_game && argc < 3)
        die("This core requires a game in order to run");

    // Load the game.
    core_load_game(argc > 2 ? argv[2] : NULL);

    // Configure the player input devices.
    g_retro.retro_set_controller_port_device(0, RETRO_DEVICE_JOYPAD);

    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    //IMGUI_CHECKVERSION();
    ImGui::SetCurrentContext(ImGui::CreateContext());
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.IniFilename = NULL; // Don't write an ini file that caches window positions
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForOpenGL(g_win, g_ctx);
    ImGui_ImplOpenGL3_Init(glsl_version);

    SDL_Event ev;

    while (running) {
        auto work_start_time = std::chrono::high_resolution_clock::now();
#if JUICE_CONCURRENCY_MODE == JUICE_CONCURRENCY_MODE_USER
        // We need to poll agents to make progress on the ICE connection
        for (int p = 0; p < SAM2_ARRAY_LENGTH(g_libretro_context.agent); p++) {
            if (!g_libretro_context.agent[p]) continue;

            int ret;
            char buffer[4096];
            while (ret = juice_user_poll(g_agent[p], buffer, sizeof(buffer))) {
                if (ret < 0) {
                    die("Error polling agent (%d)\n", ret);
                }
            }
        }
#endif

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
            ImGui_ImplSDL2_ProcessEvent(&ev);
            switch (ev.type) {
            case SDL_QUIT: running = false; break;
            case SDL_WINDOWEVENT:
                switch (ev.window.event) {
                case SDL_WINDOWEVENT_CLOSE: running = false; break;
                case SDL_WINDOWEVENT_RESIZED:
                    resize_cb(ev.window.data1, ev.window.data2);
                    break;
                }
            }
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
#if 0
        // Timing for frame rate
        struct timespec start_time, end_time;
        clock_gettime(CLOCK_MONOTONIC, &start_time);
#endif

        bool netplay_ready_to_tick = !g_waiting_for_savestate;
        for (int p = 0; p < SAM2_PORT_MAX+1; p++) {
            if (g_libretro_context.OurPort() == p) continue;
            if (g_libretro_context.room_we_are_in.peer_ids[p] > SAM2_PORT_SENTINELS_MAX) {
                netplay_ready_to_tick &= g_libretro_context.AllPeersReadyForPeerToJoin(g_libretro_context.room_we_are_in.peer_ids[p]);

                if (!g_agent[p]) {
                    netplay_ready_to_tick = false;
                } else {
                    juice_state_t state = juice_get_state(g_agent[p]);

                    // Wait until we can send netplay messages to everyone without fail
                    netplay_ready_to_tick &= state == JUICE_STATE_CONNECTED || state == JUICE_STATE_COMPLETED;
                    netplay_ready_to_tick &= g_libretro_context.input_packet[p].frame >= frame_counter;
                }
            }
        }

        //std::chrono::duration<double, std::milli> elapsed_time_milliseconds = current_time - last_frame_time;
        bool core_ready_to_tick = false;
        double core_wants_tick_in_seconds = 0.0;
        {
            static auto start = std::chrono::high_resolution_clock::now();
            static int64_t frames = 0;
            double target_frame_time_seconds = 1.0 / g_av.timing.fps - 1e-3;

            auto current_time = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> elapsed_time_milliseconds = current_time - start;
            double elapsed_time_seconds = elapsed_time_milliseconds.count() / 1000.0;
            double sleep = (frames / g_av.timing.fps) - elapsed_time_seconds;
            if (sleep > 0.0) {
                core_wants_tick_in_seconds = sleep;
            } else {
                frames++;
                core_ready_to_tick = true;

                int64_t authority_is_on_frame = g_libretro_context.input_packet[SAM2_AUTHORITY_INDEX].frame;

                if (   sleep < -target_frame_time_seconds
                    || frame_counter < authority_is_on_frame) { // If over a frame behind don't try to catch up to the next frame
                    start = std::chrono::high_resolution_clock::now();
                    frames = 0;
                }
            }
        }

        // Always send an input packet if the core is ready to tick. This subsumes retransmission logic and generally makes protocol logic less strict
        if (core_ready_to_tick) {
            g_kbd = SDL_GetKeyboardState(NULL);
            
            if (g_kbd[SDL_SCANCODE_ESCAPE])
                running = false;

            if (   !g_our_peer_id
                || g_waiting_for_savestate
                || frame_counter >= g_libretro_context.peer_joining_on_frame[g_libretro_context.OurPort()]
                || g_our_peer_id == g_room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX]) {

                if (g_libretro_context.updated_input_frame < frame_counter) {
                    g_libretro_context.updated_input_frame = frame_counter;

                    for (int i = 0; g_binds[i].k || g_binds[i].rk; ++i) {
                        g_libretro_context.input_packet[g_libretro_context.OurPort()].input_state[frame_counter % INPUT_DELAY_FRAMES_MAX][0][g_binds[i].rk] = g_kbd[g_binds[i].k];
                    }

                    if (g_libretro_context.fuzz_input) {
                        for (int i = 0; i < 16; ++i) {
                            g_libretro_context.input_packet[g_libretro_context.OurPort()].input_state[frame_counter % INPUT_DELAY_FRAMES_MAX][0][i] = rand() & 0x0001;
                        }
                    }
                }
            }

            if (   g_libretro_context.room_we_are_in.flags & SAM2_FLAG_ROOM_IS_INITIALIZED
                && g_libretro_context.AllPeersReadyForPeerToJoin(g_libretro_context.our_peer_id)
                && frame_counter >= g_libretro_context.peer_joining_on_frame[g_libretro_context.OurPort()]) {
                g_libretro_context.input_packet[g_libretro_context.OurPort()].frame = frame_counter;

                for (int p = 0; p < SAM2_PORT_MAX+1; p++) {
                    if (!g_agent[p]) continue;
                    juice_state_t state = juice_get_state(g_agent[p]);

                    // Wait until we can send netplay messages to everyone without fail
                    if (state == JUICE_STATE_CONNECTED || state == JUICE_STATE_COMPLETED) {
                        juice_send(g_agent[p], (const char *) &g_libretro_context.input_packet[g_libretro_context.OurPort()], sizeof(g_libretro_context.input_packet[0]));
                        LOG_VERBOSE("Sent input packet for frame %" PRId64 " dest peer_ids[%d]=%" PRIx64 "\n", g_libretro_context.input_packet.frame, p, g_room_we_are_in.peer_ids[p]);
                        fflush(stdout);
                    }
                }
            }
        }

        if (netplay_ready_to_tick && core_ready_to_tick) {
            g_retro.retro_run();
            frame_counter++;

            // Keep track of frame-times for plotting purposes
            static auto last_tick_time = std::chrono::high_resolution_clock::now();
            auto current_time = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> elapsed_time_milliseconds = current_time - last_tick_time;
            g_frame_time_milliseconds[g_frame_cyclic_offset] = (float) elapsed_time_milliseconds.count();
            last_tick_time = current_time;

            int64_t savestate_hash = 0;
            if (g_do_zstd_compress) {
                tick_compression_investigation(rom_data, rom_size);

                savestate_hash = fnv1a_hash(g_savebuffer[g_save_state_index], g_serialize_size);

                g_save_state_index = (g_save_state_index + 1) % MAX_SAVE_STATES;
            }

            if (g_libretro_context.room_we_are_in.flags & SAM2_FLAG_ROOM_IS_INITIALIZED) {
                //desync_debug_packet_t desync_debug_packet = { CHANNEL_DESYNC_DEBUG };
                g_libretro_context.desync_debug_packet.frame          = (frame_counter-1); // This was for the previous frame
                g_libretro_context.desync_debug_packet.save_state_hash[ (frame_counter-1) % INPUT_DELAY_FRAMES_MAX] = savestate_hash;
                g_libretro_context.desync_debug_packet.input_state_hash[(frame_counter-1) % INPUT_DELAY_FRAMES_MAX] = fnv1a_hash(g_libretro_context.InputState, sizeof(g_libretro_context.InputState));

                for (int p = 0; p < SAM2_ARRAY_LENGTH(g_libretro_context.agent); p++) {
                    if (!g_agent[p]) continue;

                    juice_send(g_agent[p], (char *) &g_libretro_context.desync_debug_packet, sizeof(g_libretro_context.desync_debug_packet));
                }
            }

            g_frame_cyclic_offset = (g_frame_cyclic_offset + 1) % g_sample_size;
        }

        // The imgui frame is updated at the monitor refresh cadence
        // So the core frame needs to be redrawn or you'll get the Windows XP infinite window thing
        draw_core_frame();
        draw_imgui();

        // We hope vsync is disabled or else this will block
        // I think you have to write platform specific code / not use OpenGL if you want this to be non-blocking
        // and still try to update on vertical sync
        SDL_GL_SwapWindow(g_win);

        if (g_sam2_socket == 0) {
            if (sam2_client_connect(&g_sam2_socket, g_sam2_address, SAM2_SERVER_DEFAULT_PORT) == 0) {
                printf("Socket created successfully SAM2\n");
            }
        }

        if (g_connected_to_sam2 || (g_connected_to_sam2 = sam2_client_poll_connection(g_sam2_socket, 0))) {
            static sam2_message_e response_tag = SAM2_EMESSAGE_NONE;
            static int response_length = 0;
            for (;;) {
                sam2_response_u &response = g_received_response[g_num_received_response];
                int status = sam2_client_poll(g_sam2_socket, &response, &response_tag, &response_length);

                if (status < 0) {
                    die("TCP Stream state corrupted exiting...\n");
                }

                if (   response_tag == SAM2_EMESSAGE_PART
                    || response_tag == SAM2_EMESSAGE_NONE) {
                    break;
                } else {
                    if (g_num_received_response+1 < SAM2_ARRAY_LENGTH(g_received_response)) {
                        g_num_received_response++;
                    }
                }

                switch (response_tag) {
                case SAM2_EMESSAGE_ERROR: {
                    g_last_sam2_error = response.error_response;
                    printf("Received error response from SAM2 (%" PRIx64 "): %s\n", g_last_sam2_error.code, g_last_sam2_error.description);
                    fflush(stdout);
                    break;
                }
                case SAM2_EMESSAGE_LIST: {
                    sam2_room_list_response_t *room_list = &response.room_list_response;
                    printf("Received list of %lld games from SAM2\n", (long long int) room_list->room_count);
                    fflush(stdout);
                    for (int i = 0; i < room_list->room_count; i++) {
                        //printf("  %s turn_host:%s\n", room_list->rooms[i].name, room_list->rooms[i].turn_hostname);
                    }

                    fflush(stdout);
                    int64_t rooms_to_copy = SAM2_MIN(room_list->room_count, (int64_t) MAX_ROOMS - g_sam2_room_count);
                    memcpy(g_sam2_rooms + g_sam2_room_count, room_list->rooms, rooms_to_copy * sizeof(sam2_room_t));
                    g_sam2_room_count += rooms_to_copy;
                    g_is_refreshing_rooms = g_sam2_room_count != room_list->server_room_count;
                    break;
                }
                case SAM2_EMESSAGE_MAKE: {
                    sam2_room_make_message_t *room_make = &response.room_make_response;
                    printf("Received room make response from SAM2\n");
                    fflush(stdout);
                    assert(g_libretro_context.our_peer_id == room_make->room.peer_ids[SAM2_AUTHORITY_INDEX]);
                    g_libretro_context.room_we_are_in = room_make->room;
                    break;
                }
                case SAM2_EMESSAGE_CONN: {
                    sam2_connect_message_t *connect_message = &response.connect_message;
                    printf("We were assigned the peer id %" PRIx64 "\n", connect_message->peer_id);
                    fflush(stdout);

                    g_libretro_context.our_peer_id = connect_message->peer_id;

                    break;
                }
                case SAM2_EMESSAGE_JOIN: {
                    sam2_room_join_message_t *room_join = &response.room_join_response;

                    if (sam2_same_room(&g_room_we_are_in, &room_join->room)) {
                        if (room_join->room.peer_ids[g_libretro_context.OurPort()] == SAM2_PORT_AVAILABLE) {

                        }
                    } else {
                        LOG_INFO("We switched rooms or joined a room for the first time %s\n", room_join->room.name);

                        // @todo This should be actually handled, but it conflicts with the if statement below
                    }

                    if (!sam2_same_room(&g_room_we_are_in, &room_join->room)) {
                        printf("We were let into the server by the authority\n");
                        fflush(stdout);

                        g_waiting_for_savestate = true;
                        g_room_we_are_in = room_join->room;
                        g_libretro_context.peer_joining_on_frame[g_libretro_context.OurPort()] = INT64_MAX; // Upper bound

                        for (int p = 0; p < SAM2_ARRAY_LENGTH(room_join->room.peer_ids); p++) {
                            if (room_join->room.peer_ids[p] <= SAM2_PORT_SENTINELS_MAX) continue;
                            if (room_join->room.peer_ids[p] == g_our_peer_id) continue;
                            if (g_agent[p]) {
                                // If we're spectating this should be true for the authority
                                assert(g_signal_message[p].peer_id == room_join->room.peer_ids[p]);
                                continue;
                            }

                            g_libretro_context.peer_ready_to_join_bitfield |= (0xFFULL << SAM2_PORT_MAX * p); // @todo This should be based on a bitflag in the room

                            startup_ice_for_peer(&g_agent[p], &g_signal_message[p], room_join->room.peer_ids[p]);
                        }
                    } else {
                        if (g_our_peer_id == room_join->room.peer_ids[SAM2_AUTHORITY_INDEX]) {
                            printf("Someone has asked us to change the state of the server in some way e.g. leaving, joining, etc.\n");
                            fflush(stdout);

                            int sender_port = locate(room_join->room.peer_ids, room_join->peer_id);

                            if (sender_port == -1) {
                                printf("They didn't specify which port they're joining on\n");
                                fflush(stdout);

                                sam2_error_response_t error = {
                                    SAM2__ERROR_HEADER,
                                    SAM2_RESPONSE_AUTHORITY_ERROR,
                                    "Client didn't try to join on any ports",
                                    room_join->peer_id
                                };

                                sam2_client_send(g_sam2_socket, (char *) &error, SAM2_EMESSAGE_ERROR);
                                break;
                            } else {
                                //g_room_we_are_in.flags |= SAM2_FLAG_PORT0_PEER_IS_INACTIVE << p;

                                juice_agent_t *peer_agent;
                                int peer_existing_port;
                                if (g_libretro_context.FindPeer(&peer_agent, &peer_existing_port, room_join->room.peer_ids[sender_port])) {
                                    if (peer_existing_port != sender_port) {
                                        g_libretro_context.MovePeer(peer_existing_port, sender_port); // This only moves spectators to real ports right now
                                    } else {
                                        LOG_INFO("Peer %" PRIx64 " has asked to change something about the room\n", room_join->room.peer_ids[sender_port]);
                                        fflush(stdout);

                                    }
                                } else {
                                    if (g_room_we_are_in.peer_ids[sender_port] != SAM2_PORT_AVAILABLE) {
                                        sam2_error_response_t error = {
                                            SAM2__ERROR_HEADER,
                                            SAM2_RESPONSE_AUTHORITY_ERROR,
                                            "Peer tried to join on unavailable port",
                                            room_join->peer_id
                                        };

                                        sam2_client_send(g_sam2_socket, (char *) &error, SAM2_EMESSAGE_ERROR);
                                        break;
                                    } else {
                                        printf("Peer %" PRIx64 " was let in by us the authority\n", room_join->room.peer_ids[sender_port]);
                                        fflush(stdout);

                                        g_libretro_context.peer_joining_on_frame[sender_port] = frame_counter;
                                        g_libretro_context.peer_ready_to_join_bitfield &= ~(0xFFULL << (8 * sender_port));

                                        for (int peer_port = 0; peer_port < SAM2_PORT_MAX; peer_port++) {
                                            if (g_libretro_context.room_we_are_in.peer_ids[peer_port] <= SAM2_PORT_SENTINELS_MAX) {
                                                g_libretro_context.peer_ready_to_join_bitfield |= 1ULL << (SAM2_PORT_MAX * sender_port + peer_port);
                                            }
                                        }

                                        g_libretro_context.room_we_are_in.peer_ids[sender_port] = room_join->room.peer_ids[sender_port];
                                        sam2_room_join_message_t response = {0};
                                        response.room = g_libretro_context.room_we_are_in;
                                        sam2_client_send(g_sam2_socket, (char *) &response, SAM2_EMESSAGE_JOIN); // This must come before the next call

                                        startup_ice_for_peer(
                                            &g_libretro_context.agent[sender_port],
                                            &g_signal_message[sender_port],
                                            room_join->room.peer_ids[sender_port]
                                        );

                                        // @todo This check is basically duplicated code with the code in the ACKJ handler
                                        if (g_libretro_context.AllPeersReadyForPeerToJoin(room_join->room.peer_ids[sender_port])) {
                                            // IS THIS NECESSARY? It doesn't seem like it so far with one connection

                                            sam2_room_acknowledge_join_message_t response = {0};
                                            response.room = g_libretro_context.room_we_are_in;
                                            response.joiner_peer_id = room_join->room.peer_ids[sender_port];
                                            response.frame_counter = g_libretro_context.peer_joining_on_frame[sender_port];

                                            sam2_client_send(g_libretro_context.sam2_socket, (char *) &response, SAM2_EMESSAGE_ACKJ);
                                        }

                                        break; // @todo I don't like this break
                                    }
                                }

                                sam2_client_send(g_sam2_socket, (char *) &response, SAM2_EMESSAGE_JOIN);
                            }
                        } else {
                            LOG_INFO("Something about the room we're in was changed by the authority\n");

                            assert(sam2_same_room(&g_room_we_are_in, &room_join->room));

                            for (int p = 0; p < SAM2_ARRAY_LENGTH(room_join->room.peer_ids); p++) {
                                // @todo Check something other than just joins and leaves
                                if (room_join->room.peer_ids[p] != g_room_we_are_in.peer_ids[p]) {
                                    if (room_join->room.peer_ids[p] == SAM2_PORT_AVAILABLE) {
                                        printf("Peer %" PRIx64 " has left the room\n", g_room_we_are_in.peer_ids[p]);
                                        fflush(stdout);

                                        if (g_agent[p]) {
                                            juice_destroy(g_agent[p]);
                                            g_agent[p] = NULL;
                                        }

                                        if (g_our_peer_id == g_room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX]) {
                                            //g_room_we_are_in.flags |= SAM2_FLAG_PORT0_PEER_IS_INACTIVE << p;
                                        }

                                        g_room_we_are_in.peer_ids[p] = SAM2_PORT_AVAILABLE;
                                    } else {
                                        printf("Peer %" PRIx64 " has joined the room\n", room_join->room.peer_ids[p]);
                                        fflush(stdout);

                                        startup_ice_for_peer(&g_agent[p], &g_signal_message[p], room_join->room.peer_ids[p]);

                                        sam2_room_acknowledge_join_message_t response = {0};
                                        response.room = g_libretro_context.room_we_are_in;
                                        response.joiner_peer_id = g_room_we_are_in.peer_ids[p] = room_join->room.peer_ids[p];
                                        response.frame_counter = g_libretro_context.peer_joining_on_frame[p] = frame_counter; // Lower bound

                                        sam2_client_send(g_libretro_context.sam2_socket, (char *) &response, SAM2_EMESSAGE_ACKJ);
                                    }
                                }
                            }
                        }
                    }
                    break;
                }
                case SAM2_EMESSAGE_ACKJ: {
                    sam2_room_acknowledge_join_message_t *acknowledge_room_join_message = &response.room_acknowledge_join_message;

                    int joiner_port = locate(g_libretro_context.room_we_are_in.peer_ids, acknowledge_room_join_message->joiner_peer_id);
                    assert(joiner_port != -1);

                    if (g_libretro_context.our_peer_id == g_room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX]) {
                        g_libretro_context.MarkPeerReadyForOtherPeer(
                            acknowledge_room_join_message->sender_peer_id,
                            acknowledge_room_join_message->joiner_peer_id
                        );

                        g_libretro_context.peer_joining_on_frame[joiner_port] = SAM2_MAX(
                            g_libretro_context.peer_joining_on_frame[joiner_port],
                            acknowledge_room_join_message->frame_counter
                        );

                        if (g_libretro_context.AllPeersReadyForPeerToJoin(acknowledge_room_join_message->joiner_peer_id)) {
                            sam2_room_acknowledge_join_message_t response = {0};
                            response.room = g_libretro_context.room_we_are_in;
                            response.joiner_peer_id = acknowledge_room_join_message->joiner_peer_id;
                            response.frame_counter = g_libretro_context.peer_joining_on_frame[joiner_port];

                            sam2_client_send(g_libretro_context.sam2_socket, (char *) &response, SAM2_EMESSAGE_ACKJ);
                        } else {
                            LOG_INFO("Peer %" PRIx64 " has been acknowledged by %" PRIx64 " but not all peers\n", 
                                acknowledge_room_join_message->joiner_peer_id, acknowledge_room_join_message->sender_peer_id);
                        }
                    } else {
                        assert(acknowledge_room_join_message->sender_peer_id == g_room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX]);
                        assert(acknowledge_room_join_message->frame_counter >= g_libretro_context.peer_joining_on_frame[joiner_port] || g_libretro_context.our_peer_id == acknowledge_room_join_message->joiner_peer_id);
                        LOG_INFO("Authority told us peer %" PRIx64 " has been acknowledged by all peers and is joining on frame %" PRId64 " (our current frame %" PRId64 ")\n", 
                            acknowledge_room_join_message->joiner_peer_id, acknowledge_room_join_message->frame_counter, g_libretro_context.frame_counter);

                        g_libretro_context.peer_ready_to_join_bitfield |= 0xFFULL << (8 * joiner_port);
                        g_libretro_context.peer_joining_on_frame[joiner_port] = acknowledge_room_join_message->frame_counter;
                    }
                    break;
                }
                case SAM2_EMESSAGE_SIGNAL: {
                    sam2_signal_message_t *room_signal = (sam2_signal_message_t *) &response;
                    printf("Received signal from peer %" PRIx64 "\n", room_signal->peer_id);
                    fflush(stdout);

                    int p = locate(g_room_we_are_in.peer_ids, room_signal->peer_id);

                    // Signaling is different for spectators and players
                    if (p == -1) {
                        int s = g_libretro_context.spectator_count;
                        printf("Received signal from unknown peer\n");
                        fflush(stdout);

                        if (g_our_peer_id == g_room_we_are_in.peer_ids[SAM2_AUTHORITY_INDEX]) {
                            if (s == SPECTATOR_MAX) {
                                printf("We can't let them in as a spectator there are too many spectators\n");

                                static sam2_error_response_t error = { 
                                    SAM2__ERROR_HEADER, 
                                    SAM2_RESPONSE_AUTHORITY_ERROR,
                                    "Authority has reached the maximum number of spectators"
                                };

                                sam2_client_send(g_sam2_socket, (char *) &error, SAM2_EMESSAGE_ERROR);
                            } else {
                                printf("We are letting them in as a spectator\n");
                                fflush(stdout);

                                startup_ice_for_peer(
                                    &g_libretro_context.spectator_agent[g_libretro_context.spectator_count],
                                    &g_libretro_context.spectator_signal_message[g_libretro_context.spectator_count],
                                    room_signal->peer_id,
                                    /* start_candidate_gathering = */ false
                                );

                                juice_set_remote_description(g_libretro_context.spectator_agent[g_libretro_context.spectator_count], room_signal->ice_sdp);
                                juice_set_remote_gathering_done(g_libretro_context.spectator_agent[g_libretro_context.spectator_count]);
                                juice_get_local_description(
                                    g_libretro_context.spectator_agent[g_libretro_context.spectator_count],
                                    g_libretro_context.spectator_signal_message[g_libretro_context.spectator_count].ice_sdp,
                                    sizeof(sam2_signal_message_t::ice_sdp)
                                );

                                // This call starts an asynchronous task that requires periodic polling via juice_user_poll to complete
                                // it will call the on_gathering_done callback once it's finished
                                juice_gather_candidates(g_libretro_context.spectator_agent[g_libretro_context.spectator_count]);
                                g_libretro_context.spectator_count++;
                            }
                        } else {
                            printf("Received unknown signal when we weren't the authority\n");
                            fflush(stdout);

                            static sam2_error_response_t error = { 
                                SAM2__ERROR_HEADER, 
                                SAM2_RESPONSE_AUTHORITY_ERROR,
                                "Received unknown signal when we weren't the authority"
                            };

                        }
                    } else {
                        printf("Received signal from known peer\n");
                        juice_set_remote_description(g_agent[p], room_signal->ice_sdp);
                        juice_set_remote_gathering_done(g_agent[p]);
                        fflush(stdout);
                    }
                    break;
                }
                default:
                    fprintf(stderr, "Received unknown message (%d) from SAM2\n", (int) response_tag);
                    break;

                }
            }
        }
        

#if 0
        // Compute how long the processing took
        clock_gettime(CLOCK_MONOTONIC, &end_time);
        long elapsed = (end_time.tv_sec - start_time.tv_sec) * 1000 + (end_time.tv_nsec - start_time.tv_nsec) / 1000000; // milliseconds
        
        // Compute how much we should sleep to maintain the desired frame rate
        int frame_time = (int)round(1000.0 / g_av.timing.fps); // Frame time in milliseconds
        int sleep_time = frame_time - elapsed; // How much time left to sleep

        // If the processing was quicker than frame time, sleep the remaining time
        if (sleep_time > 0) {
            struct timespec sleep_duration;
            sleep_duration.tv_sec = sleep_time / 1000;
            sleep_duration.tv_nsec = (sleep_time % 1000) * 1000000;
            nanosleep(&sleep_duration, NULL);
        }
#endif
        double monitor_wants_refresh_seconds = 0.0;
        if (!g_vsync_enabled) {
            // Handle timing for refreshing the OS window
            // this has been separated from ticking the core as nowadays many PC monitors aren't 60Hz
            static SDL_DisplayMode mode = {0};
            if (mode.refresh_rate == 0) {
                if (SDL_GetCurrentDisplayMode(0, &mode) != 0) {
                    SDL_Log("SDL_GetCurrentDisplayMode failed: %s", SDL_GetError());
                    // We might be on a linux server or something without a display
                    // Just set are refresh rate to 60 since this will make timing better
                    mode.refresh_rate = 60;
                }
            }

            double frameDelay = 1e6 / mode.refresh_rate;
            static auto lastSwap = std::chrono::high_resolution_clock::now();

            auto now = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - lastSwap);
            monitor_wants_refresh_seconds = SAM2_MAX(0.0, (frameDelay - duration.count()) / 1e6);
            lastSwap = now;
        }

        // Poor man's coroutine's/scheduler
        double work_elapsed_time_microseconds = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - work_start_time).count();
        double sleep = SAM2_MIN(core_wants_tick_in_seconds, monitor_wants_refresh_seconds);
        sleep = SAM2_MAX(sleep - (work_elapsed_time_microseconds / 1e6), 0.0);

        //printf("%g\n", sleep);
        //fflush(stdout);
        usleep_busy_wait((unsigned int) (sleep * 1e6));
    }
//cleanup:
    core_unload();
    audio_deinit();
    video_deinit();

    // Destroy agent
    for (int p = 0; p < SAM2_PORT_MAX+1; p++) {
        if (g_agent[p]) {
            juice_destroy(g_agent[p]);
        }
    }

    if (g_vars) {
        for (const struct retro_variable *v = g_vars; v->key; ++v) {
            free((char*)v->key);
            free((char*)v->value);
        }
        free(g_vars);
    }

    SDL_Quit();

    return EXIT_SUCCESS;
}
