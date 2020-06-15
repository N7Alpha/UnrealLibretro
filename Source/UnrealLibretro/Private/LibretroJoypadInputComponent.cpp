// Fill out your copyright notice in the Description page of Project Settings.

#include "LibretroJoypadInputComponent.h"
#include "LibretroCoreInstance.h"

template<unsigned RetroDirection, unsigned RetroAxis>
void ULibretroJoypadInputComponent::AxisChanged(float Value) {
	float coff = RetroDirection ? -1 : 1;
	LibretroCoreInstance->instance->analog[RetroDirection][RetroAxis] = (int16_t)FMath::RoundHalfToEven(coff * 0x7FFF * Value);
}


																		   
#define BindUnrealAxisToLibretro(RetroAxis, RetroDirection, UnrealAxis)    BindAxisKey(UnrealAxis,				  this,	&ULibretroJoypadInputComponent::AxisChanged   <RetroAxis, RetroDirection>);


// Called when the game starts
void ULibretroJoypadInputComponent::BeginPlay()
{
	Super::BeginPlay();
	
	BindUnrealButtonToLibretro(RETRO_DEVICE_ID_JOYPAD_B, EKeys::Gamepad_FaceButton_Right);
	BindUnrealButtonToLibretro(RETRO_DEVICE_ID_JOYPAD_Y, EKeys::Gamepad_FaceButton_Top);
	BindUnrealButtonToLibretro(RETRO_DEVICE_ID_JOYPAD_SELECT, EKeys::Gamepad_Special_Left);
	BindUnrealButtonToLibretro(RETRO_DEVICE_ID_JOYPAD_START, EKeys::Gamepad_Special_Right);
	BindUnrealButtonToLibretro(RETRO_DEVICE_ID_JOYPAD_UP, EKeys::Gamepad_DPad_Up);
	BindUnrealButtonToLibretro(RETRO_DEVICE_ID_JOYPAD_DOWN, EKeys::Gamepad_DPad_Down);
	BindUnrealButtonToLibretro(RETRO_DEVICE_ID_JOYPAD_LEFT, EKeys::Gamepad_DPad_Left);
	BindUnrealButtonToLibretro(RETRO_DEVICE_ID_JOYPAD_RIGHT, EKeys::Gamepad_DPad_Right);
	BindUnrealButtonToLibretro(RETRO_DEVICE_ID_JOYPAD_A, EKeys::Gamepad_FaceButton_Bottom);
	BindUnrealButtonToLibretro(RETRO_DEVICE_ID_JOYPAD_X, EKeys::Gamepad_FaceButton_Left);
	BindUnrealButtonToLibretro(RETRO_DEVICE_ID_JOYPAD_L, EKeys::Gamepad_LeftTrigger);
	BindUnrealButtonToLibretro(RETRO_DEVICE_ID_JOYPAD_R, EKeys::Gamepad_RightTrigger);
	BindUnrealButtonToLibretro(RETRO_DEVICE_ID_JOYPAD_L2, EKeys::Gamepad_LeftShoulder);
	BindUnrealButtonToLibretro(RETRO_DEVICE_ID_JOYPAD_R2, EKeys::Gamepad_RightShoulder);
	BindUnrealButtonToLibretro(RETRO_DEVICE_ID_JOYPAD_L3, EKeys::Gamepad_LeftThumbstick);
	BindUnrealButtonToLibretro(RETRO_DEVICE_ID_JOYPAD_R3, EKeys::Gamepad_RightThumbstick);

	BindUnrealAxisToLibretro(RETRO_DEVICE_ID_ANALOG_X, RETRO_DEVICE_INDEX_ANALOG_LEFT, EKeys::Gamepad_LeftX);
	BindUnrealAxisToLibretro(RETRO_DEVICE_ID_ANALOG_Y, RETRO_DEVICE_INDEX_ANALOG_LEFT, EKeys::Gamepad_LeftY);
	BindUnrealAxisToLibretro(RETRO_DEVICE_ID_ANALOG_X, RETRO_DEVICE_INDEX_ANALOG_RIGHT, EKeys::Gamepad_RightX);
	BindUnrealAxisToLibretro(RETRO_DEVICE_ID_ANALOG_Y, RETRO_DEVICE_INDEX_ANALOG_RIGHT, EKeys::Gamepad_RightY);
	
}
