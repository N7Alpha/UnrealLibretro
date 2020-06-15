// Fill out your copyright notice in the Description page of Project Settings.


#include "LibretroKeyboardInputComponent.h"

// Called when the game starts
void ULibretroKeyboardInputComponent::BeginPlay()
{
	Super::BeginPlay();

	BindUnrealButtonToLibretro(RETRO_DEVICE_ID_JOYPAD_B, EKeys::X);
	BindUnrealButtonToLibretro(RETRO_DEVICE_ID_JOYPAD_Y, EKeys::V);
	BindUnrealButtonToLibretro(RETRO_DEVICE_ID_JOYPAD_SELECT, EKeys::LeftShift);
	BindUnrealButtonToLibretro(RETRO_DEVICE_ID_JOYPAD_START, EKeys::Enter);
	BindUnrealButtonToLibretro(RETRO_DEVICE_ID_JOYPAD_UP, EKeys::Up);
	BindUnrealButtonToLibretro(RETRO_DEVICE_ID_JOYPAD_DOWN, EKeys::Down);
	BindUnrealButtonToLibretro(RETRO_DEVICE_ID_JOYPAD_LEFT, EKeys::Left);
	BindUnrealButtonToLibretro(RETRO_DEVICE_ID_JOYPAD_RIGHT, EKeys::Right);
	BindUnrealButtonToLibretro(RETRO_DEVICE_ID_JOYPAD_A, EKeys::Z);
	BindUnrealButtonToLibretro(RETRO_DEVICE_ID_JOYPAD_X, EKeys::C);
	BindUnrealButtonToLibretro(RETRO_DEVICE_ID_JOYPAD_L, EKeys::F);
	BindUnrealButtonToLibretro(RETRO_DEVICE_ID_JOYPAD_R, EKeys::G);
	
}
