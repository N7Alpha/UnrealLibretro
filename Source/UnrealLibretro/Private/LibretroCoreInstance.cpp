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
	: InputState(MakeShared<TStaticArray<FLibretroInputState, PortCount>, ESPMode::ThreadSafe>())
{
	PrimaryComponentTick.bCanEverTick = false;
	bWantsInitializeComponent = true;

	InputMap.AddZeroed(PortCount);
}
 
void ULibretroCoreInstance::ConnectController(APlayerController* PlayerController, int Port, TMap<FKey, ERetroInput> ControllerBindings, FOnControllerDisconnected OnControllerDisconnected)
{
	check(Port >= 0 && Port < PortCount);

	DisconnectController(Port);

	Controller[Port] = MakeWeakObjectPtr(PlayerController);
	Disconnected[Port] = OnControllerDisconnected;

	InputMap[Port]->KeyBindings.Empty();
	InputMap[Port]->BindKeys(ControllerBindings);
	PlayerController->PushInputComponent(InputMap[Port]);
	
}

void ULibretroCoreInstance::DisconnectController(int Port)
{
	check(Port >= 0 && Port < PortCount);

	auto PlayerController = Controller[Port];

	if (PlayerController.IsValid())
	{
		PlayerController->PopInputComponent(InputMap[Port]);
	}

	Disconnected[Port].ExecuteIfBound(PlayerController.Get(), Port);
}

void ULibretroCoreInstance::Launch() 
{
	Shutdown();

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

	this->CoreInstance = LibretroContext::Launch(CorePath, RomPath, RenderTarget, AudioBuffer, InputState,
		[weakThis = MakeWeakObjectPtr(this)](bool bottom_left_origin) 
			{ // Core Loaded
			FGraphEventRef Task = FFunctionGraphTask::CreateAndDispatchWhenReady([=]()
				{
					if (weakThis.IsValid()) 
					{
						weakThis->OnCoreIsReady.Broadcast(weakThis->RenderTarget, weakThis->AudioBuffer, bottom_left_origin);
						weakThis->OnCoreIsReadyNative.Broadcast(weakThis.Get(), weakThis->RenderTarget, weakThis->AudioBuffer, bottom_left_origin);
					}

				}, TStatId(), NULL, ENamedThreads::GameThread);
			});
}

void ULibretroCoreInstance::Pause(bool ShouldPause)
{
	if (CoreInstance.IsSet()) 
	{
		CoreInstance.GetValue()->Pause(ShouldPause);
		Paused = ShouldPause;
	}
}

void ULibretroCoreInstance::Shutdown() {
	if (CoreInstance.IsSet())
	{
		LibretroContext::Shutdown(CoreInstance.GetValue());
		CoreInstance.Reset();
	}
}

#include "Editor.h"
#include "Scalability.h"

void ULibretroCoreInstance::InitializeComponent() {
	ResumeEditor = FEditorDelegates::ResumePIE.AddLambda([this](const bool bIsSimulating)
		{
			// This could have wierd behavior if CoreInstance is launched when the editor is paused. That really shouldn't ever happen though
			if (this->CoreInstance.IsSet())
			{
				this->CoreInstance.GetValue()->Pause(Paused);
			}
			
		});
	PauseEditor = FEditorDelegates::PausePIE.AddLambda([this](const bool bIsSimulating)
		{
			if (this->CoreInstance.IsSet())
			{
				this->CoreInstance.GetValue()->Pause(true);
			}
		});

	for (int Port = 0; Port < PortCount; Port++)
	{
		InputMap[Port] = NewObject<ULibretroInputComponent>();
		InputMap[Port]->Initialize(&(*InputState)[Port], [Port, this]() { this->DisconnectController(Port); });
	}
}

void ULibretroCoreInstance::BeginPlay()
{
	Super::BeginPlay();

	/*if (Scalability::GetQualityLevels().AntiAliasingQuality) {
		FMessageDialog::Open(EAppMsgType::Ok, FText::AsCultureInvariant("You have temporal anti-aliasing enabled. The emulated games will look will look blurry and laggy if you leave this enabled. If you happen to know how to fix this let me know. I tried enabling responsive AA on the material to prevent this, but that didn't work."));
	}*/
}

// @todo: Using this function I think would be the proper way of keeping the UObjects I'm using in LibretroContext alive instead of the weak pointer shared pointer mess I'm currently using. However its kind of a low priority since the current system I'm using works fine.
//		  The thing I worry about though is if delaying destruction for a few seconds is simply too long Unreal AFAIK only uses this to delay destruction until render resources are deleted which is no more than a few frames
//bool ULibretroCoreInstance::IsReadyForFinishDestroy() { return false;  }

void ULibretroCoreInstance::BeginDestroy()
{
	for (int Port = 0; Port < PortCount; Port++)
	{
		DisconnectController(Port);
	}
	
	Shutdown();

	FEditorDelegates::ResumePIE.Remove(ResumeEditor);
	FEditorDelegates::PausePIE .Remove(PauseEditor );

	Super::BeginDestroy();
}
