

# UnrealLibretro

[Discord](https://discord.gg/nSTy2jyJmh)

UnrealLibretro is a Libretro Frontend for Unreal Engine. It is a Blueprint compatible library that lets you run emulators within Unreal Engine. More Technically it allows you to run [Libretro Cores](https://docs.libretro.com/meta/core-list/).

## Compatibility

Information about platform and Libretro Core compatibility can be found [here](COMPATIBILITY.md).

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
You can download Libretro Cores directly from the Unreal Editor. If that isn't working try accessing downloading them from the [buildbot](https://buildbot.libretro.com/nightly/windows/x86_64/latest/)

### Download a ROM
You know and I know you know where to get these. Once you have the one you want place it into your MyROMs folder.

### (Sometimes required) Download content folder
Some cores require that you also provide a content folder. PPSSPP is [one example](https://docs.libretro.com/library/ppsspp/#bios). Mainly this just involves taking a folder from a release of the emulator and moving it into the `UnrealLibretro/System` directory. There might be weirder ones. You can probably just find them by googling or searching the [Libretro docs](https://docs.libretro.com/).

### In the Unreal Editor
- Restart your project if you performed the setup process while the Unreal Editor was running
- Enable "Show Plugin Content" from the Content Browser options
- Navigate to UnrealLibretro's content folder in the Unreal Editor Content Browser, and open the example map LibretroMap
- Click on the actors to set their Cores and ROMs then hit play

## Contributing

Try to follow the Unreal Engine coding standards at least in the Unreal based source files. Mainly what needs to be worked on is Libretro core compatibility and probably fleshing out ```ULibretroCoreInstance``` to incorporate more of the API ```libretro.h``` exposes. I'm mainly just developing this for a project I'm working on myself, so there also might be some oversights in the API that should be rectified. More information about contributing can be found [here](CONTRIBUTING.md).

## Contact

You should [post an issue](https://github.com/N7Alpha/UnrealLibretro/issues) if you have a problem or discover a bug. If you have general questions about the project or want to make a big contribution and you need to talk through the project structure with me you can email me at rehbein@cock.li

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details
