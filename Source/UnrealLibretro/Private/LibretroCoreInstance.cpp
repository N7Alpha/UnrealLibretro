// Fill out your copyright notice in the Description page of Project Settings.


#include "LibretroCoreInstance.h"
#include "LibretroInputComponent.h"
#include "RHIResources.h"
#include "libretro/libretro.h"
#include "LambdaRunnable.h"
#include <Runtime\Core\Public\HAL\PlatformFilemanager.h>
#include "Misc/MessageDialog.h"


#include "Interfaces/IPluginManager.h"



ULibretroCoreInstance::ULibretroCoreInstance()
{
	PrimaryComponentTick.bCanEverTick = false;

	ResumeEditor = FEditorDelegates::ResumePIE.AddLambda([this](const bool bIsSimulating)
		{
			this->instance->UnrealThreadTask->Thread->Suspend(Paused); // This is buggy replace with optional
		});
	PauseEditor = FEditorDelegates::PausePIE.AddLambda([this](const bool bIsSimulating)
		{
			this->instance->UnrealThreadTask->Thread->Suspend(true);
		});

	Controller.InsertDefaulted(0, PortCount);
	Disconnected.InsertDefaulted(0, PortCount);
	for (int Port = 0; Port < PortCount; Port++) 
	{
		InputMap.Add(NewObject<ULibretroInputComponent>());
		InputMap[Port]->Port = Port;
		InputMap[Port]->LibretroCoreInstance = this;
	}
}

TMap<FKey, ERetroInput> ULibretroCoreInstance::CombineInputMaps(const TMap<FKey, ERetroInput> &InMap1, const TMap<FKey, ERetroInput> &InMap2) {
	auto OutMap(InMap1);
	OutMap.Append(InMap2);
	return OutMap;
}

void ULibretroCoreInstance::ConnectController(APlayerController* PlayerController, int Port, TMap<FKey, ERetroInput> ControllerBindings, FOnControllerDisconnected OnControllerDisconnected)
{
	if (instance) {
		check(Port >= 0 && Port < PortCount);
		Controller[Port] = MakeWeakObjectPtr(PlayerController);
		Disconnected[Port] = OnControllerDisconnected;

		InputMap[Port]->BindKeys(ControllerBindings);

		PlayerController->PushInputComponent(InputMap[Port]);
	}
	else {
		UE_LOG(Libretro, Warning, TEXT("Tried to connect controller before the libretro instance finished launching. If you want to attach the controller right when the console starts try using the On Core Is Ready delegate."));
	}
	
}

void ULibretroCoreInstance::DisconnectController(int Port) {
	check(Port >= 0 && Port < PortCount);

	InputMap[Port]->KeyBindings.Empty();
	auto PlayerController = Controller[Port];

	if (PlayerController.IsValid())
	{
		TInlineComponentArray<ULibretroInputComponent*> InputTypes(GetOwner());

		PlayerController->PopInputComponent(InputMap[Port]);

		Disconnected[Port].ExecuteIfBound(PlayerController.Get(), Port);
	}
	else {
		UE_LOG(Libretro, Warning, TEXT("Tried to disconnect controller from port %d, but the stored reference for PlayerController was null. This means either (likely) one wasn't attached to that port or (unlikely) the player controller was garbage collected"), Port);
	}
}

void ULibretroCoreInstance::Launch() 
{
	Rom = Rom.IsEmpty() ? "MAZE" : Rom;
	Core = Core.IsEmpty() ? "emux_chip8_libretro.dll" : Core;

	auto LibretroPluginRootPath = IPluginManager::Get().FindPlugin("UnrealLibretro")->GetBaseDir();
	auto CorePath = FPaths::Combine(LibretroPluginRootPath, TEXT("MyCores"), Core);
	auto RomPath  = FPaths::Combine(LibretroPluginRootPath, TEXT("MyROMs"), Rom );

	if (!IPlatformFile::GetPlatformPhysical().FileExists(*CorePath))
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::AsCultureInvariant("Failed to launch Libretro core. Couldn't find core at path " + IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*CorePath)));
		return;
	}
	else if (!IPlatformFile::GetPlatformPhysical().FileExists(*RomPath))
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::AsCultureInvariant("Failed to launch Libretro core " + Core + ". Couldn't find ROM at path " + IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*RomPath)));
		return;
	}

	if (!AudioBuffer) 
	{
		AudioBuffer = NewObject<URawAudioSoundWave>();
	}

	if (!RenderTarget)
	{
		RenderTarget = NewObject<UTextureRenderTarget2D>();
	}

	RenderTarget->Filter = TF_Nearest;

	this->instance = LibretroContext::launch(CorePath, RomPath, RenderTarget, AudioBuffer,
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

	/*if (Scalability::GetQualityLevels().AntiAliasingQuality) {
		FMessageDialog::Open(EAppMsgType::Ok, FText::AsCultureInvariant("You have temporal anti-aliasing enabled. The emulated games will look will look blurry and laggy if you leave this enabled. If you happen to know how to fix this let me know. I tried enabling responsive AA on the material to prevent this, but that didn't work."));
	}*/
}

// @todo: Using this function I think would be the proper way of keeping the UObjects I'm using in LibretroContext alive instead of the weak pointer shared pointer mess I'm currently using. However its kind of a low priority since the current system I'm using works fine
// bool ULibretroCoreInstance::IsReadyForFinishDestroy() { return false;  }
void ULibretroCoreInstance::BeginDestroy()
{
	if (instance) {
		instance->running = false;
	}

	for (int Port = 0; Port < PortCount; Port++) {
		DisconnectController(Port);
	}
	
	FEditorDelegates::ResumePIE.Remove(ResumeEditor);
	FEditorDelegates::PausePIE .Remove(PauseEditor );

	Super::BeginDestroy();
}
