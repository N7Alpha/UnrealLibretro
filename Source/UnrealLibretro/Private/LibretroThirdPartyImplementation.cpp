#include "UnrealLibretro.h"

#include "imgui.cpp"
#include "imgui_draw.cpp"
#include "imgui_tables.cpp"
#include "imgui_widgets.cpp"

#include "implot.cpp"
#include "implot_items.cpp"

#if PLATFORM_WINDOWS
#include "Windows/MinWindows.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#endif


#if UNREALLIBRETRO_NETIMGUI
#define NETIMGUI_IMPLEMENTATION
#include "NetImgui_Api.h"
#endif
