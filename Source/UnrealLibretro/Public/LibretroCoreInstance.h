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
#include "sdlarch.h"
#include "LibretroCoreInstance.generated.h"

//class ULibretroJoypadInputComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnCoreIsReady, const class UTextureRenderTarget2D*, LibretroFramebuffer, const class USoundWave*, AudioBuffer, const bool, BottomLeftOrigin);

/** shadow delegate declaration for above */
DECLARE_MULTICAST_DELEGATE_FourParams(FOnCoreIsReadyNative, const class ULibretroCoreInstance*, const class UTextureRenderTarget2D*, const class USoundWave*, const bool);

class ULibretroJoypadInputComponent;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class UNREALLIBRETRO_API ULibretroCoreInstance : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	ULibretroCoreInstance();

	// DELEGATE FUNCTIONS
	UPROPERTY(BlueprintAssignable)
	FOnCoreIsReady OnCoreIsReady;

	FOnCoreIsReadyNative OnCoreIsReadyNative; // @todo not really sure how much this OnCoreIsReadyNative delegate is needed I was just mimicking Unreal Engine example code


	// BLUEPRINT CALLABLE FUNCTIONS
	UFUNCTION(BlueprintCallable, Category = "Libretro")
	void Launch();

	UFUNCTION(BlueprintCallable, Category = "Libretro")
	void Pause(bool ShouldPause);

	UFUNCTION(BlueprintCallable, Category = "Libretro")
	void SetController(APlayerController* PlayerController);

	

	// EDITOR PROPERTIES
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Libretro)
	UTextureRenderTarget2D* RenderTarget;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Libretro)
	URawAudioSoundWave *AudioBuffer;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Libretro)
	FString Rom;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Libretro)
	FString Core;

	LibretroContext* instance = nullptr;


protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	FDelegateHandle ResumeEditor, PauseEditor;
	bool Paused = false;




public:	
	virtual void BeginDestroy();
};
