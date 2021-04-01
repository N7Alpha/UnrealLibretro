#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "LibretroInputDefinitions.h"
#include "LibretroCoreInstance.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCoreIsReady, const class UTextureRenderTarget2D*, LibretroFramebuffer, const class USoundWave*, AudioBuffer);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCoreFramebufferResize, float, ScaleFillU, float, ScaleFillV);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnControllerDisconnected, const class APlayerController*, PlayerController, const int, Port);


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class UNREALLIBRETRO_API ULibretroCoreInstance : public UActorComponent
{
	GENERATED_BODY()

public:
	/** Lifetime */
	ULibretroCoreInstance();
	virtual void InitializeComponent() override;
	virtual void BeginPlay() override;
	virtual void BeginDestroy();
	


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
	 * @brief Allows a user to control the Game.
	 *
	 * **Note:** This isn't an end all be all solution to handling input. Depending on the kind of functionality you want you might want to write your own solution. You can look at LibretroInputComponent to get an idea of how this might be done.
	 * 
	 * @param PlayerController - The Player Controller that will control the Port. You will probably have to disable input handling if PlayerController is controlling a pawn I don't disable it automatically or use the possession system Unreal uses.
	 * @param Port - Should be set between 0-3. 0 would control first player.
	 * @param ControllerBindings - Bindings between Unreal Keys and Libretro's abstract controller interface. Be careful, the Libretro controller abstraction is counterintuitive since it uses the SNES layout you can read more about it [here](https://retropie.org.uk/docs/RetroArch-Configuration/).
	 * @param OnControllerDisconnected - Called when DisconnectController(Port) is called and is called automatically if the LibretroComponent is destroyed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Libretro")
	void ConnectController(APlayerController* PlayerController, int Port, TMap<FKey, ERetroInput> ControllerBindings, FOnControllerDisconnected OnControllerDisconnected);

	/**
	 * @brief Stops user attached to the port from controlling the game.
	 * 
	 * Causes this Libretro instance to no longer receive input from PlayerController attached to the port and calls the associated On Controller Disconnected delegate.
	 * 
	 * @param Port - Should be set between 0-3. 0 would disconnect first player.
	 */
	UFUNCTION(BlueprintCallable, Category = "Libretro")
	void DisconnectController(int Port);


	/** Blueprint Properties */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Libretro)
	UTextureRenderTarget2D* RenderTarget;

	UPROPERTY(BlueprintReadWrite, Category = Libretro)
	UAudioComponent* AudioComponent;

	/**
	 * This is what the libretro core reads from when determining input. If you want to use your own input method you can modify this directly.
	 */
	TSharedRef<TStaticArray<FLibretroInputState, PortCount>, ESPMode::ThreadSafe> InputState;

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

protected:
	// @todo: It'd be nice if I could use something like std::wrapped_reference however Unreal doesn't offer an equivalent for now
	TOptional<struct LibretroContext*> CoreInstance;

	bool Paused = false;

	UPROPERTY()
	USoundWave* AudioBuffer;
	
	UPROPERTY()
	TArray<class ULibretroInputComponent*> InputMap;

	TStaticArray<TWeakObjectPtr<APlayerController>, PortCount> Controller{ nullptr };

	TStaticArray<FOnControllerDisconnected, PortCount> Disconnected{ FOnControllerDisconnected() };
};
