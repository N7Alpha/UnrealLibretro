// Fill out your copyright notice in the Description page of Project Settings.


#include "LibretroCoreInstance.h"
#include "LibretroInputComponent.h"
#include "RHIResources.h"
#include "libretro/libretro.h"
#include "LambdaRunnable.h"
#include <Runtime\Core\Public\HAL\PlatformFilemanager.h>
#include "Misc/MessageDialog.h"


#include "Interfaces/IPluginManager.h"

#include "sdlarch.h"



// Sets default values for this component's properties
ULibretroCoreInstance::ULibretroCoreInstance()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	// ...
}

void ULibretroCoreInstance::SetController(APlayerController* PlayerController) 
{
	TInlineComponentArray<ULibretroInputComponent*> InputTypes(GetOwner());
	check(instance);
	for (auto LibretroInputComponent : InputTypes)
	{
		PlayerController->PushInputComponent(LibretroInputComponent);
	}
}

void ULibretroCoreInstance::Launch() 
{
	Rom = Rom.IsEmpty() ? "MAZE" : Rom;
	Core = Core.IsEmpty() ? "emux_chip8_libretro.dll" : Core;

	auto LibretroPluginRootPath = IPluginManager::Get().FindPlugin("UnrealLibretro")->GetBaseDir();
	auto filePath = FPaths::Combine(LibretroPluginRootPath, TEXT("MyCores"), Core);
	auto RomPath  = FPaths::Combine(LibretroPluginRootPath, TEXT("MyROMs"), Rom );

	if (!AudioBuffer) {
		AudioBuffer = NewObject<URawAudioSoundWave>();
	}

	if (!RenderTarget) {
		RenderTarget = NewObject<UTextureRenderTarget2D>();
	}

	RenderTarget->Filter = TF_Nearest;

	this->instance = LibretroContext::launch(filePath, RomPath, RenderTarget, AudioBuffer,
		[weakThis = MakeWeakObjectPtr(this)](auto context) 
			{ // Core Loaded
			FGraphEventRef Task = FFunctionGraphTask::CreateAndDispatchWhenReady([=]()
				{
					if (weakThis.IsValid()) 
					{
						weakThis->instance = context;
						weakThis->OnCoreIsReady.Broadcast(weakThis->RenderTarget, weakThis->AudioBuffer, context->g_video.hw.bottom_left_origin);
						weakThis->OnCoreIsReadyNative.Broadcast(weakThis.Get(), weakThis->RenderTarget, weakThis->AudioBuffer, context->g_video.hw.bottom_left_origin);
					}

				}, TStatId(), NULL, ENamedThreads::GameThread);
			});
}

void ULibretroCoreInstance::Pause(bool ShouldPause)
{
	if (instance) 
	{
		instance->UnrealThreadTask->Thread->Suspend(ShouldPause);
		Paused = ShouldPause;
	}
	else 
	{
		UE_LOG(Libretro, Warning, TEXT("Tried to resume core that hadn't been launched yet."));
	}
}


#include "Editor.h"
#include "Scalability.h"


void ULibretroCoreInstance::BeginPlay()
{
	Super::BeginPlay();
	ResumeEditor = FEditorDelegates::ResumePIE.AddLambda([this](const bool bIsSimulating) {
		this->instance->UnrealThreadTask->Thread->Suspend(Paused);
		});
	PauseEditor  = FEditorDelegates::PausePIE .AddLambda([this](const bool bIsSimulating) {
		this->instance->UnrealThreadTask->Thread->Suspend(true);
		});

	if (Scalability::GetQualityLevels().AntiAliasingQuality) {
		//FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("test", "You have temporal anti-aliasing enabled. The emulated games will look will look blurry and laggy if you leave this enabled. If you happen to know how to fix this let me know. I tried enabling responsive AA on the material to prevent this, but that didn't work."), LOCTEXT("test", "Temporal Anti-Aliasing is enabled"));
	}
}

void ULibretroCoreInstance::BeginDestroy()
{
	if (instance) {
		instance->running = false;
	}
	
	FEditorDelegates::ResumePIE.Remove(ResumeEditor);
	FEditorDelegates::PausePIE .Remove(PauseEditor );

	Super::BeginDestroy();
}
