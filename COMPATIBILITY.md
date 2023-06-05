
Unfortunately the full Libretro API is quite complex so I have only partially implemented it, so some cores might not work correctly. A non-exhaustive list is documented here.

# Platform Compatibility

# Windows
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
* `dosbox_pure     ` (This one fails when loading the content)

# Android
I know for certain `gearboy` and `mupen64plus_next` work so I'd try testing those first. I'll probably try to set up automated regression tests in the future so a list can be automatically maintained.

# How to run the right cores for the right platform
You can always manually give a path to a core if needed however the recommended way is to store them in the same way as the `UnrealLibretroEditor` module does when it downloads them. These paths are used when packaging a project and `UnrealLibretro` uses them to load cores in a platform agnostic way.

Here's how the plugin directory might look after downloading a couple cores.
```
📦UnrealLibretro
 ┣ 📂MyCores
 ┃ ┣ 📂Android
 ┃ ┃ ┣ 📂arm64-v8a
 ┃ ┃ ┃ ┣ 📜gearboy_libretro_android.so
 ┃ ┃ ┃ ┗ 📜mupen64plus_next_gles3_libretro_android.so
 ┃ ┃ ┗ 📂armeabi-v7a
 ┃ ┃   ┣ 📜gearboy_libretro_android.so
 ┃ ┃   ┗ 📜mupen64plus_next_gles3_libretro_android.so
 ┃ ┗ 📂Win64
 ┃   ┣ 📜gearboy_libretro.dll
 ┃   ┗ 📜mupen64plus_next_libretro.dll
 ┃
 ┗ 📂MyROMs
   ┣ 📜baserom.us.z64
   ┗ 📜Legend of Zelda, The - Link's Awakening DX (USA, Europe) (SGB Enhanced).gbc
 ```
- Cores are organized hierarchically in the same convention Unreal Engine uses for binaries
- Notice how `gearboy_libretro.dll` and `gearboy_libretro_android.so` only differ by a `.dll` and `_android.so`. This is actually a standard naming format used for cores. So I've written the code so `ULibretroCoreInstance::CorePath` can just be set to `gearboy` and the correct core will be chosen for the correct platform
- Unfortunately some cores like `mupen64plus_next_libretro.dll` and `mupen64plus_next_gles3_libretro_android.so` differ by more than just the standard format so they can't be directly associated. So you'll have to come up with your own way to load these correctly for the correct platform

[1]: README.md#sometimes-required-download-content-folder