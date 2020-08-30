# This file will download the required binaries, headers, and create user folders. After running this you can drag and drop the UnrealLibretro folder into your project's plugins folder
# and you should be able to load the plugin, and the example blueprint.
#!/bin/bash

#DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
mkdir -p MyROMs
mkdir -p MyCores
mkdir -p Saves/SaveStates Saves/SRAM
mkdir -p Binaries/Win64

# Download general binaries for running Libretro Cores
wget -c https://buildbot.libretro.com/nightly/windows/x86_64/redist.7z --directory /tmp

mkdir -p libretro

# Install p7zip for openning windows 7Zip files
pacman -S --noconfirm --needed p7zip
7z x -aoa -o./libretro /tmp/redist.7z

# Download SDL2 and install it. You could probably just use the one in the Libretro folder, but god knows I'm not changing it now
wget -c https://www.libsdl.org/release/SDL2-devel-2.0.12-VC.zip --directory /tmp
7z x -aoa -o/tmp /tmp/SDL2-devel-2.0.12-VC.zip
cp -a /tmp/SDL2-2.0.12/include/. ./Source/UnrealLibretro/Public/SDL2
cp -a /tmp/SDL2-2.0.12/lib/x64/. ./Binaries/Win64

# Aquire and move unversioned data from unversioned branch (Note this will break if you don't have a really new version of git)
git fetch origin unversioned:unversioned
git restore --source unversioned Content/