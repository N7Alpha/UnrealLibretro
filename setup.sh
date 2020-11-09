#!/bin/bash
# This file will setup the required binaries, headers, and create user folders. After running this you can drag and drop the UnrealLibretro folder into your project's plugins folder
# and you should be able to load the plugin, and the example blueprint.

# Prompt user to install 7zip if necessary
if ! command -v 7z
then
    echo "7zip is not installed"
    echo "Try one of these commands to install it:"
    printf "\tMINGW: pacman -S --noconfirm --needed p7zip\n"
    printf "\tMacOS: brew install p7zip\n"
    exit 127;
fi

# Make directories that won't be dynamically generated
mkdir -p MyROMs MyCores
mkdir -p Binaries/Win64/ThirdParty/libretro

# Get general binaries for running Libretro Cores on Windows
wget -c https://buildbot.libretro.com/nightly/windows/x86_64/redist.7z --directory /tmp
7z x -aoa -oBinaries/Win64/ThirdParty/libretro /tmp/redist.7z

# Acquire and move unversioned data from unversioned branch (Note this will break if you don't have a really new version of git)
git fetch -f origin unversioned:unversioned
git restore --source unversioned Content/ Binaries/ Source/UnrealLibretro/Private/Windows/SDL2 Source/UnrealLibretro/Private/Mac/SDL2
