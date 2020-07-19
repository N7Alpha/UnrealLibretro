#pragma once




#include "RawAudioSoundWave.h"
#include <Runtime\Engine\Classes\Engine\TextureRenderTarget2D.h>

#include "glad/gl.h" // TODO: This will shadow driver implementations of OpenGL functions with glad's macro defined functions. If the user happens to be using OpenGL for some other purpose when they include this header it will cause weird behavior. A way to fix it would be to find a way to just declare OpenGL Types since that's all I need in this header or use glad's multi-instance version that doesn't Alias OpenGL functions.
#include "glad/khrplatform.h"


#include "Windows/PreWindowsApi.h"
#include "SDL2/SDL.h"
#include "Windows/PostWindowsApi.h"

#include "libretro/libretro.h"

#include <stdio.h>
#include <array>

// C++
//#include <variant>
#include <functional>

#include <unordered_map>
#include <string>

// Unreal imports
#include "CoreMinimal.h"
#include "Misc/Paths.h"
#include "Misc/Optional.h"
#include "GenericPlatform/GenericPlatformProcess.h"
#include "LambdaRunnable.h"
#include "CoreGlobals.h"
#include "Templates/UniquePtr.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Components/AudioComponent.h"
#include "Misc/FileHelper.h"
#include "LibretroInputComponent.h"

UNREALLIBRETRO_API DECLARE_LOG_CATEGORY_EXTERN(Libretro, Log, All);

struct FLibretroInputState;

extern struct func_wrap_t {
    func_wrap_t* next;
    retro_audio_sample_batch_t c_fn;
    retro_video_refresh_t c_video_refresh;
    retro_audio_sample_t c_audio_sample;
    retro_environment_t c_environment;
    retro_input_poll_t c_input_poll;
    retro_input_state_t c_input_state;
    retro_hw_get_current_framebuffer_t c_get_current_framebuffer;
    std::function<size_t(const int16_t*, size_t)>    fn;
    std::function<void(const void*, unsigned, unsigned, size_t)> video_refresh;
    std::function<void(int16_t, int16_t)> audio_sample;
    std::function<bool(unsigned cmd, void* data)> environment;
    std::function<void(void)> input_poll;
    std::function<int16_t(unsigned port, unsigned device, unsigned index, unsigned id)> input_state;
    std::function<uintptr_t(void)> get_current_framebuffer;
} func_wrap_table[];


struct LibretroContext {
public:
    static LibretroContext* launch(FString core, FString game, UTextureRenderTarget2D* RenderTarget, URawAudioSoundWave* SoundEmitter, TSharedPtr<TStaticArray<FLibretroInputState, PortCount>, ESPMode::ThreadSafe> InputState, std::function<void(LibretroContext*)> LoadedCallback);
    FLambdaRunnable* UnrealThreadTask;
protected:
    LibretroContext(TSharedRef<TStaticArray<FLibretroInputState, PortCount>, ESPMode::ThreadSafe> InputState);
    ~LibretroContext() {}
    // UNREAL ENGINE VARIABLES


    // From all the crazy container types you can tell I had trouble with multithreading. I would like to just have GC references to UnrealRenderTarget and UnrealSoundEmitter, but I ran into issues putting them into the rootset or trying to make this class into a subclass of UObject since it doesn't like some of the syntax. However from what I understand my solution is threadsafe.
    TWeakObjectPtr<UTextureRenderTarget2D> UnrealRenderTarget;
    TWeakObjectPtr<URawAudioSoundWave> UnrealSoundBuffer;
    TSharedRef<TStaticArray<FLibretroInputState, PortCount>, ESPMode::ThreadSafe> UnrealInputState;

    // These are both ThreadSafe shared pointers that are the main bridge between my code and unreal.
    FTexture2DRHIRef TextureRHI; // @todo: be careful with this it can become stale if you reinit the UTextureRenderTarget2D without updating this reference. Say if you were adding a feature to change games without reiniting the entire core. What I really need to do is probably implement my own dynamic texture subclass
    TSharedPtr<TCircularQueue<int32>, ESPMode::ThreadSafe> QueuedAudio;

    

    // UNREAL ENGINE VARIABLES END

public:
    TAtomic<bool> running = true;
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
     struct retro_system_av_info av = { 0 };
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

     struct {
        void* handle;
        bool initialized;

        void (*retro_init)(void);
        void (*retro_deinit)(void);
        unsigned (*retro_api_version)(void);
        void (*retro_get_system_info)(struct retro_system_info* info);
        void (*retro_get_system_av_info)(struct retro_system_av_info* info);
        void (*retro_set_controller_port_device)(unsigned port, unsigned device);
        void (*retro_reset)(void);
        void (*retro_run)(void);
        //	size_t retro_serialize_size(void);
        //	bool retro_serialize(void *data, size_t size);
        //	bool retro_unserialize(const void *data, size_t size);
        //	void retro_cheat_reset(void);
        //	void retro_cheat_set(unsigned index, bool enabled, const char *code);
        bool (*retro_load_game)(const struct retro_game_info* game);
        //	bool retro_load_game_special(unsigned game_type, const struct retro_game_info *info, size_t num_info);
        void (*retro_unload_game)(void);
        //	unsigned retro_get_region(void);
        void *(*retro_get_memory_data)(unsigned id);
        size_t (*retro_get_memory_size)(unsigned id);
    } g_retro;

     



     void init_framebuffer(int width, int height);
     void create_window(int width, int height);
     void resize_to_aspect(double ratio, int sw, int sh, int* dw, int* dh);
     void video_configure(const struct retro_game_geometry* geom);
     bool video_set_pixel_format(unsigned format);
     void video_deinit();
     void video_refresh(const void* data, unsigned width, unsigned height, unsigned pitch);
     uintptr_t core_get_current_framebuffer();
     void core_audio_sample(int16_t left, int16_t right);
     size_t audio_write(const int16_t* buf, unsigned frames);
     bool core_environment(unsigned cmd, void* data);
     void core_input_poll(void);
     int16_t core_input_state(unsigned port, unsigned device, unsigned index, unsigned id);
     void core_load(const char* sofile);
     void core_load_game(const char* filename);
     void core_unload();
};