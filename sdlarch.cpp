#include "sam2.c"
#include "glad.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

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
#include <windows.h>
static void sleep(unsigned int secs) { Sleep(secs * 1000); }
#else
#include <unistd.h> // for sleep
#endif

#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_opengl.h>
#include "libretro.h"

// Considering various things like STUN/TURN headers and the UDP/IP headers, additional junk load balancers and routers might add I keep this conservative
#define PACKET_MTU_PAYLOAD_BYTES 1408

#define JUICE_CONCURRENCY_MODE JUICE_CONCURRENCY_MODE_USER


#define NETPLAY 1

static SDL_Window *g_win = NULL;
static SDL_GLContext g_ctx = NULL;
static SDL_AudioDeviceID g_pcm = 0;
static struct retro_frame_time_callback runloop_frame_time;
static retro_usec_t runloop_frame_time_last = 0;
static const uint8_t *g_kbd = NULL;
struct retro_system_av_info g_av = {0};
static juice_agent_t *agent = NULL;
bool g_netplay_ready = false;
static sam2_socket_t g_sam2_socket = 0;
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


struct keymap {
	unsigned k;
	unsigned rk;
};

static struct keymap g_binds[] = {
    { SDL_SCANCODE_X, RETRO_DEVICE_ID_JOYPAD_A },
    { SDL_SCANCODE_Z, RETRO_DEVICE_ID_JOYPAD_B },
    { SDL_SCANCODE_A, RETRO_DEVICE_ID_JOYPAD_Y },
    { SDL_SCANCODE_S, RETRO_DEVICE_ID_JOYPAD_X },
    { SDL_SCANCODE_UP, RETRO_DEVICE_ID_JOYPAD_UP },
    { SDL_SCANCODE_DOWN, RETRO_DEVICE_ID_JOYPAD_DOWN },
    { SDL_SCANCODE_LEFT, RETRO_DEVICE_ID_JOYPAD_LEFT },
    { SDL_SCANCODE_RIGHT, RETRO_DEVICE_ID_JOYPAD_RIGHT },
    { SDL_SCANCODE_RETURN, RETRO_DEVICE_ID_JOYPAD_START },
    { SDL_SCANCODE_BACKSPACE, RETRO_DEVICE_ID_JOYPAD_SELECT },
    { SDL_SCANCODE_Q, RETRO_DEVICE_ID_JOYPAD_L },
    { SDL_SCANCODE_W, RETRO_DEVICE_ID_JOYPAD_R },
    { 0, 0 }
};

static unsigned g_joy[RETRO_DEVICE_ID_JOYPAD_R3 + 1 /* Null terminator */] = { 0 };

static int64_t g_frame_counter_remote{-1};
int64_t frame_counter = 0;

