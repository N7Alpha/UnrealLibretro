#pragma once

#include "CoreMinimal.h"
#include "Components/InputComponent.h"
#include "LibretroInputDefinitions.h"

#include "LibretroInputComponent.generated.h"

UCLASS(ClassGroup=(Custom), hidecategories = (Activation, "Components|Activation"))
class UNREALLIBRETRO_API ULibretroInputComponent : public UInputComponent
{
	GENERATED_BODY()
public:
	void Initialize(FLibretroInputState* InputState, TFunction<void()> Disconnect);

	void BindKeys(const TMap<FKey, ERetroInput> &ControllerBindings);


	template<unsigned RetroButton>
	void ButtonPressed();

	template<unsigned RetroButton>
	void ButtonReleased();

	template<unsigned RetroStick, unsigned RetroAxis>
	void AxisChanged(float Value);

protected:
	
	static TArray<void (ULibretroInputComponent::*)(), TFixedAllocator<(uint32)ERetroInput::DigitalCount>>      ButtonPressedFunctions;

	static TArray<void (ULibretroInputComponent::*)(), TFixedAllocator<(uint32)ERetroInput::DigitalCount>>      ButtonReleasedFunctions;

	static TArray<void (ULibretroInputComponent::*)(float), TFixedAllocator<(uint32)ERetroInput::AnalogCount>>  ButtonAnalog;

	FLibretroInputState* InputStatePort;
	TFunction<void()> DisconnectPort;
	void DisconnectController();


};
