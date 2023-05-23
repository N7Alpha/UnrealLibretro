#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Components/AudioComponent.h"
#include "LibretroInputDefinitions.h"
#include "Engine/TextureRenderTarget2D.h"
#include "LibretroCoreInstance.generated.h"

USTRUCT(BlueprintType)
struct FLibretroOptions
{
	GENERATED_BODY()

	static CONSTEXPR int DefaultOptionIndex = 0; // By libretro convention

	UPROPERTY(BlueprintReadOnly)
	FString         Key;

	UPROPERTY(BlueprintReadOnly)
	FString         Description;

	UPROPERTY(BlueprintReadOnly)
	TArray<FString> Values;
};

USTRUCT(BlueprintType)
struct FLibretroControllerDescription // retro_controller_description
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere)
	FString Description{"Unspecified"};

	UPROPERTY(VisibleAnywhere)
	unsigned int ID{RETRO_DEVICE_DEFAULT};
};

USTRUCT(BlueprintType)
struct FLibretroControllerDescriptions
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere)
	FLibretroControllerDescription ControllerDescription[PortCount];

	      FLibretroControllerDescription& operator[](int Port)       { return ControllerDescription[Port]; }
	const FLibretroControllerDescription& operator[](int Port) const { return ControllerDescription[Port]; }
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCoreIsReady, const class UTextureRenderTarget2D*, LibretroFramebuffer, const class USoundWave*, AudioBuffer);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCoreFramebufferResize);


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class UNREALLIBRETRO_API ULibretroCoreInstance : public UActorComponent
{
	GENERATED_BODY()

public:
	/** Lifetime */
	ULibretroCoreInstance();
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void BeginDestroy();
	
	friend class FLibretroCoreInstanceDetails;

	/** Delegate Functions */
	/**
	 * Issued after the emulator has been successfully launched
	 */
	UPROPERTY(BlueprintAssignable)
	FOnCoreIsReady OnCoreIsReady;

	/**
	 * Issued initially whenever the core framebuffer is changes dimensions. The arguments provided will scale uv's appropriately to exactly fit the framebuffer
	 */
	UPROPERTY(BlueprintAssignable)
	FOnCoreFramebufferResize OnCoreFrameBufferResize;

								  
	/** Blueprint Callable Functions */
	/**
	 * @brief Starts the launch process on a background thread
	 * After the emulator has been successfully launched it will issue the event "On Core Is Ready".
	 * 
	 * **Note:** This will implicitly call Shutdown if a Core is already running
	 * 
	 * **Postcondition:** Immediately after calling this function functions marked "Ineffective Before Launch" should now function properly.
	 */
	UFUNCTION(BlueprintCallable, Category = "Libretro")
	void Launch();

	UFUNCTION(BlueprintCallable, Category = "Libretro|IneffectiveBeforeLaunch")
	void Shutdown();

	/**
	 * @brief Basically the same as loading a state in an emulator
	 *
	 * The save states are unique based on the Rom's filename and the passed identifier.
	 * 
	 * **Caution**: Don't use save states with one ROM on multiple different emulators,
	 * likely their serialization formats will be different.
	 *
	 * @param FilePath - Allows for storing multiple save states per ROM
	 * 
	 * @see https://higan.readthedocs.io/en/stable/concepts/save-states/#save-states-versus-in-game-saves
	 */
	UFUNCTION(BlueprintCallable, Category = "Libretro|IneffectiveBeforeLaunch")
	void LoadState(const FString &FilePath = "Default.sav");

	/**
	 * @brief Basically the same as saving a state in an emulator
	 *
	 * @see LoadState(const FString)
	 */
	UFUNCTION(BlueprintCallable, Category = "Libretro|IneffectiveBeforeLaunch")
	void SaveState(const FString &FilePath = "Default.sav");

	/**
	 * @brief Suspends the emulator instance @details The game will no longer run until you call Pause with false which will resume gameplay.
	 * 
	 * @param ShouldPause - Passing true will Suspend the emulator, false will resume it.
	 */
	UFUNCTION(BlueprintCallable, Category = "Libretro|IneffectiveBeforeLaunch")
	void Pause(bool ShouldPause = true);

	/**
	 * @brief Effectively a getter version of `retro_set_controller_port_device`
	 *
	 * @see `retro_set_controller_port_device` in libretro.h
	 */
	UFUNCTION(BlueprintCallable, Category = "Libretro|IneffectiveBeforeCoreIsReady")
	void GetController(int Port, int64 &ID, FString &Description);

	/**
	 * @brief Sets the state of a button for the Libretro core
	 * 
	 * Bindings in ConnectController will override inputs set here
	 * You can set analog inputs through this interface as well however it won't work well
	 * 
	 */
	UFUNCTION(BlueprintCallable, Category = "Libretro|IneffectiveBeforeLaunch")
	void SetInputDigital(int Port, bool Activated, ERetroDeviceID Input);

	/**
	 * @brief Sets the analog state of an input for the Libretro core
	 * 
	 * This should mainly only be used for analog stick inputs and analog triggers
	 * The internal datatype returned when input is queried by the Libretro Core is int16
	 * for digital inputs its 1 or 0 in which case use SetInputDigital
	 */
	UFUNCTION(BlueprintCallable, Category = "Libretro|IneffectiveBeforeLaunch")
	void SetInputAnalog(int Port, int _16BitSignedInteger, ERetroDeviceID Input);

	/** 
	 * @brief Where the Libretro Core's frame is drawn
	 * 
	 * Tip: While running with PIE you can visually examine this 
     * 1. Click Eject or Press F8
     * 2. Click on the actor with the rendertarget you want to look at 
     * 3. Click on the LibretroCoreInstance component
     * 4. Click the rendertarget in the details 
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Libretro)
	UTextureRenderTarget2D* RenderTarget;

	UPROPERTY(BlueprintReadWrite, Category = Libretro)
	UAudioComponent* AudioComponent;

	/**
	 * @brief A key-value map for configuring Libretro Cores
	 * 
	 * Options are appended to this list when you change them in the 'Libretro Core Options' section of the details panel
	 * They are only added here if they differ from their default
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Libretro)
	TMap<FString, FString> CoreOptions;

	/**
	 * Making @todo probably don't make this part of the public interface
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Libretro, meta = (ShowInnerProperties))
	TMap<FString, FLibretroControllerDescriptions> ControllersSetOnLaunch;

	/**
	 * You should provide a path to your ROM relative to the MyROMs directory in the UnrealLibretro directory in your project's Plugins directory.
	 * So if your ROM is at [MyProjectName]/Plugins/UnrealLibretro/MyROMs/myrom.rom this should be set to myrom.rom
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Libretro)
	FString RomPath;

	/**
	 * You should provide a path to your Libretro core relative to the MyCores directory in the UnrealLibretro directory in your project's Plugins directory.
	 * So if your Libretro Core is at [MyProjectName]/Plugins/UnrealLibretro/MyCores/mycore.dll this should be set to mycore.dll
	 * You can get Libretro Cores from here https://buildbot.libretro.com/nightly/windows/x86_64/latest/
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Libretro)
	FString CorePath;

	/**
	 * Provided by some cores. FPath is either absolute or relative to $(PluginDir)/UnrealLibretro/Saves/SaveStates/$(RomFileName)/  
	 * This would only be needed if you were running two instances of the same game at once or you were specifying some other path on the filesystem
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Libretro, AdvancedDisplay)
	FString SRAMPath = "Default.srm";


	/** These properties are with respect to how the frame is drawn by the Libretro Core in the framebuffer it's provided */
	UPROPERTY(BlueprintReadOnly, Category = Libretro)
	float FrameRotation;

	UPROPERTY(BlueprintReadOnly, Category = Libretro)
	int FrameWidth;

	UPROPERTY(BlueprintReadOnly, Category = Libretro)
	int FrameHeight;

	UPROPERTY(BlueprintReadOnly, Category = Libretro)
	bool bFrameBottomLeftOrigin;

	/** Forward keyboard input from this APlayerController to the libretro core if the core can receive it */
	UPROPERTY(BlueprintReadWrite, Category = Libretro)
	APlayerController* KeyboardInputSourcePlayerController{nullptr};

protected:

	// @todo: It'd be nice if I could use something like std::wrapped_reference however Unreal doesn't offer an equivalent for now
	TOptional<struct LibretroContext*> CoreInstance;

	bool Paused = false;

	UPROPERTY()
	USoundWave* AudioBuffer;
};
