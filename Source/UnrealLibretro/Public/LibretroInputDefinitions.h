#pragma once

#include <atomic>

#include "libretro/libretro.h"

#include "CoreMinimal.h"

constexpr int PortCount = 4;
struct FLibretroInputState
{ // @todo should verify these call the zero init constructor
	TAtomic<unsigned> digital[RETRO_DEVICE_ID_JOYPAD_R3 + 1]{};
	TAtomic<int16_t>  analog[RETRO_DEVICE_INDEX_ANALOG_RIGHT + 1][RETRO_DEVICE_ID_ANALOG_Y + 1]{};
};

// DO NOT REORDER THESE
UENUM(BlueprintType)
enum  class ERetroInput : uint8
{
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