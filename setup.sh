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
wget -c https://buildbot.libretro.com/nightly/windows/x86_64/RetroArch_update.7z --directory /tmp --timeout=600
7z x -aoa -o/tmp /tmp/RetroArch_update.7z
cp /tmp/RetroArch-Win64/*.dll Binaries/Win64/ThirdParty/libretro

# Look for the first .uproject file two directories up
U_PROJECT_FILE=$(find ../../ -maxdepth 1 -name "*.uproject" | head -n 1)

# If .uproject file not found, default to UE5.02
if [[ ! -f $U_PROJECT_FILE ]]; then
    cp UnrealLibretro.uplugin.UE5.3 UnrealLibretro.uplugin
    exit
fi

ENGINE_VERSION=$(grep -oP '"EngineAssociation": "\K[0-9.]*' "$U_PROJECT_FILE")

MAJOR_VERSION=$(echo $ENGINE_VERSION | cut -d'.' -f1)
MINOR_VERSION=$(echo $ENGINE_VERSION | cut -d'.' -f2)

echo "Detected Engine Version: $MAJOR_VERSION.$MINOR_VERSION"

if [[ $MAJOR_VERSION -eq 4 || $MINOR_VERSION -le 2 ]]; then
    cp UnrealLibretro.uplugin.UE5.2 UnrealLibretro.uplugin
else
    cp UnrealLibretro.uplugin.UE5.3 UnrealLibretro.uplugin
fi

echo "Success! Now in the Unreal Engine Editor make sure you enable 'Show Plugin Content', then open the example map in the plugin's content folder"
