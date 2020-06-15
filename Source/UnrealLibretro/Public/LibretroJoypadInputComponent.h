// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "LibretroInputComponent.h"

#include "LibretroJoypadInputComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class UNREALLIBRETRO_API ULibretroJoypadInputComponent : public ULibretroInputComponent
{
	GENERATED_BODY()

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	template<unsigned RetroAxis, unsigned RetroDirection>
	void AxisChanged(float Value);
		
};
