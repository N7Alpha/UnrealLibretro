#pragma once

// Libretro API
#include "libretro/libretro.h"
static_assert(RETRO_API_VERSION == 1, "Retro API version changed");


// Standard Template Library https://docs.unrealengine.com/en-US/Programming/Development/CodingStandard/#useofstandardlibraries
#include <atomic>

// Unreal imports
#include "CoreMinimal.h"
#include "Engine/TextureRenderTarget2D.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Containers/CircularQueue.h"
#include "Containers/Queue.h"
#include "RHIResources.h"

#include "LibretroInputDefinitions.h"
#include "RawAudioSoundWave.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#endif

#include "ThirdParty/OpenGL/GL/glcorearb.h"
#include "GL/extension_definitions.h"

#if PLATFORM_WINDOWS
#include "Windows/HideWindowsPlatformTypes.h"
#endif

DECLARE_STATS_GROUP(TEXT("UnrealLibretro"), STATGROUP_UnrealLibretro, STATCAT_Advanced);

#define ENUM_GL_WIN32_INTEROP_PROCEDURES(EnumMacro) \
        EnumMacro(PFNGLIMPORTMEMORYWIN32HANDLEEXTPROC, glImportMemoryWin32HandleEXT) \
        EnumMacro(PFNGLCREATEMEMORYOBJECTSEXTPROC, glCreateMemoryObjectsEXT) \
        EnumMacro(PFNGLDELETEMEMORYOBJECTSEXTPROC, glDeleteMemoryObjectsEXT) \
        EnumMacro(PFNGLTEXTURESTORAGEMEM2DEXTPROC, glTextureStorageMem2DEXT) \
        EnumMacro(PFNGLCREATETEXTURESPROC, glCreateTextures) \

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

struct libretro_api_t {
    void* handle;
    bool initialized;
    bool supports_no_game;

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

    retro_keyboard_event_t keyboard_event;
};

struct FLibretroContext {
public:
    /**
     * @brief analogous to new except asynchronous
     * @post The LoadedCallback is always called
     */
    static FLibretroContext* Launch(class ULibretroCoreInstance* LibretroCoreInstance, FString core, FString game, UTextureRenderTarget2D* RenderTarget, URawAudioSoundWave* SoundEmitter, TUniqueFunction<void(FLibretroContext*, libretro_api_t&)> LoadedCallback);
    
    /**
     * @brief analogous to delete except asynchronous
     */
    static void Shutdown(FLibretroContext* Instance);

    /**
     * Queued tasks will still execute even if paused
     */
    void Pause(bool ShouldPause);
    
    /**
     * @post Everything queued before calling shutdown will be executed
     */
    void EnqueueTask(TUniqueFunction<void(libretro_api_t&)> LibretroAPITask);

    /**
     * This is what the libretro core reads from when determining input. If you want to use your own input method you can modify this directly.
     */
    FLibretroInputState InputState[PortCount];
    FLibretroInputState NextInputState[PortCount];

    std::atomic<bool> OptionsHaveBeenModified;
    TArray<std::atomic<uint8>> OptionSelectedIndex;
    TMap<FString, FString> StartingOptions;

    // The following are always safe to access from the game thread and are guranteed to be initialized after exiting the starting state
    TArray<FLibretroOptionDescription> OptionDescriptions;
    TStaticArray<TArray<struct FLibretroControllerDescription>, PortCount> ControllerDescriptions;
    decltype(retro_controller_description::id) DeviceIDs[PortCount]; // The core doesn't keep track of this so we have to

    struct retro_system_info system = { 0 };

    TUniqueFunction<TRemovePointer<retro_environment_t>::Type> CoreEnvironmentCallback;

    /**
     * @brief Describes the state of the core we're executing
     * 
     * @note Certain fields like ControllerDescriptions are only safe to access from other threads once we're in the running state @todo Maybe add encapsulation to make this explicit
     * The following State Machine indicates valid state transitions:
     *   +----------+          +-----------+
     *   | Starting |--------->|  Running  |------------+
     *   +----------+          +-----------+            |
     *        |                   ^     |               |
     *        v                   |     v               v
     *   +-------------+       +-----------+       +-----------+
     *   | StartFailed |       |  Paused   |------>|  Shutdown |
     *   +-------------+       +-----------+       +-----------+
     */
    enum class ECoreState : int8
    {
        Starting,
        StartFailed,
        Running,
        Paused,
        Shutdown
    };
    
    std::atomic<ECoreState> CoreState{ ECoreState::Starting };

