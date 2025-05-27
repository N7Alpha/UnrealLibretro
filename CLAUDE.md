# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

UnrealLibretro is a Libretro frontend for Unreal Engine that enables running emulators (Libretro cores) within UE applications. It provides Blueprint-compatible functionality with basic VR support and networking

## Setup Commands

```bash
# Initial setup (run once after cloning)
./setup.sh          # Linux/Mac
setup.cmd           # Windows

# Generate UE project files after setup
# Right-click .uproject file → "Generate Visual Studio project files"
```

The setup script downloads RetroArch binaries, creates directory structure (MyROMs, MyCores, etc.), and configures the appropriate .uplugin file for the detected UE version.

## Build System

### Unreal Engine Integration
- **Main module**: `Source/UnrealLibretro/UnrealLibretro.Build.cs` - Runtime module with conditional compilation for UE 4.24 through 5.5+
- **Editor module**: `Source/UnrealLibretroEditor/UnrealLibretroEditor.Build.cs` - Editor-only functionality
- **Third-party module**: `Source/ThirdParty/LibretroThirdParty.Build.cs` - External libraries

### Standalone CMake Build
```bash
# Build standalone netplay tools
mkdir build && cd build
cmake ..
make netarch
```

Used for debugging and standalone netplay functionality without UE dependencies. **`ulnet.h`,`sam2.h`,`netarch.cpp`** make up the netplay implementation

## Core Architecture

### Primary Components
- **`ULibretroCoreInstance`**: Main actor component managing individual emulator instances
- **`FLibretroContext`**: Basically this the part `ULibretroCoreInstance` that runs on a background thread most of the state, but not all is owned by the background thread
- **`ULibretroBlueprintFunctionLibrary`**: Blueprint utility functions
- **VR Demo**: This was ported from UE 5.0 and works on 4.24 and beyond
- **ulnet.h**: Custom netplay protocol using libjuice for ICE/STUN

### Multi-Instance Architecture
- The plugin supports running multiple emulator instances simultaneously through macro-generated function tables (see `FUNC_WRAP_*` macros) and `libretro_callbacks_table`

### Threading Model
- Emulator cores each run on a dedicated background thread in (`FLibretroContext`)
- Game thread serves the user friendly api and handles input
- LibretroContext thread integrates with UE's RHI thread, Render thread, and Audio thread
- Game thread (`FLibretroCoreInstance`) sends tasks through per-instance asynchronous task queues for executing libretro api commands on the libretro thread (`FLibretroContext`)
- OpenGL context sharing is enabled for old versions of OpenGL where rendering happens on a separate thread from the libretro thread (basically dolphin emulator is the only one that needs this). Newer OpenGL API's allow you to forgo the `glBind*` calls entirely which means shared contexts aren't needed

## Key Development Patterns

### Blueprint Integration
The main user interface for the UnrealLibretro plugin is exposed to Blueprint through `ULibretroCoreInstance` and `ULibretroBlueprintFunctionLibrary`. `ULibretroCoreInstance` provides an event-driven interface with delegates for core lifecycle events.

### Input System
- `ERetroDeviceID` represents an index into `LibretroContext::InputState`
- `LibretroContext::core_input_state` callback demonstrates the mapping between `FLibretroInputState` and `retro_input_state_t`

### Rendering Pipeline
- Hardware acceleration via OpenGL with RHI interop
- Software rendering fallback with BGRA conversion
- Shared texture memory optimization
- Multi-threaded rendering with proper synchronization

### Platform Support
- **Windows x64**: Full feature support including VR
- **Android**: ARM/ARM64 with architecture-specific core loading
- **Planned**: Linux, Mac, other mobile platforms

## Development Guidelines

## Coding Standard

- No tabs except for `.cs` files
- No trailing whitespace
- No C++ exceptions
- Acceptable stdlib headers <atomic>, <type_traits>, <initializer_list>, <regex>, <limits>, <cmath>, <cstring>
- Depend on as few internal Unreal Engine API's as reasonable to maintain compatibility across many engine versions

## Configuration Files

- **`Config/DefaultUnrealLibretro.ini`**: Core plugin settings
- **`Config/Input.ini`**: VR and traditional input mappings
- **`Config/FilterPlugin.ini`**: Platform-specific plugin filtering
- **Version-specific .uplugin files**: UE compatibility (automatically selected by setup script)

## Network Architecture

The netplay system implements:
- P2P and client-server topologies
- Bespoke transmission strategies utilizing redundancy, reliable UDP, and FEC in `ulnet.h` prioritizing low latency and reliability

## Testing and Quality

### CI Pipeline
- UE version build compatibility testing (4.24 through 5.5+)

## File Structure Notes

- **`MyCores/`**: Platform-specific libretro core binaries
- **`MyROMs/`**: Game ROM files (not distributed)
- **`System/`**: Core-specific system files and BIOS
- **`Content/`**: UE assets including example blueprints and materials
- **`LibretroThirdPartyImplementation.c/.cpp`** Unity build file for thirdparty dependencies
- **`libretro.h`** Libretro API