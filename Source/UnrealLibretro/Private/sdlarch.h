#pragma once

/*#pragma warning(push)
#pragma warning(disable:4191)
#pragma warning(pop)*/

// Libretro API
#include "libretro/libretro.h"
static_assert(RETRO_API_VERSION == 1, "Retro API version changed");


// Standard Template Library https://docs.unrealengine.com/en-US/Programming/Development/CodingStandard/#useofstandardlibraries
//#include <variant>
#include <array>
#include <unordered_map>
#include <string>

// Unreal imports
#include "CoreMinimal.h"
#include "LambdaRunnable.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "LibretroInputComponent.h"

#include "RawAudioSoundWave.h"
#include "Engine/TextureRenderTarget2D.h"

// Third party libraries
#if PLATFORM_WINDOWS
#include "Windows/PreWindowsApi.h"
#include "ThirdParty/Win64/Include/SDL2/SDL.h"
#endif

#include "ThirdParty/OpenGL/GL/glcorearb.h"

#if PLATFORM_WINDOWS
#include "Windows/PostWindowsApi.h"
#endif

#if PLATFORM_APPLE
#include "ThirdParty/MacOS/Include/SDL2/SDL.h"
#endif

UNREALLIBRETRO_API DECLARE_LOG_CATEGORY_EXTERN(Libretro, Log, All);

struct FLibretroInputState;

extern struct func_wrap_t {
    func_wrap_t* next;
	
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
} func_wrap_table[];

struct libretro_api_t { // @todo the retro prefix to the functions is superfluous, but you must change the macro that binds the function pointers from the dll as well
    void* handle;
    bool initialized;

    void     (*init)(void);
    void     (*deinit)(void);
    unsigned (*api_version)(void);
    void     (*get_system_info)(struct retro_system_info* info);
    void     (*get_system_av_info)(struct retro_system_av_info* info);
    void     (*set_controller_port_device)(unsigned port, unsigned device);
    void     (*reset)(void);
    void     (*run)(void);
    size_t   (*serialize_size)(void);
    bool     (*serialize)(void *data, size_t size);
    bool     (*unserialize)(const void *data, size_t size);
    //    void cheat_reset(void);
    //	  void cheat_set(unsigned index, bool enabled, const char *code);
    bool     (*load_game)(const struct retro_game_info* game);
    //	  bool load_game_special(unsigned game_type, const struct retro_game_info *info, size_t num_info);
    void     (*unload_game)(void);
    //unsigned get_region(void);
    void*    (*get_memory_data)(unsigned id);
    size_t   (*get_memory_size)(unsigned id);
};

struct LibretroContext {
public:                                                                                                                                             // @todo: The UObjects shouldn't be parameters of this function, and the callback below should pass the audio buffer and framebuffer to the caller as well
    static LibretroContext* Launch(FString core, FString game, UTextureRenderTarget2D* RenderTarget, URawAudioSoundWave* SoundEmitter, TSharedPtr<TStaticArray<FLibretroInputState, PortCount>, ESPMode::ThreadSafe> InputState, TUniqueFunction<void(libretro_api_t&, bool)> LoadedCallback);
    static void             Shutdown(LibretroContext* Instance);

           void             Pause(bool ShouldPause);
           void             EnqueueTask(TUniqueFunction<void(libretro_api_t&)> LibretroAPITask);
    
protected:
    LibretroContext(TSharedRef<TStaticArray<FLibretroInputState, PortCount>, ESPMode::ThreadSafe> InputState);
    ~LibretroContext() {}

    TAtomic<bool> running{true};
    TAtomic<bool> enqueue_audio{true}; // @hack prevents hangs when destructing because some cores will call audio_write in an infinite loop until audio is enqueued. We have a fixed size audio buffer and have no real control over how the audio is dequeued so this can happen often.
    bool          exit_run_loop = false;
    FLambdaRunnable* UnrealThreadTask;

    // From all the crazy container types you can tell I had trouble with multithreading. However from what I understand my solution is threadsafe.
    TWeakObjectPtr<UTextureRenderTarget2D> UnrealRenderTarget;
    TWeakObjectPtr<URawAudioSoundWave> UnrealSoundBuffer;
    TSharedRef<TStaticArray<FLibretroInputState, PortCount>, ESPMode::ThreadSafe> UnrealInputState;

    // These are both ThreadSafe shared pointers that are the main bridge between my code and unreal.
    FTexture2DRHIRef TextureRHI; // @todo: be careful with this it can become stale if you reinit the UTextureRenderTarget2D without updating this reference. Say if you were adding a feature to change games without reiniting the entire core. What I really need to do is probably implement my own dynamic texture subclass
    TSharedPtr<TCircularQueue<int32>, ESPMode::ThreadSafe> QueuedAudio;

    struct _8888_color {
        uint32 B : 8;
        uint32 G : 8;
        uint32 R : 8;
        uint32 A : 8;
    };

    bool which = false;
    TSharedPtr<TArray<_8888_color>, ESPMode::ThreadSafe> bgra_buffers[2] = { nullptr, nullptr };
    TAtomic<_8888_color*> RenderThreadsBuffer{nullptr};

    libretro_api_t g_retro;

    TQueue<TUniqueFunction<void(libretro_api_t&)>, EQueueMode::Spsc> LibretroAPITasks; // TQueue<T, EQueueMode::Spsc> has aquire-release semantics on Enqueue and Dequeue so this should be thread-safe



    
    // As a libretro frontend you own directory path data.
     static std::array<char, 260> system_directory;
     static std::array<char, 260> save_directory;

     SDL_Window* g_win = NULL;
     SDL_GLContext g_ctx = NULL;
     SDL_AudioDeviceID g_pcm = 0;
     struct retro_frame_time_callback runloop_frame_time = {0};
     retro_usec_t runloop_frame_time_last = 0;
     const uint8_t* g_kbd = NULL;
     struct retro_audio_callback audio_callback = {0};
     func_wrap_t *callback_instance;
     bool UsingOpenGL = false;
     struct retro_system_av_info av = {{0}, {0}};
     std::unordered_map<std::string, std::string> settings;
     const struct retro_hw_render_context_negotiation_interface* hw_render_context_negotiation = nullptr;

     float g_scale = 3;
    

     struct {
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
    } g_video = { 0 };

     struct {
        GLuint vao;
        GLuint vbo;
        GLuint program;

        GLint i_pos;
        GLint i_coord;
        GLint u_tex;
        GLint u_mvp;

    } g_shader = { 0 };

     



     void init_framebuffer(int width, int height);
     void create_window(int width, int height);
     void resize_to_aspect(double ratio, int sw, int sh, int* dw, int* dh);
     void video_configure(const struct retro_game_geometry* geom);
     bool video_set_pixel_format(unsigned format);
     void video_deinit();
     void video_refresh(const void* data, unsigned width, unsigned height, unsigned pitch);
     uintptr_t core_get_current_framebuffer();
     void core_audio_sample(int16_t left, int16_t right);
     size_t audio_write(const int16_t* buf, size_t frames);
     bool core_environment(unsigned cmd, void* data);
     void core_input_poll(void);
     int16_t core_input_state(unsigned port, unsigned device, unsigned index, unsigned id);
     void core_load(const char* sofile);
     void core_load_game(const char* filename);
};
