// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/AudioComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/PlayerController.h"
#include "RawAudioSoundWave.h"
#include "LibretroInputComponent.h"
#include "sdlarch.h"
#include "Interfaces/IPluginManager.h"
#include "LibretroCoreInstance.generated.h"



DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnCoreIsReady, const class UTextureRenderTarget2D*, LibretroFramebuffer, const class USoundWave*, AudioBuffer, const bool, BottomLeftOrigin);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnControllerDisconnected, const class APlayerController*, PlayerController, const int, Port);


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class UNREALLIBRETRO_API ULibretroCoreInstance : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	ULibretroCoreInstance();

	// DELEGATE FUNCTIONS

	/**
	 * Issued after the emulator has been successfully launched
	 */
	UPROPERTY(BlueprintAssignable)
	FOnCoreIsReady OnCoreIsReady;
								  
	// BLUEPRINT CALLABLE FUNCTIONS

	/**
	 * @brief Starts the launch process on a background thread
	 *
	 * Functions marked "Ineffective Before Launch" should now function properly.
	 * After the emulator has been successfully launched it will issue the event "On Core Is Ready".
	 */
	UFUNCTION(BlueprintCallable, Category = "Libretro")
	void Launch();

	UFUNCTION(BlueprintCallable, Category = "Libretro|IneffectiveBeforeLaunch")
	void Shutdown();

	/**
	 * @brief Basically the same as loading a state in an emulator.
	 *
	 * The save states are unique based on the Rom's filename.
	 * 
	 * **Caution**: Don't use save states with one ROM on multiple different emulators,
	 * likely their serialization formats will be different and this will trigger an assert.
	 *
	 * @param Identifier - Allows for storing multiple save states per ROM
	 */
	UFUNCTION(BlueprintCallable, Category = "Libretro|IneffectiveBeforeLaunch")
	void LoadState(const FString Identifier = "Default");

	/**
	 * @brief Basically the same as saving a state in an emulator.
	 *
	 * @see LoadState(const FString)
	 */
	UFUNCTION(BlueprintCallable, Category = "Libretro|IneffectiveBeforeLaunch")
	void SaveState(const FString Identifier = "Default");

	/**
	 * @brief Suspends the emulator instance. @details The game will no longer run until you call Pause with false which will resume gameplay.
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


	// EDITOR PROPERTIES

	/**
	 * Optionally needs to be set. This is where the game's framebuffer will be written
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Libretro)
	UTextureRenderTarget2D* RenderTarget;

	UPROPERTY()
	URawAudioSoundWave* AudioBuffer;

	/**
	 * You should provide a path to your ROM relative to the MyROMs directory in the UnrealLibretro directory in your project's Plugins directory.
	 * So if your ROM is at [MyProjectName]/Plugins/UnrealLibretro/MyROMs/myrom.rom this should be set to myrom.rom
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Libretro)
	FString Rom;

	/**
	 * You should provide a path to your Libretro core relative to the MyCores directory in the UnrealLibretro directory in your project's Plugins directory.
	 * So if your Libretro Core is at [MyProjectName]/Plugins/UnrealLibretro/MyCores/mycore.dll this should be set to mycore.dll
	 * You can get Libretro Cores from here https://buildbot.libretro.com/nightly/windows/x86_64/latest/
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Libretro)
	FString Core;

	// @todo: It'd be nice if I could use something like std::wrapped_reference however Unreal doesn't offer an equivalent for now
	/**
	* Handle to the actual libretro core instance.
	*/
	TOptional<LibretroContext*> CoreInstance;


protected:
	virtual void BeginPlay() override;
	virtual void InitializeComponent() override;
	//virtual bool IsReadyForFinishDestroy() override;
	FDelegateHandle ResumeEditor, PauseEditor;
	bool Paused = false;

	TStaticArray<TWeakObjectPtr<APlayerController>, PortCount> Controller{ nullptr };

	UPROPERTY()
	TArray<ULibretroInputComponent*> InputMap;

	TStaticArray<FOnControllerDisconnected, PortCount> Disconnected{ FOnControllerDisconnected() };
	
	FString SaveStatePath(FString Identifier)
	{
		auto LibretroPluginRootPath = IPluginManager::Get().FindPlugin("UnrealLibretro")->GetBaseDir();
		
		return FPaths::Combine(LibretroPluginRootPath, TEXT("Saves"), TEXT("SaveStates"), Rom, Identifier + ".sav");
	}

	FString SRAMPath(FString Identifier)
	{
		auto LibretroPluginRootPath = IPluginManager::Get().FindPlugin("UnrealLibretro")->GetBaseDir();

		return FPaths::Combine(LibretroPluginRootPath, TEXT("Saves"), TEXT("SRAM"), Rom, Identifier + ".srm");
	}

public:
	/**
	 * This is what the libretro core reads from when determining input. If you want to use your own input method you can modify this directly.
	 */
	TSharedRef<TStaticArray<FLibretroInputState, PortCount>, ESPMode::ThreadSafe> InputState;
	
	virtual void BeginDestroy();
};