    EPixelFormat UnrealPixelFormat{PF_B8G8R8A8};

// @todo I need to fix the sam2.h include so that I can just use the type in there
#if PLATFORM_WINDOWS
    unsigned __int64 sam_socket = 0;
#else
    int sam_socket = 0;
#endif
    int netimgui_port = 0;
    bool connected_to_sam2 = false;
    struct ulnet_session* netplay_session = nullptr;
    unsigned char* netplay_save_state_data = nullptr;
    size_t netplay_save_state_size = 0;

protected:
    FLibretroContext() {}
    ~FLibretroContext() {}

    libretro_api_t        libretro_api = { 0 };
    struct libretro_callbacks_t* libretro_callbacks = nullptr;
    TQueue<TUniqueFunction<void(libretro_api_t&)>, EQueueMode::Spsc> LibretroAPITasks; // TQueue<T, EQueueMode::Spsc> has acquire-release semantics on Enqueue and Dequeue so this should be thread-safe

    // @todo remove these and have the loaded callback handle these resources
    TWeakObjectPtr<UTextureRenderTarget2D> UnrealRenderTarget{nullptr};
    TWeakObjectPtr<URawAudioSoundWave> UnrealSoundBuffer{nullptr};

    TMap<FString, TArray<char>> OptionsCache; // This isn't technically needed in theory a core should immediately convert an option into it's internal representation and cease referencing 
                                              // the string before control flow is passed back to you, but the lifetime of a string passed to a core when it uses RETRO_ENVIRONMENT_GET_VARIABLE 
                                              // isn't well defined in the libretro spec, so some cores like mame will misbehave if you free a string before it's done with it

    struct
    {
        // These are all ThreadSafe shared pointers that are the main bridge between and unreal
        FTexture2DRHIRef TextureRHI;
        TSharedPtr<TCircularQueue<int32>, ESPMode::ThreadSafe> AudioQueue;
        
        struct
        {
            FCriticalSection CriticalSection;
            void* ClientBuffer{ nullptr };
        } FrameUpload;
    } Unreal = {0};

    struct {
        bool using_opengl;
        
        struct {
#if PLATFORM_ANDROID
            void* egl_context;
            void* egl_display;
#elif PLATFORM_WINDOWS
            HWND window;
            HGLRC context;
            HDC hdc;
#endif

            GLuint texture;
            GLuint framebuffer;
            GLuint renderbuffer;
            GLuint rhi_interop_memory;

            GLuint pixel_buffer_objects[2];
            GLsync fence;

            GLuint pitch;
            GLuint pixel_type;
            GLuint pixel_format;
            GLuint bits_per_pixel;
        } gl;

        struct {
            void* bgra_buffers[2];
        } software;

        bool free_framebuffer_index;

        const struct retro_hw_render_context_negotiation_interface* hw_render_context_negotiation; // @todo
        struct retro_hw_render_callback hw;
        struct retro_system_av_info av;

        TArray<char, TInlineAllocator<512>>   save_directory,
                                            system_directory;
    } core = { 0 };

public:
    bool &LibretroThread_bottom_left_origin = core.hw.bottom_left_origin;
    struct retro_game_geometry &LibretroThread_geometry = core.av.geometry;

    class FLambdaRunnable* LambdaRunnable{nullptr};
protected:
    // This is where the callback implementation logic is for the callbacks from the Libretro Core
    void    core_video_refresh(const void* data, unsigned width, unsigned height, unsigned pitch);
    void    core_audio_sample(int16_t left, int16_t right);
    size_t  core_audio_write(const int16_t* buf, size_t frames);
    int16_t core_input_state(unsigned port, unsigned device, unsigned index, unsigned id);
    // void   core_input_poll(void);
    bool    core_environment(unsigned cmd, void* data);
    
    // I read somewhere online that you technically have to load OpenGL procedures per context you make on Windows
    // I don't think it actually matters unless you're using multiple rendering devices, and even in that case it might
    // not matter. I still load them per instance anyway to be on the safe side.
    #define DEFINE_GL_PROCEDURES(Type,Func) Type Func = NULL;
    ENUM_GL_PROCEDURES(DEFINE_GL_PROCEDURES);
    ENUM_GL_WIN32_INTEROP_PROCEDURES(DEFINE_GL_PROCEDURES)
    bool gl_win32_interop_supported_by_driver{false};
    
    void create_window();
    void video_configure(const struct retro_game_geometry* geom);

    void load(const char* sofile);
    void load_game(const char* filename);
};