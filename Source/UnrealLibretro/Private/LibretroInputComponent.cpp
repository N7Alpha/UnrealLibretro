#include "LibretroInputComponent.h"
#include "LibretroCoreInstance.h"

#include "libretro/libretro.h"

void ULibretroInputComponent::Initialize(FLibretroInputState* InputState, TFunction<void()> Disconnect)
{
	InputStatePort = InputState;
	DisconnectPort = Disconnect;
}

template<unsigned RetroButton>
void ULibretroInputComponent::ButtonPressed()
{
	InputStatePort->digital[RetroButton].store(true, std::memory_order_relaxed);
}

template<unsigned RetroButton>
void ULibretroInputComponent::ButtonReleased()
{
	InputStatePort->digital[RetroButton].store(false, std::memory_order_relaxed);
}

template<unsigned RetroAxis, unsigned RetroStick>
void ULibretroInputComponent::AxisChanged(float Value)
{
	float coff = RetroAxis == RETRO_DEVICE_ID_ANALOG_Y && RetroStick == RETRO_DEVICE_INDEX_ANALOG_LEFT ? -1 : 1; // Both Y-Axes should be inverted because of Libretro convention however Unreal has a quirk where the Y-Axis of the right stick is inverted by default for some reason
	InputStatePort->analog[RetroAxis][RetroStick].store((int16_t)FMath::RoundHalfToEven(coff * 0x7FFF * Value), std::memory_order_relaxed); // Some cores support 0x7FFF to -0x7FFF others to -0x8000. However I support only 0x7FFF to -0x7FFF
}

void ULibretroInputComponent::DisconnectController()
{
	DisconnectPort();
}

TArray<void (ULibretroInputComponent::*)(), TFixedAllocator<(uint32)ERetroInput::DigitalCount>> ULibretroInputComponent::ButtonReleasedFunctions = 
{
		&ULibretroInputComponent::ButtonReleased <RETRO_DEVICE_ID_JOYPAD_B     >,
		&ULibretroInputComponent::ButtonReleased <RETRO_DEVICE_ID_JOYPAD_Y	   >,
		&ULibretroInputComponent::ButtonReleased <RETRO_DEVICE_ID_JOYPAD_SELECT>,
		&ULibretroInputComponent::ButtonReleased <RETRO_DEVICE_ID_JOYPAD_START >,
		&ULibretroInputComponent::ButtonReleased <RETRO_DEVICE_ID_JOYPAD_UP	   >,
		&ULibretroInputComponent::ButtonReleased <RETRO_DEVICE_ID_JOYPAD_DOWN  >,
		&ULibretroInputComponent::ButtonReleased <RETRO_DEVICE_ID_JOYPAD_LEFT  >,
		&ULibretroInputComponent::ButtonReleased <RETRO_DEVICE_ID_JOYPAD_RIGHT >,
		&ULibretroInputComponent::ButtonReleased <RETRO_DEVICE_ID_JOYPAD_A	   >,
		&ULibretroInputComponent::ButtonReleased <RETRO_DEVICE_ID_JOYPAD_X	   >,
		&ULibretroInputComponent::ButtonReleased <RETRO_DEVICE_ID_JOYPAD_L	   >,
		&ULibretroInputComponent::ButtonReleased <RETRO_DEVICE_ID_JOYPAD_R	   >,
		&ULibretroInputComponent::ButtonReleased <RETRO_DEVICE_ID_JOYPAD_L2	   >,
		&ULibretroInputComponent::ButtonReleased <RETRO_DEVICE_ID_JOYPAD_R2	   >,
		&ULibretroInputComponent::ButtonReleased <RETRO_DEVICE_ID_JOYPAD_L3	   >,
		&ULibretroInputComponent::ButtonReleased <RETRO_DEVICE_ID_JOYPAD_R3    >
};

