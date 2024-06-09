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
#if 0
// This doesn't quite work with packaging on 4.24 I think it's probably caused by other source files in the
// unity-build including the header prior to my definition of NETIMGUI_IMPLEMENTATION here. If you macro-guard
// the implementation and definitions separately then this issue would not exist. I may at some point make
//  a PR in netImgui to fix this @todo
#define NETIMGUI_IMPLEMENTATION
#include "NetImgui_Api.h"
#else
#define NETIMGUI_INTERNAL_INCLUDE
#include "NetImgui_Config.h"
#undef NETIMGUI_INTERNAL_INCLUDE
#include "../../ThirdParty/netImgui/Code/Client/Private/NetImgui_Api.cpp"
#include "../../ThirdParty/netImgui/Code/Client/Private/NetImgui_Client.cpp"
#include "../../ThirdParty/netImgui/Code/Client/Private/NetImgui_CmdPackets_DrawFrame.cpp"
#include "../../ThirdParty/netImgui/Code/Client/Private/NetImgui_NetworkUE4.cpp"
#endif
#endif
