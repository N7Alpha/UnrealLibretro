#!/bin/bash
# This file will setup the required binaries and create user folders. After running this you can drag and drop the UnrealLibretro folder into your project's plugins folder
# and you should be able to load the plugin, and then try loading the example map in the plugin's content folder don't forget to enable "Show Plugin Content".

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
wget -c https://buildbot.libretro.com/nightly/windows/x86_64/RetroArch_update.7z --directory /tmp
7z x -aoa -o/tmp /tmp/RetroArch_update.7z
cp /tmp/RetroArch-Win64/*.dll Binaries/Win64/ThirdParty/libretro