TArray<void (ULibretroInputComponent::*)(), TFixedAllocator<(uint32)ERetroInput::DigitalCount>> ULibretroInputComponent::ButtonPressedFunctions = 
{
		&ULibretroInputComponent::ButtonPressed <RETRO_DEVICE_ID_JOYPAD_B     >,
		&ULibretroInputComponent::ButtonPressed <RETRO_DEVICE_ID_JOYPAD_Y	  >,
		&ULibretroInputComponent::ButtonPressed <RETRO_DEVICE_ID_JOYPAD_SELECT>,
		&ULibretroInputComponent::ButtonPressed <RETRO_DEVICE_ID_JOYPAD_START >,
		&ULibretroInputComponent::ButtonPressed <RETRO_DEVICE_ID_JOYPAD_UP	  >,
		&ULibretroInputComponent::ButtonPressed <RETRO_DEVICE_ID_JOYPAD_DOWN  >,
		&ULibretroInputComponent::ButtonPressed <RETRO_DEVICE_ID_JOYPAD_LEFT  >,
		&ULibretroInputComponent::ButtonPressed <RETRO_DEVICE_ID_JOYPAD_RIGHT >,
		&ULibretroInputComponent::ButtonPressed <RETRO_DEVICE_ID_JOYPAD_A	  >,
		&ULibretroInputComponent::ButtonPressed <RETRO_DEVICE_ID_JOYPAD_X	  >,
		&ULibretroInputComponent::ButtonPressed <RETRO_DEVICE_ID_JOYPAD_L	  >,
		&ULibretroInputComponent::ButtonPressed <RETRO_DEVICE_ID_JOYPAD_R	  >,
		&ULibretroInputComponent::ButtonPressed <RETRO_DEVICE_ID_JOYPAD_L2	  >,
		&ULibretroInputComponent::ButtonPressed <RETRO_DEVICE_ID_JOYPAD_R2	  >,
		&ULibretroInputComponent::ButtonPressed <RETRO_DEVICE_ID_JOYPAD_L3	  >,
		&ULibretroInputComponent::ButtonPressed <RETRO_DEVICE_ID_JOYPAD_R3    >
};

TArray<void (ULibretroInputComponent::*)(float), TFixedAllocator<(uint32)ERetroInput::AnalogCount>>  ULibretroInputComponent::ButtonAnalog = 
{
		&ULibretroInputComponent::AxisChanged   <RETRO_DEVICE_ID_ANALOG_X, RETRO_DEVICE_INDEX_ANALOG_LEFT >,
		&ULibretroInputComponent::AxisChanged   <RETRO_DEVICE_ID_ANALOG_Y, RETRO_DEVICE_INDEX_ANALOG_LEFT >,
		&ULibretroInputComponent::AxisChanged   <RETRO_DEVICE_ID_ANALOG_X, RETRO_DEVICE_INDEX_ANALOG_RIGHT>,
		&ULibretroInputComponent::AxisChanged   <RETRO_DEVICE_ID_ANALOG_Y, RETRO_DEVICE_INDEX_ANALOG_RIGHT>
};

#include <type_traits>

template<typename E>
constexpr auto to_integral(E e) -> typename std::underlying_type<E>::type
{
	return static_cast<typename std::underlying_type<E>::type>(e);
}

void ULibretroInputComponent::BindKeys(const TMap<FKey, ERetroInput>& ControllerBindings)
{
	for (auto& kv : ControllerBindings) 
	{
		if (kv.Value != ERetroInput::DisconnectController)
		{
			if (kv.Key.IsFloatAxis())
			{
				BindAxisKey(kv.Key, this, ULibretroInputComponent::ButtonAnalog[to_integral(kv.Value) - to_integral(ERetroInput::LeftX)]);
			}
			else
			{
				BindKey(kv.Key, IE_Pressed, this, ULibretroInputComponent::ButtonPressedFunctions[to_integral(kv.Value)]);
				BindKey(kv.Key, IE_Released, this, ULibretroInputComponent::ButtonReleasedFunctions[to_integral(kv.Value)]);
			}
		}
		else 
		{
			BindKey(kv.Key, IE_Pressed, this, &ULibretroInputComponent::DisconnectController);
		}
	}
}