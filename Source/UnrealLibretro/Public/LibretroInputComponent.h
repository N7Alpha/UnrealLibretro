// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/InputComponent.h"
#include "LibretroCoreInstance.h"

#include "LibretroInputComponent.generated.h"


#define BindUnrealButtonToLibretro(RetroButton, UnrealButton)			   BindKey    (UnrealButton, IE_Pressed,  this, &ULibretroJoypadInputComponent::ButtonPressed <RetroButton>); \
																		   BindKey    (UnrealButton, IE_Released, this, &ULibretroJoypadInputComponent::ButtonReleased<RetroButton>);

UCLASS(Abstract, ClassGroup=(Custom), hidecategories = (Activation, "Components|Activation"))
class UNREALLIBRETRO_API ULibretroInputComponent : public UInputComponent
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	ULibretroCoreInstance* LibretroCoreInstance;


	// Called when the game starts
	virtual void BeginPlay() override;	

public: // @todo: make this a friend function of whoever calls it
	template<unsigned RetroButton>
	void ButtonPressed();

	template<unsigned RetroButton>
	void ButtonReleased();
};
