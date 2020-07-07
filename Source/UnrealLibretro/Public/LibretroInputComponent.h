// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/InputComponent.h"
#include "sdlarch.h"

#include "LibretroInputComponent.generated.h"


#define BindUnrealButtonToLibretro(RetroButton, UnrealButton)			   BindKey    (UnrealButton, IE_Pressed,  this, &ULibretroInputComponent::ButtonPressed <RetroButton>); \
																		   BindKey    (UnrealButton, IE_Released, this, &ULibretroInputComponent::ButtonReleased<RetroButton>);

class ULibretroCoreInstance;

// DO NOT REORDER THESE
UENUM(BlueprintType)
enum  class ERetroInput : uint8 {
	B,
	Y,
	SELECT,
	START,
	UP,
	DOWN,
	LEFT,
	RIGHT,
	A,
	X,
	L,
	R,
	L2,
	R2,
	L3,
	R3,
	LeftX,
	LeftY,
	RightX,
	RightY,
	DisconnectController,

	DigitalCount = LeftX UMETA(Hidden),
	AnalogCount = DisconnectController - DigitalCount UMETA(Hidden)
};

UCLASS(ClassGroup=(Custom), hidecategories = (Activation, "Components|Activation"))
class UNREALLIBRETRO_API ULibretroInputComponent : public UInputComponent
{
	GENERATED_BODY()
public:
	void BindKeys(const TMap<FKey, ERetroInput> &ControllerBindings);

	ULibretroCoreInstance* LibretroCoreInstance;
	int Port = 0;

	template<unsigned RetroButton>
	void ButtonPressed();

	template<unsigned RetroButton>
	void ButtonReleased();

	template<unsigned RetroAxis, unsigned RetroDirection>
	void AxisChanged(float Value);

protected:
	
	
	static TArray<void (ULibretroInputComponent::*)(), TFixedAllocator<(uint32)ERetroInput::DigitalCount>>      ButtonPressedFunctions;

	static TArray<void (ULibretroInputComponent::*)(), TFixedAllocator<(uint32)ERetroInput::DigitalCount>>      ButtonReleasedFunctions;

	static TArray<void (ULibretroInputComponent::*)(float), TFixedAllocator<(uint32)ERetroInput::AnalogCount>>  ButtonAnalog;

	void DisconnectController();

	virtual void BeginPlay() override;	


};
