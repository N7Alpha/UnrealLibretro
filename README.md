

# UnrealLibretro

UnrealLibretro is a Libretro Frontend for Unreal Engine. Basically this just lets you run emulators within Unreal Engine. Technically it allows you to run [Libretro Cores](https://docs.libretro.com/meta/core-list/).

## Libretro Core Compatibility

Unfortunately the full Libretro API is quite complex so I have only partially implemented it.

### Known working cores

* snes9x
* mupen64plus_next
* PPSSPP

### Known broken cores

* Dolphin

## Platform Compatibility

For now UnrealLibretro only works for Windows. Most of the libraries I use are cross platform so it shouldn't be too hard to get it working on other platforms. If you want to fix compatibility with other platforms contributions are welcome.

## Installing in your Project

### Simple method
Download the [latest release](https://github.com/N7Alpha/UnrealLibretro/releases/latest)
Extract the archive and place it in your project's plugins folder.

### Using git

Clone the repo into your Unreal Engine project's Plugin folder.

Open a MinGW64 terminal and navigate to the root directory of this plugin then run this command
```
./setup.sh
```
Then in the Windows file explorer navigate to the root directory of your project and right click the .uproject file then select "Generate Visual Studio project files" in the context menu.

## Integrating into your Project

### Download a Libretro Core
You can download a Libretro Core from [here](https://buildbot.libretro.com/nightly/windows/x86_64/latest/) and place it into the MyCores folder.

### Download a ROM
You know and I know you know where to get these. Once you have the one you want place it into your MyROMs folder.

### (Sometimes required) Download content folder
Some cores require that you also provide a content folder. PPSSPP is [one example](https://docs.libretro.com/library/ppsspp/#bios). Mainly this just involves taking a folder from a release of the emulator and moving it into the ```UnrealLibretro/system``` directory. There might be weirder ones. You can probably just find them by googling or searching the [Libretro docs](https://docs.libretro.com/).

### In the Unreal Editor
Restart your project if you performed the setup while the Unreal Editor was running.
Navigate to UnrealLibretro's content folder in the Unreal Editor content browser, and open the example map LibretroWorld. Exploring the objects in this folder should give you an idea of how to use the API.

## Contributing

Try to follow the Unreal Engine coding standards at least in the Unreal based source files. Mainly what needs to be worked on is Libretro core compatibility and probably fleshing out ```ULibretroCoreInstance```. I'm mainly just developing this for a project I'm working on myself, so there also might be some oversights in the API that should be rectified.

## Debugging Libretro Cores
Debugging libretro cores is kind of a nightmare since a lot of times your game will crash inside the cores code and if you don't have symbols good luck figuring that out. Luckily this [tutorial](https://docs.libretro.com/development/retroarch/compilation/windows/) mostly explains how to get symbolized debugging working. Note: You absolutely must use the MinGW64 terminal not the MSYS2 terminal when building Libretro cores for Windows. Also don't forget to enable the debug flags they mention when building with ```make```.

The one thing that isn't mentioned is that once you successfully build the DLL you have to use [cv2pdb](https://github.com/rainers/cv2pdb/releases/latest) to make a Windows debug symbol database file (pdb).

You produce the pdb by running something like this in MinGW64

```
cv2pdb.exe {core_name}.exe
```

The pdb is produced in the same directory as the DLL. Copy both of them over to MyCores. Once that's done Visual Studio should automagically load the pdb  and will ask you to point to the source files. And now you might get more insight into whats going wrong... hopefully.

### Random notes
Q: Visual Studio loads the source files for the wrong core when debugging 

A: Remove other pdbs AND source files and clean the build and rerun

## Contact

You should [post an issue](https://github.com/N7Alpha/UnrealLibretro/issues) if you have a problem or discover a bug. If you have general questions about the project or want to make a big contribution and you need to talk through the project structure with me you can email me at rehbein@cock.li

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details
