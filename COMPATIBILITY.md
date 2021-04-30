# Core Compatibility

Unfortunately the full Libretro API is quite complex so I have only partially implemented it, so some cores might not work correctly. A non-exhaustive list is documented here.

## Known working cores

* `nestopia        `
* `gearboy         `
* `snes9x          `
* `mupen64plus_next`
* `PPSSPP          ` (Requires [intallation][1] of additional [support files](https://docs.libretro.com/library/ppsspp/#bios))
* `fbalpha2012     `
* `vbam            `
* `desmume2015     `
* `mame            `
* `dosbox_svn      `

## Known broken cores

* `dolphin         `
* `sameboy         `
* `dosbox_core     ` (This one calls a callback from another thread which I think is a bug on their end)
* `dosbox_pure     ` (This one fails when loading the content)

# Platform Compatibility

For now UnrealLibretro only works for Windows. Most of the external libraries I use are cross platform so it shouldn't be too hard to get it working on other platforms. I'm using SDL2 to handle obtaining OpenGL contexts and windows (necessary for OpenGL contexts) in a cross platform way. I know theres some platform specific quirks I'm missing since I haven't tested it. For example, I obtain the window on a background thread, but on MacOS this will only work on the main thread and in Linux this will only work on a background thread if you set some special flag. If you want to fix compatibility with other platforms contributions are welcome.

[1]: README.md#sometimes-required-download-content-folder