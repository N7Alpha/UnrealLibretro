# This file will download the required binaries, headers, and create user folders. After running this you can drag and drop the UnrealLibretro folder into your project's plugins folder
# and you should be able to load the plugin, and the example blueprint.
#!/bin/bash

#DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
mkdir -p MyROMs;
mkdir -p MyCores;

wget https://buildbot.libretro.com/nightly/windows/x86_64/redist.7z --directory /tmp;

mkdir -p libretro;

pacman -S --noconfirm --needed p7zip;
7z x -aoa -o./libretro /tmp/redist.7z;

wget https://www.libsdl.org/release/SDL2-devel-2.0.12-VC.zip --directory /tmp;
7z x -aoa -o/tmp /tmp/SDL2-devel-2.0.12-VC.zip
cp -a /tmp/SDL2-2.0.12/include/. ./Source/UnrealLibretro/Public/SDL2
cp -a /tmp/SDL2-2.0.12/lib/x64/. ./Binaries/Win64