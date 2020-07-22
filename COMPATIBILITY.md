## Core Compatibility

Unfortunately the full Libretro API is quite complex so I have only partially implemented it.

### Known working cores

* nestopia
* gearboy
* snes9x
* mupen64plus_next
* PPSSPP

### Known broken cores

* dolphin
* sameboy

## Platform Compatibility

For now UnrealLibretro only works for Windows. Most of the external libraries I use are cross platform so it shouldn't be too hard to get it working on other platforms. I'm using SDL2 to handle obtaining OpenGL contexts and windows (necessary for OpenGL contexts) in a cross platform way. I know theres some platform specific quirks I'm missing since I haven't tested it. For example, I obtain the window on a background thread, but on MacOS this will only work on the main thread and in Linux this will only work on a background thread if you set some special flag. If you want to fix compatibility with other platforms contributions are welcome.