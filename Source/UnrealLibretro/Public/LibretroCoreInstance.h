// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include <Runtime\Engine\Classes\Engine\StaticMeshActor.h>
#include <Runtime\Engine\Classes\Engine\TextureRenderTarget2D.h>
#include "Components/AudioComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/PlayerController.h"
#include "RawAudioSoundWave.h"
#include "LibretroInputComponent.h"
#include "sdlarch.h"
#include "LibretroCoreInstance.generated.h"



DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnCoreIsReady, const class UTextureRenderTarget2D*, LibretroFramebuffer, const class USoundWave*, AudioBuffer, const bool, BottomLeftOrigin);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnControllerDisconnected, const class APlayerController*, PlayerController, const int, Port);

/** shadow delegate declaration for above */
DECLARE_MULTICAST_DELEGATE_FourParams(FOnCoreIsReadyNative, const class ULibretroCoreInstance*, const class UTextureRenderTarget2D*, const class USoundWave*, const bool);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class UNREALLIBRETRO_API ULibretroCoreInstance : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	ULibretroCoreInstance();
	UFUNCTION(BlueprintCallable, BlueprintPure)
	static TMap<FKey, ERetroInput> CombineInputMaps(const TMap<FKey, ERetroInput> &InMap1, const TMap<FKey, ERetroInput> &InMap2);

	// DELEGATE FUNCTIONS

	UPROPERTY(BlueprintAssignable)
	FOnCoreIsReady OnCoreIsReady;

	FOnCoreIsReadyNative OnCoreIsReadyNative; // @todo not really sure how much this OnCoreIsReadyNative delegate is needed I was just mimicking Unreal Engine example code


	
											  
	// BLUEPRINT CALLABLE FUNCTIONS

	/**
	 * Starts the launch process on a background thread. After the emulator has been successfully launched it will issue the event "On Core Is Ready".
	 */
	UFUNCTION(BlueprintCallable, Category = "Libretro")
	void Launch();

	/**
	 * Suspends the emulator instance. The game will no longer run until you call Pause with false which will resume gameplay.
	 * @param ShouldPause - Passing true will Suspend the emulator, false will resume it.
	 */
	UFUNCTION(BlueprintCallable, Category = "Libretro")
	void Pause(bool ShouldPause);

	/**
	 * Disables input on PlayerCharacter and sets PlayerCharacter's APlayerController to control the port of the console using sibling LibretroInputComponent's attached to the AActor.
	 * PlayerCharacter's input is reenabled once DisconnectController is called.
	 * Note: This isn't an end all be all solution to handling input. Depending on the kind of functionality you want you might want to write your own solution. You can look at LibretroInputComponent to get an idea of how this might be done.
	 * @param PlayerController - The Player Controller that will control the Port. You will probably have to disable input handling if PlayerController is controlling a pawn I don't disable it automatically or use the posession system Unreal uses.
	 * @param Port - Should be set between 0-3. 0 would control first player.
	 * @param ControllerBindings - Bindings between Unreal Keys and Libretros abstract controller interface. Be careful, the Libretro controller abstraction is counterintuitive since it uses the SNES layout you can read more about it here https://retropie.org.uk/docs/RetroArch-Configuration/.
	 * @param OnControllerDisconnected - Called when DisconnectController(Port) is called and is called automatically if the LibretroComponent is destroyed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Libretro")
	void ConnectController(APlayerController* PlayerController, int Port, TMap<FKey, ERetroInput> ControllerBindings, FOnControllerDisconnected OnControllerDisconnected);

	/**
	 * Causes this Libretro instance to no longer recieves input from PlayerController attached to the port and calls the associated On Controller Disconnected delegate.
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

	LibretroContext* instance = nullptr;


protected:
	virtual void BeginPlay() override;
	virtual void InitializeComponent() override;
	//virtual bool IsReadyForFinishDestroy() override;
	FDelegateHandle ResumeEditor, PauseEditor;
	bool Paused = false;

	TArray<TWeakObjectPtr<APlayerController>, TFixedAllocator<PortCount>> Controller;

	UPROPERTY()
	TArray<ULibretroInputComponent*> InputMap;

	TArray<FOnControllerDisconnected, TFixedAllocator<PortCount>> Disconnected;

public:
	/**
	 * This is what the libretro core reads from when determining input. If you want to use your own input method you can modify this directly.
	 */
	TSharedPtr<TStaticArray<FLibretroInputState, PortCount>, ESPMode::ThreadSafe> InputState;
	
	virtual void BeginDestroy();
};
