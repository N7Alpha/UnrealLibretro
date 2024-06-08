#pragma once

#include "libretro/libretro.h"


// By convention RETRO_DEVICE_JOYPAD is the default controller although often it represents something different compared to
// the libretro.h documentation of RETRO_DEVICE_JOYPAD. Basically it will be the most reasonable controller for the core
#define RETRO_DEVICE_DEFAULT RETRO_DEVICE_JOYPAD


#include "InputCoreTypes.h"
#include "CoreMinimal.h"

#include <type_traits> // One of the few std headers where its usage is recommended over a native Unreal Engine implementation see https://docs.unrealengine.com/5.0/en-US/epic-cplusplus-coding-standard-for-unreal-engine/#useofstandardlibraries

#include "LibretroInputDefinitions.generated.h"

USTRUCT(BlueprintType)
struct FLibretroOptionDescription
{
    GENERATED_BODY()

    static constexpr int DefaultOptionIndex = 0; // By libretro convention

    UPROPERTY(BlueprintReadOnly, Category = "Libretro")
    FString         Key;

    UPROPERTY(BlueprintReadOnly, Category = "Libretro")
    FString         Description;

    UPROPERTY(BlueprintReadOnly, Category = "Libretro")
    TArray<FString> Values;
};

USTRUCT(BlueprintType)
struct FLibretroControllerDescription // retro_controller_description
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, Category = "Libretro")
    FString Description{"Unspecified"};

    UPROPERTY(VisibleAnywhere, Category = "Libretro")
    unsigned int ID{ RETRO_DEVICE_DEFAULT };
};

// Check libretro.h for further documentation. More or less this maps to RETRO_DEVICE_ID values in there
// DO NOT REORDER THESE... doing so will break indexing badly....
//      The order for each RETRO_DEVICE is in the same numerical increasing order as every RETRO_DEVICE_*_ID value 
UENUM(BlueprintType)
enum class ERetroDeviceID : uint8
{
    // RETRO_DEVICE_ID_JOYPAD
    JoypadB,
    JoypadY,
    JoypadSelect,
    JoypadStart,
    JoypadUp,
    JoypadDown,
    JoypadLeft,
    JoypadRight,
    JoypadA,
    JoypadX,
    JoypadL,
    JoypadR,
    JoypadL2,
    JoypadR2,
    JoypadL3,
    JoypadR3,

    // RETRO_DEVICE_ID_LIGHTGUN
    LightgunX UMETA(Hidden), // The Lightgun entries marked UMETA(Hidden) here are deprecated according to libretro.h
    LightgunY UMETA(Hidden),
    LightgunTrigger,
    LightgunAuxA,
    LightgunAuxB,
    LightgunPause UMETA(Hidden),
    LightgunStart,
    LightgunSelect,
    LightgunAuxC,
    LightgunDpadUp,
    LightgunDpadDown,
    LightgunDpadLeft,
    LightgunDpadRight,
    LightgunScreenX,
    LightgunScreenY,
    LightgunIsOffscreen,
    LightgunReload,

    // RETRO_DEVICE_ID_ANALOG                                       (For triggers)
    // CartesianProduct(RETRO_DEVICE_ID_ANALOG, RETRO_DEVICE_INDEX) (For stick input)
    AnalogLeftX,
    AnalogLeftY,
    AnalogRightX,
    AnalogRightY,
    AnalogL2,
    AnalogR2,

    // RETRO_DEVICE_ID_POINTER
    PointerX,
    PointerY,
    PointerPressed,
    PointerCount,
    PointerX1         UMETA(Hidden),
    PointerY1         UMETA(Hidden),
    PointerPressed1   UMETA(Hidden),
    PointerCountVoid1 UMETA(Hidden),
    PointerX2         UMETA(Hidden),
    PointerY2         UMETA(Hidden),
    PointerPressed2   UMETA(Hidden),
    PointerCountVoid2 UMETA(Hidden),
    PointerX3         UMETA(Hidden),
    PointerY3         UMETA(Hidden),
    PointerPressed3   UMETA(Hidden),

    Size UMETA(Hidden),
};

// std alchemy that converts enum class variables to their integer type
template<typename E>
static constexpr auto to_integral(E e) -> typename std::underlying_type<E>::type
{
    return static_cast<typename std::underlying_type<E>::type>(e);
}

constexpr int PortCount = 8;

struct FLibretroInputState
{
    int16_t Data[64]{0}; // @todo I should get the type from ulnet.h so the relationship to that file is clearer instead of having an arbitrary 64 here I want that to be a private include though ideally

    FLibretroInputState() {}

