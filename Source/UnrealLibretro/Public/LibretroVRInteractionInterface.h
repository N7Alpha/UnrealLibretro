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
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void TriggerAxis(float Val);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void TriggerPressed();

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void TriggerReleased();


	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void SecondaryTriggerAxis(float Val);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void SecondaryTriggerPressed();

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void SecondaryTriggerReleased();
};