#define load_sym(V, S) do {\
    if (!((*(void**)&V) = SDL_LoadFunction(g_retro.handle, #S))) \
        die("Failed to load symbol '" #S "'': %s", SDL_GetError()); \
	} while (0)
#define load_retro_sym(S) load_sym(g_retro.S, S)


static void die(const char *fmt, ...) {
	char buffer[4096];

	va_list va;
	va_start(va, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, va);
	va_end(va);

	fputs(buffer, stderr);
	fputc('\n', stderr);
	fflush(stderr);

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

#define CHANNEL_INPUT              0b00000000
#define CHANNEL_SAVESTATE_TRANSFER 0b00010000

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
typedef struct {
    uint8_t channel_and_flags;
    int64_t frame;
    unsigned joy[RETRO_DEVICE_ID_JOYPAD_R3 + 1 /* Null terminator */];
} input_packet_t; // @todo get this to have the no padding

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

    uint8_t payload[]; // Variable size; at most PACKET_MTU_PAYLOAD_BYTES-3
} savestate_transfer_packet_t;

typedef struct {
    uint8_t channel_and_flags;
    union {
        uint8_t reed_solomon_k;
        uint8_t packet_groups;
        uint8_t sequence_hi;
    };

    uint8_t sequence_lo;

    uint8_t payload[PACKET_MTU_PAYLOAD_BYTES-3]; // Variable size; at most PACKET_MTU_PAYLOAD_BYTES-3
} savestate_transfer_packet2_t;
static_assert(sizeof(savestate_transfer_packet2_t) == PACKET_MTU_PAYLOAD_BYTES);

typedef struct {
    int64_t total_size_bytes;
    int64_t frame;
    uint64_t encoding_chain; // @todo probably won't use this
    uint64_t xxhash;

    int64_t savestate_size;
    uint8_t zipped_savestate[];
} savestate_transfer_payload_t;


static sam2_room_t g_room = { "My Room Name", "TURN host", 0b01010101, 0 };
static sam2_room_t g_sam2_rooms[MAX_ROOMS];
static int g_sam2_room_count = 0;
static int g_zstd_compress_level = 0;
static sam2_request_u g_sam2_request;
#define MAX_SAMPLE_SIZE 128
static int g_sample_size = MAX_SAMPLE_SIZE/2;
static uint64_t g_save_cycle_count[MAX_SAMPLE_SIZE] = {0};
static uint64_t g_zstd_cycle_count[MAX_SAMPLE_SIZE] = {1};
static uint64_t g_zstd_compress_size[MAX_SAMPLE_SIZE] = {0};
static uint64_t g_reed_solomon_encode_cycle_count[MAX_SAMPLE_SIZE] = {0};
static uint64_t g_reed_solomon_decode_cycle_count[MAX_SAMPLE_SIZE] = {0};
static uint64_t g_save_cycle_count_offset = 0;
static size_t g_serialize_size = 0;
static bool g_do_zstd_compress = false;
static bool g_do_zstd_delta_compress = false;
static bool g_use_rle = false;

int g_zstd_thread_count = 4;

size_t dictionary_size = 0;
unsigned char g_dictionary[256*1024];

static bool g_use_dictionary = false;
static bool g_dictionary_is_dirty = true;
static ZDICT_cover_params_t g_parameters = {0};


#define CHANNEL_INPUT              0b00000000
#define CHANNEL_SAVESTATE_TRANSFER 0b00010000


#define MAX_REDUNDANT_PACKETS 32
static bool g_do_reed_solomon = false;
static int g_redundant_packets = MAX_REDUNDANT_PACKETS - 1;
static int g_lost_packets = 0;

uint64_t g_remote_savestate_hash = 0x6AEBEEF1EDADD1E5;

#define MAX_SAVE_STATES 64
static unsigned char g_savebuffer[MAX_SAVE_STATES][20 * 1024 * 1024] = {0};

#define SAVE_STATE_COMPRESSED_BOUND_BYTES ZSTD_COMPRESSBOUND(sizeof(g_savebuffer[0]))
#define SAVE_STATE_COMPRESSED_BOUND_WITH_REDUNDANCY_BYTES (255 * SAVE_STATE_COMPRESSED_BOUND_BYTES / (255 - MAX_REDUNDANT_PACKETS))
static unsigned char g_savestate_transfer_payload_untyped[sizeof(savestate_transfer_payload_t) + SAVE_STATE_COMPRESSED_BOUND_WITH_REDUNDANCY_BYTES];
static savestate_transfer_payload_t *g_savestate_transfer_payload = (savestate_transfer_payload_t *) g_savestate_transfer_payload_untyped;
static uint8_t *g_savebuffer_compressed = g_savestate_transfer_payload->zipped_savestate;

static int g_save_state_index = 0;
static int g_save_state_used_for_delta_index_offset = 1;

#define MAX_FEC_PACKET_GROUPS 16
#define FEC_REDUNDANT_BLOCKS 16
static void* g_fec_packet[MAX_FEC_PACKET_GROUPS][255 - FEC_REDUNDANT_BLOCKS];
static int g_fec_index[MAX_FEC_PACKET_GROUPS][255 - FEC_REDUNDANT_BLOCKS];
static int g_fec_index_counter[MAX_FEC_PACKET_GROUPS] = {0};
static unsigned char g_remote_savestate_transfer_packets[SAVE_STATE_COMPRESSED_BOUND_WITH_REDUNDANCY_BYTES + MAX_FEC_PACKET_GROUPS * (255 - FEC_REDUNDANT_BLOCKS) * sizeof(savestate_transfer_packet_t)];
static int64_t g_remote_savestate_transfer_offset = 0;
uint8_t g_remote_packet_groups = MAX_FEC_PACKET_GROUPS;
static bool g_send_savestate_next_frame = false;

static bool g_is_refreshing_rooms = false;


static ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
static bool g_connected_to_sam2 = false;
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
        static float f = 0.0f;
        static int counter = 0;
        char unit[FORMAT_UNIT_COUNT_SIZE] = {0};

        ImGui::Begin("Compression investigation");                          // Create a window called "Hello, world!" and append into it.

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
            static const char* items[] = {"cycle_count", "cycle_count", "compress_size"};
            static int current_item = 0;  // default selection
            ImGui::Combo("Buffers", &current_item, items, IM_ARRAYSIZE(items));

            // Create a temporary array to hold float values for plotting.
            float temp[MAX_SAMPLE_SIZE];

            // Based on the selection, copy data to the temp array and draw the graph for the corresponding buffer.
            if (current_item == 0) {
                for (int i = 0; i < g_sample_size; ++i) {
                    temp[i] = static_cast<float>(g_save_cycle_count[(i+g_save_cycle_count_offset)%g_sample_size]);
                }
                ImGui::PlotLines("save_cycle_count", temp, g_sample_size);
            } else if (current_item == 1) {
                for (int i = 0; i < g_sample_size; ++i) {
                    temp[i] = static_cast<float>(g_zstd_cycle_count[(i+g_save_cycle_count_offset)%g_sample_size]);
                }
                ImGui::PlotLines("cycle_count", temp, g_sample_size);
            } else if (current_item == 2) {
                for (int i = 0; i < g_sample_size; ++i) {
                    temp[i] = static_cast<float>(g_zstd_compress_size[(i+g_save_cycle_count_offset)%g_sample_size]);
                }
                ImGui::PlotLines("compress_size", temp, g_sample_size);
            }

            // Add a toggle for switching between histogram and line plot
            static bool show_histogram = false;
            ImGui::Checkbox("Show as Histogram", &show_histogram);

            if (show_histogram) {
                if (current_item == 0) {
                    ImGui::PlotHistogram("save_cycle_count", temp, g_sample_size);
                } else if (current_item == 1) {
                    ImGui::PlotHistogram("cycle_count", temp, g_sample_size);
                } else if (current_item == 2) {
                    ImGui::PlotHistogram("compress_size", temp, g_sample_size);
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

        ImGui::SliderFloat("float", &f, 0.0f, 1.0f);           // Edit 1 float using a slider from 0.0f to 1.0f
        ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color
        
        ImGui::End();
        {
            ImGui::Begin("Sam2 Interface", NULL, ImGuiWindowFlags_AlwaysAutoResize);

            if (g_connected_to_sam2) {
                ImGui::TextColored(ImVec4(0, 1, 0, 1), "Connected to the SAM2");
            } else {
                ImGui::TextColored(ImVec4(0.5, 0.5, 0.5, 1), "Connecting to the SAM2 %c", spinnerGlyph);
            }

            // Editable text fields for room name and TURN host
            ImGui::InputText("##name", g_room.name, sizeof(g_room.name));
            ImGui::SameLine();
            ImGui::InputText("##turn_hostname", g_room.turn_hostname, sizeof(g_room.turn_hostname));

            // Fixed text fields to display binary values
            char ports_str[65];
            char flags_str[65];

            // Convert the integer values to binary strings
            for (int i = 0; i < 64; i+=4) {
                ports_str[i/4] = '0' + ((g_room.ports >> (60 - i)) & 0xF);
                flags_str[i/4] = '0' + ((g_room.flags >> (60 - i)) & 0xF);
            }

            ports_str[16] = '\0';
            flags_str[16] = '\0';

            ImGui::Text("Port bitfield: %s", ports_str);
            ImGui::SameLine();
            ImGui::Text("Flags bitfield: %s", flags_str);

            // Create a "Make" button that sends a make room request when clicked
            if (ImGui::Button("Make")) {
                // Send a make room request
                sig_room_make_request_t *request = &g_sam2_request.room_make_request;
                request->room = g_room;
                // Fill in the rest of the request fields appropriately...
                sam2_client_send(g_sam2_socket, &g_sam2_request, SAM2_EMESSAGE_MAKE);
            }
        }

        {
            if (ImGui::Button(g_is_refreshing_rooms ? "Stop" : "Refresh")) {
                // Toggle the state
                g_is_refreshing_rooms = !g_is_refreshing_rooms;

                if (g_is_refreshing_rooms) {
                    // The list message is only a header
                    g_sam2_room_count = 0;
                    sam2_client_send(g_sam2_socket, &g_sam2_request, SAM2_EMESSAGE_LIST);
                } else {

                }
            }

            // If we're in the "Stop" state
            if (g_is_refreshing_rooms) {
                // Run your "Stop" code here
            }

            int selected_room_index = -1;  // Initialize as -1 to indicate no selection
            // Table
            if (ImGui::BeginTable("Rooms table", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
                ImGui::TableSetupColumn("Room Name");
                ImGui::TableSetupColumn("TURN Host Name");
                ImGui::TableHeadersRow();

                for (int room_index = 0; room_index < g_sam2_room_count; ++room_index) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    // Make the row selectable and keep track of the selected room
                    ImGuiSelectableFlags selectable_flags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick;
                    if (ImGui::Selectable(g_sam2_rooms[room_index].name, selected_room_index == room_index, selectable_flags))
                    {
                        selected_room_index = room_index;
                    }
                    
                    ImGui::TableNextColumn();
                    ImGui::Text("%s", g_sam2_rooms[room_index].turn_hostname);
                }

                ImGui::EndTable();
            }

            if (selected_room_index != -1) {
                printf("Selected room at index %d\n", selected_room_index);
                selected_room_index = -1;
            }
        }

        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
        ImGui::End();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
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
    SDL_QueueAudio(g_pcm, buf, sizeof(*buf) * frames * 2);
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
    core_log(RETRO_LOG_INFO, "[timer] %s: %i - %i", g_retro.perf_counter_last->ident, g_retro.perf_counter_last->start, g_retro.perf_counter_last->total);
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
    draw_imgui();
    SDL_GL_SwapWindow(g_win);
}


static void core_input_poll(void) {
	int i;
    g_kbd = SDL_GetKeyboardState(NULL);

	for (i = 0; g_binds[i].k || g_binds[i].rk; ++i)
        g_joy[g_binds[i].rk] = g_kbd[g_binds[i].k];

    //printf("Sending Input for frame %d\n", frame_counter);
#if NETPLAY
    input_packet_t input_packet;

    input_packet.channel_and_flags = CHANNEL_INPUT;
    input_packet.frame = frame_counter++;
    memcpy(input_packet.joy, g_joy, sizeof(g_joy));

    juice_send(agent, (char *) &input_packet, sizeof(input_packet));
    if (g_kbd[SDL_SCANCODE_ESCAPE])
        running = false;
#endif
}

static unsigned g_joy_remote[RETRO_DEVICE_ID_JOYPAD_R3+1] = { 0 };

static int16_t core_input_state(unsigned port, unsigned device, unsigned index, unsigned id) {
	if (port || index || device != RETRO_DEVICE_JOYPAD)
		return 0;

#if NETPLAY
    while (g_frame_counter_remote != frame_counter-1) {
#if JUICE_CONCURRENCY_MODE == JUICE_CONCURRENCY_MODE_USER
    juice_user_poll(&agent, 1);
#endif
        _mm_pause();
        _mm_pause();
    }
#endif
    //printf("frame_counter: %d\n", frame_counter);
    //fflush(stdout);
	return g_joy[id] > g_joy_remote[id] ? g_joy[id] : g_joy_remote[id];
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
    snprintf(window_title, sizeof(window_title), "sdlarch %s %s", system.library_name, system.library_version);
    SDL_SetWindowTitle(g_win, window_title);
}

static void core_unload() {
	if (g_retro.initialized)
		g_retro.retro_deinit();

	if (g_retro.handle)
        SDL_UnloadObject(g_retro.handle);
}

static void noop() {}

#define BUFFER_SIZE 4096

static bool gathering_done = false;
char g_sdp[JUICE_MAX_SDP_STRING_LEN];

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

int test_connectivity() {
  juice_set_log_handler(receive_juice_log);

  juice_set_log_level(JUICE_LOG_LEVEL_WARN);
  // Create agent
  juice_config_t config;
  memset(&config, 0, sizeof(config));

  // STUN server example*
  config.concurrency_mode = JUICE_CONCURRENCY_MODE;
  config.stun_server_host = "stun.l.google.com";
  config.stun_server_port = 19302;
  //config.bind_address = "127.0.0.1";

  config.cb_state_changed = on_state_changed;
  config.cb_candidate = on_candidate;
  config.cb_gathering_done = on_gathering_done;
  config.cb_recv = on_recv;
  config.user_ptr = NULL;

  agent = juice_create(&config);

  // Generate local description
  
  juice_get_local_description(agent, g_sdp, JUICE_MAX_SDP_STRING_LEN);
  printf("Local description:\n%s\n", g_sdp);

  // Gather candidates
  juice_gather_candidates(agent);
  char sdp_remote[JUICE_MAX_SDP_STRING_LEN] = {0};

  bool remote_candidates_aquired = false;
  while (!remote_candidates_aquired) {
    for (SDL_Event ev; SDL_PollEvent(&ev);) {
        ImGui_ImplSDL2_ProcessEvent(&ev);

        if (ev.type == SDL_WINDOWEVENT) {
            switch (ev.window.event) {
            case SDL_WINDOWEVENT_CLOSE: 
                return -1;
            case SDL_WINDOWEVENT_RESIZED:
                resize_cb(ev.window.data1, ev.window.data2);
                break;
            }
        }
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(g_win);
    ImGui::NewFrame();

    ImGuiIO& io = ImGui::GetIO();
    glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
    glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);

    // Show ImGui window
    ImGui::Begin("SDP Connectivity Test");
    ImGui::Text("Local SDP:");
    ImGui::InputTextMultiline("Local SDP", g_sdp, sizeof(g_sdp), ImVec2(0, 0), ImGuiInputTextFlags_ReadOnly);
    ImGui::InputTextMultiline("Remote SDP", sdp_remote, sizeof(sdp_remote), ImVec2(0, 0), ImGuiInputTextFlags_None);

    // ImGui::InputTextMultiline("Local Candidates", candidates, sizeof(candidates), ImVec2(0, 0), ImGuiInputTextFlags_ReadOnly);
    // ImGui::InputTextMultiline("Remote Candidates", remote_candidates, sizeof(remote_candidates), ImVec2(0, 0), ImGuiInputTextFlags_None);
    
    if (ImGui::Button("Submit")) {
        auto status = juice_set_remote_description(agent, sdp_remote);
        assert(JUICE_ERR_SUCCESS == status);
        remote_candidates_aquired = true;
    }
    ImGui::End();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    SDL_GL_SwapWindow(g_win);
  }

#if 0
  for (char *p = remote_candidates; p[0] != '\0'; p++) {
     char *base = p;

     while (p[0] != '\0' && p[0] != '\n') p++;
     if (p[0] == '\n') p[0] = '\0';

     int status = juice_add_remote_candidate(agent, base);
     assert(status == JUICE_ERR_SUCCESS);
  }
#endif

  juice_set_remote_gathering_done(agent);
  #if JUICE_CONCURRENCY_MODE == JUICE_CONCURRENCY_MODE_USER
  // Poll for two seconds sleeping 20ms between polls
  for(int i = 0; i < 100; ++i) {
    juice_user_poll(&agent, 1);
    #ifdef _WIN32
      Sleep(20);
    #else
      usleep(20000);
    #endif
  }
  #else
  sleep(2);
  #endif
  // -- Connection should be finished --

  juice_state_t state = juice_get_state(agent);
  bool success = (state == JUICE_STATE_COMPLETED);
  
  if (!success) {
    printf("Connection failed: %d\n", state);
    return -1;
  }

  // Retrieve candidates
  char local[JUICE_MAX_CANDIDATE_SDP_STRING_LEN];
  char remote[JUICE_MAX_CANDIDATE_SDP_STRING_LEN];
  if (success &= (juice_get_selected_candidates(agent, local, JUICE_MAX_CANDIDATE_SDP_STRING_LEN, remote,
                                                JUICE_MAX_CANDIDATE_SDP_STRING_LEN) == 0)) {
    printf("Local candidate: %s\n", local);
    printf("Remote candidate: %s\n", remote);
  }
  assert(success);

  // Retrieve addresses
  char localAddr[JUICE_MAX_ADDRESS_STRING_LEN];
  char remoteAddr[JUICE_MAX_ADDRESS_STRING_LEN];
  if (success &= (juice_get_selected_addresses(agent, localAddr, JUICE_MAX_ADDRESS_STRING_LEN, remoteAddr, JUICE_MAX_ADDRESS_STRING_LEN) == 0)) {
    printf("Local address: %s\n", localAddr);
    printf("Remote address: %s\n", remoteAddr);
  }
  assert(success);

  if (success) {
    printf("Success\n");
    return 0;
  } else {
    printf("Failure\n");
    return -1;
  }
}

// On state changed
static void on_state_changed(juice_agent_t *agent, juice_state_t state, void *user_ptr) {
  printf("State: %s\n", juice_state_to_string(state));

  if (state == JUICE_STATE_CONNECTED) {
    // On connected, send a message
    //const char *message = "Hello";
    //juice_send(agent, message, strlen(message));
  }
}

// On local candidate gathered
static void on_candidate(juice_agent_t *agent, const char *sdp, void *user_ptr) {
  printf("Candidate: %s\n", sdp);

  strcat(g_sdp, sdp);
  strcat(g_sdp, "\n");
}

// On local candidates gathering done
static void on_gathering_done(juice_agent_t *agent, void *user_ptr) {
  printf("Gathering done\n");
  gathering_done = true;
}

// On message received
static void on_recv(juice_agent_t *agent, const char *data, size_t size, void *user_ptr) {
    uint8_t channel_and_flags = data[0];
    switch (channel_and_flags & 0xF0) {
    case CHANNEL_INPUT: {
        SDL_assert(size == sizeof(input_packet_t));
        memcpy(&g_frame_counter_remote, data + offsetof(input_packet_t, frame), sizeof(input_packet_t::frame));
        memcpy(g_joy_remote, data + offsetof(input_packet_t, joy), sizeof(input_packet_t::joy));
        break;
    }
    case CHANNEL_SAVESTATE_TRANSFER: {
        SDL_assert(size >= sizeof(savestate_transfer_packet_t));
        SDL_assert(size <= PACKET_MTU_PAYLOAD_BYTES);
        uint8_t sequence_hi = 0;
        int k = 239;
        if (channel_and_flags & SAVESTATE_TRANSFER_FLAG_K_IS_239) {
            if (channel_and_flags & SAVESTATE_TRANSFER_FLAG_SEQUENCE_HI_IS_0) {
                g_remote_packet_groups = data[1];
            } else {
                sequence_hi = data[1];
            }
        } else {
            k = data[1];
            g_remote_packet_groups = 1; // k != 239 => 1 packet group
        }

        if (g_fec_index_counter[sequence_hi] == k) {
            // We already have received enough Reed-Solomon blocks to decode the payload; we can ignore this packet
            break;
        }

        uint8_t sequence_lo = data[2];

        uint8_t *copied_packet_ptr = (uint8_t *) memcpy(g_remote_savestate_transfer_packets + g_remote_savestate_transfer_offset, data, size);
        g_fec_packet[sequence_hi][sequence_lo] = copied_packet_ptr + sizeof(savestate_transfer_packet_t);
        g_remote_savestate_transfer_offset += size;

        g_fec_index[sequence_hi][g_fec_index_counter[sequence_hi]++] = sequence_lo;

        if (g_fec_index_counter[sequence_hi] == k) {
            int redudant_blocks_sent = k * FEC_REDUNDANT_BLOCKS / (255 - FEC_REDUNDANT_BLOCKS);
            void *rs_code = fec_new(k, k + redudant_blocks_sent);
            int rs_block_size = (int) (size - sizeof(savestate_transfer_packet_t));
            int status = fec_decode(rs_code, g_fec_packet[sequence_hi], g_fec_index[sequence_hi], rs_block_size);
            SDL_assert(status == 0);
            fec_free(rs_code);

            bool all_data_decoded = true;
            for (int i = 0; i < g_remote_packet_groups; i++) {
                all_data_decoded &= g_fec_index_counter[i] == k;
            }

            if (all_data_decoded) { 
                printf("All the savestate data has been decoded\n");

                uint8_t *savestate_transfer_payload_untyped = (uint8_t *) g_savestate_transfer_payload;
                int64_t remote_payload_size = 0;
                
                for (int j = 0; j < g_remote_packet_groups; j++) {
                    for (int i = 0; i < k; i++) {
                        memcpy(savestate_transfer_payload_untyped + remote_payload_size, g_fec_packet[j][i], rs_block_size);
                        remote_payload_size += rs_block_size;
                    }
                }

                g_remote_savestate_hash = fnv1a_hash(g_savestate_transfer_payload, g_savestate_transfer_payload->total_size_bytes);
                printf("Received savestate payload with hash: %llx size: %llu bytes\n", g_remote_savestate_hash, g_savestate_transfer_payload->total_size_bytes);

                // Reset the savestate transfer state
                g_remote_packet_groups = MAX_FEC_PACKET_GROUPS;
                g_remote_savestate_transfer_offset = 0;
                memset(g_fec_index_counter, 0, sizeof(g_fec_index_counter));
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

void logical_partition(int sz, int redundant, int *n, int *out_k, int *packet_size, int *packet_groups) {
    int k_max = 255 - redundant;
    *packet_groups = 1;
    int k;
    for (;;) {
        k = (sz - 1) / (*packet_groups * *packet_size) + 1;

        if (k > k_max) {
            *packet_groups = (k - 1) / k_max + 1;
            *packet_size = (sz - 1) / (k_max * *packet_groups) + 1;
        } else {
            break;
        }
    }

    *n = k + k * redundant / k_max;
    *out_k = k;
}



int main(int argc, char *argv[]) {
	if (argc < 2)
		die("usage: %s <core> [game]", argv[0]);

    SDL_SetMainReady();

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

        SDL_RWclose(file);
    }

    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_EVENTS) < 0)
        die("Failed to initialize SDL");

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

#if NETPLAY
    if (test_connectivity() == -1) goto cleanup;
#endif
    while (running) {
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
        g_netplay_ready = false;
        g_retro.retro_run();

        g_serialize_size = g_retro.retro_serialize_size();
        if (sizeof(g_savebuffer[0]) >= g_serialize_size) {
            uint64_t start = rdtsc();
            g_retro.retro_serialize(g_savebuffer[g_save_state_index], sizeof(g_savebuffer[0]));
            g_save_cycle_count[g_save_cycle_count_offset] = rdtsc() - start;
        } else {
            fprintf(stderr, "Save buffer too small to save state\n");
        }

        if (g_do_zstd_compress) {
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
                    rle_encode32(buffer, g_serialize_size / 4, g_savebuffer_compressed, g_zstd_compress_size + g_save_cycle_count_offset);
                    g_zstd_compress_size[g_save_cycle_count_offset] *= 4;
                } else {
                    rle_encode8(buffer, g_serialize_size, g_savebuffer_compressed, g_zstd_compress_size + g_save_cycle_count_offset);
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
                        g_zstd_compress_size[g_save_cycle_count_offset] = ZSTD_compress_usingCDict(cctx, 
                                                                                                   g_savebuffer_compressed, SAVE_STATE_COMPRESSED_BOUND_BYTES,
                                                                                                   buffer, g_serialize_size, 
                                                                                                   cdict);
                    }
                } else {
                    g_zstd_compress_size[g_save_cycle_count_offset] = ZSTD_compress(g_savebuffer_compressed,
                                                                                    SAVE_STATE_COMPRESSED_BOUND_BYTES,
                                                                                    buffer, g_serialize_size, g_zstd_compress_level);
                }

            }

            if (ZSTD_isError(g_zstd_compress_size[g_save_cycle_count_offset])) {
                fprintf(stderr, "Error compressing: %s\n", ZSTD_getErrorName(g_zstd_compress_size[g_save_cycle_count_offset]));
                g_zstd_compress_size[g_save_cycle_count_offset] = 0;
            }

            g_zstd_cycle_count[g_save_cycle_count_offset] = rdtsc() - start;

            // Reed Solomon
            int packet_payload_size = PACKET_MTU_PAYLOAD_BYTES - sizeof(savestate_transfer_packet_t);

            int n, k, packet_groups;
            logical_partition(sizeof(savestate_transfer_payload_t) + g_zstd_compress_size[g_save_cycle_count_offset], 16, &n, &k, &packet_payload_size, &packet_groups);
            //int k = g_zstd_compress_size[g_save_cycle_count_offset]/PACKET_MTU_PAYLOAD_BYTES+1;
            //int n = k + g_redundant_packets;
            char *data[(SAVE_STATE_COMPRESSED_BOUND_BYTES/PACKET_MTU_PAYLOAD_BYTES+1+MAX_REDUNDANT_PACKETS)];
            if (g_do_reed_solomon) {
                #if 0
                uint64_t remaining = g_zstd_compress_size[g_save_cycle_count_offset];
                int i = 0;
                for (; i < k; i++) {
                    uint64_t consume = SAM2_MIN(PACKET_MTU_PAYLOAD_BYTES, remaining);
                    memcpy(g_savebuffer_compressed_packetized[i], &g_savebuffer_compressed[i * PACKET_MTU_PAYLOAD_BYTES], consume);
                    remaining -= consume;
                    data[i] = (char *) g_savebuffer_compressed_packetized[i];
                }

                memset(&g_savebuffer_compressed_packetized[i + PACKET_MTU_PAYLOAD_BYTES - remaining], 0, remaining);

                for (; i < n; i++) {
                    data[i] = (char *) g_savebuffer_compressed_packetized[i];
                }

                uint64_t start = rdtsc();
                rs_encode2(k, n, data, PACKET_MTU_PAYLOAD_BYTES);
                g_reed_solomon_encode_cycle_count[g_save_cycle_count_offset] = rdtsc() - start;

                for (i = 0; i < g_lost_packets; i++) {
                    data[i] = NULL;
                }

                start = rdtsc();
                rs_decode2(k, n, data, PACKET_MTU_PAYLOAD_BYTES);
                g_reed_solomon_decode_cycle_count[g_save_cycle_count_offset] = rdtsc() - start;
                #else

                #endif
            }

            if (g_send_savestate_next_frame) {
                g_send_savestate_next_frame = false;

                g_savestate_transfer_payload->frame = frame_counter;
                g_savestate_transfer_payload->savestate_size = g_zstd_compress_size[g_save_cycle_count_offset];
                g_savestate_transfer_payload->total_size_bytes = sizeof(savestate_transfer_payload_t) + g_zstd_compress_size[g_save_cycle_count_offset];

                uint64_t hash = fnv1a_hash(g_savestate_transfer_payload, g_savestate_transfer_payload->total_size_bytes);
                printf("Sending savestate payload with hash: %llx size: %llu bytes\n", hash, g_savestate_transfer_payload->total_size_bytes);

                for (int j = 0; j < packet_groups; j++) {
                    void *data[255];
                    void *rs_code = fec_new(k, n);
                    
                    for (int i = 0; i < n; i++) {
                        data[i] = g_savestate_transfer_payload_untyped + i * packet_payload_size + j * packet_payload_size * packet_groups;
                    }

                    for (int i = k; i < n; i++) {
                        fec_encode(rs_code, (void **)data, data[i], i, packet_payload_size);
                    }

                    fec_free(rs_code);

                    if (agent) {
                        for (int i = 0; i < n; i++) {
                            // @todo I need to send the rest of the payload
                            // interchange the for loop as well
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

                            memcpy(packet.payload, data[i], packet_payload_size);

                            int status = juice_send(agent, (char *) &packet, sizeof(savestate_transfer_packet_t) + packet_payload_size);
                            SDL_assert(status == 0);
                        }
                    }
                }
            }
        }


        g_save_cycle_count_offset = (g_save_cycle_count_offset + 1) % g_sample_size;
        g_save_state_index = (g_save_state_index + 1) % MAX_SAVE_STATES;

        if (g_sam2_socket == 0) {
            if (sam2_client_connect(&g_sam2_socket, "35.84.2.235") == 0) {
                printf("Socket created successfully SAM2\n");
            }
        }

        if (g_connected_to_sam2 || (g_connected_to_sam2 = sam2_client_poll_connection(g_sam2_socket, 0))) {
            static sam2_response_u response;
            static sam2_message_e response_tag = SAM2_EMESSAGE_NONE;
            static int response_length = 0;
            for (;;) {
                // I think we get stuck in an infinite loop here
                int status = sam2_client_poll(g_sam2_socket, &response, &response_tag, &response_length);

                if (status < 0) {
                    fprintf(stderr, "TCP Stream state corrupted exiting...\n");
                    fflush(stderr);
                    exit(1);
                }

                if (   response_tag == SAM2_EMESSAGE_PART
                    || response_tag == SAM2_EMESSAGE_NONE) {
                    break;
                }

                switch (response_tag) {
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


	}
cleanup:
	core_unload();
	audio_deinit();
	video_deinit();

    // Destroy agent
    juice_destroy(agent);

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