    int16_t& operator[](unsigned Index)               { return Data[Index]; }
    int16_t& operator[](ERetroDeviceID RetroDeviceID) { return Data[to_integral(RetroDeviceID)]; }
};

static_assert(std::is_trivially_copyable<FLibretroInputState>::value, "FLibretroInputState should be trivially copyable");
static_assert(std::is_standard_layout<FLibretroInputState>::value, "FLibretroInputState should have a standard layout");
static_assert(sizeof(FLibretroInputState) == sizeof(int16_t) * std::extent<decltype(FLibretroInputState::Data)>::value, "FLibretroInputState size should match the size of the Data array");
static_assert(alignof(FLibretroInputState) == alignof(int16_t), "FLibretroInputState alignment should match the alignment of int16_t");

static_assert(static_cast<size_t>(ERetroDeviceID::Size) < sizeof(FLibretroInputState::Data) / sizeof(std::remove_reference_t<decltype(std::declval<FLibretroInputState>().Data[0])>),
              "ERetroDeviceID::Size must be less than the number of elements in FLibretroInputState::Data");

// const for a reason if you make this non-const make sure you don't cause a data race also make it non static too
static const struct { FKey Unreal; retro_key libretro; } key_bindings[] = {
    { EKeys::BackSpace,        RETROK_BACKSPACE    },
    { EKeys::Tab,              RETROK_TAB          },
    { EKeys::Escape,           RETROK_CLEAR        },
    { EKeys::Enter,            RETROK_RETURN       },
    { EKeys::Pause,            RETROK_PAUSE        },
    { EKeys::Escape,           RETROK_ESCAPE       },
    { EKeys::SpaceBar,         RETROK_SPACE        },
    { EKeys::Exclamation,      RETROK_EXCLAIM      },
    { EKeys::Quote,            RETROK_QUOTEDBL     },
//  {                          RETROK_HASH         },
    { EKeys::Dollar,           RETROK_DOLLAR       },
    { EKeys::Ampersand,        RETROK_AMPERSAND    },
    { EKeys::Apostrophe,       RETROK_QUOTE        },
    { EKeys::LeftParantheses,  RETROK_LEFTPAREN    },
    { EKeys::RightParantheses, RETROK_RIGHTPAREN   },
    { EKeys::Asterix,          RETROK_ASTERISK     },
    { EKeys::Add,              RETROK_PLUS         },
    { EKeys::Comma,            RETROK_COMMA        },
    { EKeys::Subtract,         RETROK_MINUS        },
    { EKeys::Period,           RETROK_PERIOD       },
    { EKeys::Slash,            RETROK_SLASH        },
    { EKeys::Zero,             RETROK_0            },
    { EKeys::One,              RETROK_1            },
    { EKeys::Two,              RETROK_2            },
    { EKeys::Three,            RETROK_3            },
    { EKeys::Four,             RETROK_4            },
    { EKeys::Five,             RETROK_5            },
    { EKeys::Six,              RETROK_6            },
    { EKeys::Seven,            RETROK_7            },
    { EKeys::Eight,            RETROK_8            },
    { EKeys::Nine,             RETROK_9            },
    { EKeys::Colon,            RETROK_COLON        },
    { EKeys::Semicolon,        RETROK_SEMICOLON    },
//  {                          RETROK_LESS         },
//  {                          RETROK_EQUALS       },
//  {                          RETROK_GREATER      },
//  {                          RETROK_QUESTION     },
//  {                          RETROK_AT           },
    { EKeys::LeftBracket,      RETROK_LEFTBRACKET  },
    { EKeys::Backslash,        RETROK_BACKSLASH    },
    { EKeys::RightBracket,     RETROK_RIGHTBRACKET },
    { EKeys::Caret,            RETROK_CARET        },
    { EKeys::Underscore,       RETROK_UNDERSCORE   },
    { EKeys::Tilde,            RETROK_BACKQUOTE    }, //@?
    { EKeys::A,                RETROK_a            },
    { EKeys::B,                RETROK_b            },
    { EKeys::C,                RETROK_c            },
    { EKeys::D,                RETROK_d            },
    { EKeys::E,                RETROK_e            },
    { EKeys::F,                RETROK_f            },
    { EKeys::G,                RETROK_g            },
    { EKeys::H,                RETROK_h            },
    { EKeys::I,                RETROK_i            },
    { EKeys::J,                RETROK_j            },
    { EKeys::K,                RETROK_k            },
    { EKeys::L,                RETROK_l            },
    { EKeys::M,                RETROK_m            },
    { EKeys::N,                RETROK_n            },
    { EKeys::O,                RETROK_o            },
    { EKeys::P,                RETROK_p            },
    { EKeys::Q,                RETROK_q            },
    { EKeys::R,                RETROK_r            },
    { EKeys::S,                RETROK_s            },
    { EKeys::T,                RETROK_t            },
    { EKeys::U,                RETROK_u            },
    { EKeys::V,                RETROK_v            },
    { EKeys::W,                RETROK_w            },
    { EKeys::X,                RETROK_x            },
    { EKeys::Y,                RETROK_y            },
    { EKeys::Z,                RETROK_z            },
//  {                          RETROK_LEFTBRACE    },
//  {                          RETROK_BAR          },
//  {                          RETROK_RIGHTBRACE   },
//  {                          RETROK_TILDE        },
    { EKeys::Delete,           RETROK_DELETE       },

    { EKeys::NumPadZero,       RETROK_KP0          },
    { EKeys::NumPadOne,        RETROK_KP1          },
    { EKeys::NumPadTwo,        RETROK_KP2          },
    { EKeys::NumPadThree,      RETROK_KP3          },
    { EKeys::NumPadFour,       RETROK_KP4          },
    { EKeys::NumPadFive,       RETROK_KP5          },
    { EKeys::NumPadSix,        RETROK_KP6          },
    { EKeys::NumPadSeven,      RETROK_KP7          },
    { EKeys::NumPadEight,      RETROK_KP8          },
    { EKeys::NumPadNine,       RETROK_KP9          },
    { EKeys::Decimal,          RETROK_KP_PERIOD    },
    { EKeys::Divide,           RETROK_KP_DIVIDE    },
    { EKeys::Multiply,         RETROK_KP_MULTIPLY  },
    { EKeys::Subtract,         RETROK_KP_MINUS     },
    { EKeys::Add,              RETROK_KP_PLUS      },
//  {                          RETROK_KP_ENTER     },
//  {                          RETROK_KP_EQUALS    },

    { EKeys::Up,               RETROK_UP           },
    { EKeys::Down,             RETROK_DOWN         },
    { EKeys::Right,            RETROK_RIGHT        },
    { EKeys::Left,             RETROK_LEFT         },
//  {                          RETROK_INSERT       },
//  {                          RETROK_HOME         },
//  {                          RETROK_END          },
//  {                          RETROK_PAGEUP       },
//  {                          RETROK_PAGEDOWN     },
//  {                                              },
    { EKeys::F1,               RETROK_F1           },
    { EKeys::F2,               RETROK_F2           },
    { EKeys::F3,               RETROK_F3           },
    { EKeys::F4,               RETROK_F4           },
    { EKeys::F5,               RETROK_F5           },
    { EKeys::F6,               RETROK_F6           },
    { EKeys::F7,               RETROK_F7           },
    { EKeys::F8,               RETROK_F8           },
    { EKeys::F9,               RETROK_F9           },
    { EKeys::F10,              RETROK_F10          },
    { EKeys::F11,              RETROK_F11          },
    { EKeys::F12,              RETROK_F12          },
//  {                          RETROK_F13          },
//  {                          RETROK_F14          },
//  {                          RETROK_F15          },

    { EKeys::NumLock,          RETROK_NUMLOCK      },
    { EKeys::CapsLock,         RETROK_CAPSLOCK     },
    { EKeys::ScrollLock,       RETROK_SCROLLOCK    },

    { EKeys::RightShift,       RETROK_RSHIFT       },
    { EKeys::LeftShift,        RETROK_LSHIFT       },
    { EKeys::RightControl,     RETROK_RCTRL        },
    { EKeys::LeftControl,      RETROK_LCTRL        },
    { EKeys::RightAlt,         RETROK_RALT         },
    { EKeys::LeftAlt,          RETROK_LALT         },
//  {                          RETROK_RMETA        },
//  {                          RETROK_LMETA        },
    { EKeys::LeftCommand,      RETROK_LSUPER       }, //@?
    { EKeys::RightCommand,     RETROK_RSUPER       }, //@?
//  {                          RETROK_MODE         },
//  {                          RETROK_COMPOSE      },

//  {                          RETROK_HELP         },
//  {                          RETROK_PRINT        },
//  {                          RETROK_SYSREQ       },
//  {                          RETROK_BREAK        },
//  {                          RETROK_MENU         },
//  {                          RETROK_POWER        },
//  {                          RETROK_EURO         },
//  {                          RETROK_UNDO         },
//  {                          RETROK_OEM_102      },
};

static const int count_key_bindings = sizeof(key_bindings) / sizeof(key_bindings[0]);