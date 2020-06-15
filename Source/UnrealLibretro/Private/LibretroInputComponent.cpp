// Fill out your copyright notice in the Description page of Project Settings.


#include "LibretroInputComponent.h"

// Called when the game starts
void ULibretroInputComponent::BeginPlay()
{
	Super::BeginPlay();

	LibretroCoreInstance = GetOwner()->FindComponentByClass<ULibretroCoreInstance>();
	check(LibretroCoreInstance);
}

template<unsigned RetroButton>
void ULibretroInputComponent::ButtonPressed() {
	LibretroCoreInstance->instance->g_joy[RetroButton] = true;
}

template<unsigned RetroButton>
void ULibretroInputComponent::ButtonReleased() {
	LibretroCoreInstance->instance->g_joy[RetroButton] = false;
}