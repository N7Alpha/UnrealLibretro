#pragma once

/*#pragma warning(push)
#pragma warning(disable:4191)
#pragma warning(pop)*/

// Libretro API
#include "libretro/libretro.h"
static_assert(RETRO_API_VERSION == 1, "Retro API version changed");


// Standard Template Library https://docs.unrealengine.com/en-US/Programming/Development/CodingStandard/#useofstandardlibraries
#include <array>
#include <unordered_map>
#include <string>

// Unreal imports
#include "CoreMinimal.h"
#include "Engine/TextureRenderTarget2D.h"
#include "UObject/WeakObjectPtrTemplates.h"

#include "LibretroInputDefinitions.h"
#include "RawAudioSoundWave.h"
#include "UnrealLibretro.h" // For Libretro debug log category
#include "LambdaRunnable.h"


// Third party libraries
#if PLATFORM_WINDOWS
#include "Windows/PreWindowsApi.h"
#include "Windows/SDL2/SDL.h"
#include "Windows/PostWindowsApi.h"
#elif PLATFORM_APPLE
#include "Mac/SDL2/SDL.h"
#endif

#if PLATFORM_WINDOWS
#include "Windows/PreWindowsApi.h"
#endif

#include "ThirdParty/OpenGL/GL/glcorearb.h"

#if PLATFORM_WINDOWS
#include "Windows/PostWindowsApi.h"
#endif

struct libretro_callbacks_t;

struct libretro_api_t {
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

    bool shutdown = false;
    TAtomic<bool> shutdown_audio{false}; // @hack prevents hangs when destructing because some cores will call core_audio_write in an infinite loop until audio is enqueued. We have a fixed size audio buffer and have no real control over how the audio is dequeued so this can happen often.
    bool          exit_run_loop = false;
    FLambdaRunnable* UnrealThreadTask = nullptr;

    // From all the crazy container types you can tell I had trouble with multithreading. However from what I understand my solution is threadsafe.
    TWeakObjectPtr<UTextureRenderTarget2D> UnrealRenderTarget{nullptr};
    TWeakObjectPtr<URawAudioSoundWave> UnrealSoundBuffer{nullptr};
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
    _8888_color *bgra_buffers[2] = { nullptr, nullptr };
    TAtomic<_8888_color*> RenderThreadsBuffer{nullptr};

    libretro_api_t        libretro_api = {0};
    libretro_callbacks_t* callback_instance = nullptr;
    TQueue<TUniqueFunction<void(libretro_api_t&)>, EQueueMode::Spsc> LibretroAPITasks; // TQueue<T, EQueueMode::Spsc> has acquire-release semantics on Enqueue and Dequeue so this should be thread-safe


    SDL_Window* g_win = nullptr;
    SDL_GLContext g_ctx = nullptr;
    struct retro_frame_time_callback runloop_frame_time = {0};
    retro_usec_t runloop_frame_time_last = 0;
    struct retro_audio_callback audio_callback = {0};
    
    bool UsingOpenGL = false;
    struct retro_system_av_info av = {{0}, {0}};
    std::unordered_map<std::string, std::string> settings;
    const struct retro_hw_render_context_negotiation_interface* hw_render_context_negotiation = nullptr;
    

    struct {
        GLuint tex_id;
        GLuint fbo_id;
        GLuint rbo_id;

        int glmajor;
        int glminor;

        GLuint pitch;

        GLuint pixfmt;
        GLuint pixtype;
        GLuint bpp;

        struct retro_hw_render_callback hw;
    } g_video = { 0 };

	
    void create_window();
    void video_configure(const struct retro_game_geometry* geom);
    
    void load(const char* sofile);
    void load_game(const char* filename);

	// These are the callbacks the Libretro Core calls directly via C-function wrappers. This is where the callback implementation logic is.
    void    core_video_refresh(const void* data, unsigned width, unsigned height, unsigned pitch);
    void    core_audio_sample(int16_t left, int16_t right);
    size_t  core_audio_write(const int16_t* buf, size_t frames);
    int16_t core_input_state(unsigned port, unsigned device, unsigned index, unsigned id);
    // void   core_input_poll(void);
    bool    core_environment(unsigned cmd, void* data);
};