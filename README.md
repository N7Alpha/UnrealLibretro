# UnrealLibretro

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Discord](https://img.shields.io/discord/810332877577125899?label=Discord&logo=discord)](https://discord.gg/nSTy2jyJmh)
[![CI](https://github.com/N7Alpha/UnrealLibretro/actions/workflows/main.yml/badge.svg)](https://github.com/N7Alpha/UnrealLibretro/actions/workflows/main.yml)
[![from](https://img.shields.io/badge/dynamic/yaml?url=https://raw.githubusercontent.com/N7Alpha/UnrealLibretro/master/.github/workflows/main.yml&query=%24.jobs.build.strategy.matrix.version%5B%3A1%5D.minor&prefix=4.&label=engine&color=black)](https://github.com/N7Alpha/UnrealLibretro/releases/latest)
[![to](https://img.shields.io/badge/dynamic/yaml?url=https://raw.githubusercontent.com/N7Alpha/UnrealLibretro/master/.github/workflows/main.yml&query=%24.jobs.build.strategy.matrix.version%5B-1%3A%5D.minor&prefix=5.&label=-&color=black&labelColor=black)](https://github.com/N7Alpha/UnrealLibretro/releases/latest)

UnrealLibretro is a Libretro Frontend for Unreal Engine. It is a Blueprint compatible library that lets you run emulators within Unreal Engine. More Technically it allows you to run [Libretro Cores](https://docs.libretro.com/meta/core-list/).

## Compatibility

Information about platform and Libretro Core compatibility can be found [here](COMPATIBILITY.md).

## Installing in your Project

### Simple method

Download the [latest release](https://github.com/N7Alpha/UnrealLibretro/releases/latest)
Extract the archive and place it in your project's plugins folder.

### Using git

Clone the repo into your Unreal Engine project's Plugin folder.

Run the associated `setup` script for your current platform. If Windows double-click the `setup.cmd`

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
- Click on an actor and in the `LibretroCoreInstance` actor component set the Libretro Core first then the ROM

## Contributing

Mainly what needs to be worked on is Libretro Core compatibility and probably fleshing out `ULibretroCoreInstance` to incorporate more of the API `libretro.h` exposes. More information about contributing can be found [here](CONTRIBUTING.md).

## Contact

You should [post an issue](https://github.com/N7Alpha/UnrealLibretro/issues) if you have a problem or discover a bug. If that is too intimidating or for more basic troubleshooting you can post in the [Discord](https://discord.gg/nSTy2jyJmh).

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details
