#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "LibretroVRInteractionInterface.generated.h"

// UE Reflection boilerplate
UINTERFACE(BlueprintType, MinimalAPI)
class ULibretroVRInteractionInterface : public UInterface
{
	GENERATED_BODY()
};


class UNREALLIBRETRO_API ILibretroVRInteractionInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Libretro")
	void TriggerAxis(float Val);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Libretro")
	void TriggerPressed();

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Libretro")
	void TriggerReleased();


	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Libretro")
	void SecondaryTriggerAxis(float Val);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Libretro")
	void SecondaryTriggerPressed();

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Libretro")
	void SecondaryTriggerReleased();
};
