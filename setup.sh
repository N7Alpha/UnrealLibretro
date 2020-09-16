#!/bin/bash
# This file will download the required binaries, headers, and create user folders. After running this you can drag and drop the UnrealLibretro folder into your project's plugins folder
# and you should be able to load the plugin, and the example blueprint.

# Attempt to find the package manager for this platform and install 7Zip
if   which brew
then
    brew install p7zip
elif which pacman
then
    pacman -S --noconfirm --needed p7zip
else
    echo "Couldn't find a package manager"
fi

# Make Plugin directories that won't be dynamically generated
mkdir -p MyROMs MyCores
mkdir -p Binaries/Win64 Binaries/Mac 
mkdir -p Source/ThirdParty
(cd Source/ThirdParty; mkdir -p Win64/Include Mac/Include Win64/Libraries Mac/Libraries)
mkdir -p libretro

# Get general binaries for running Libretro Cores on Windows
wget -c https://buildbot.libretro.com/nightly/windows/x86_64/redist.7z --directory /tmp
7z x -aoa -o./libretro /tmp/redist.7z

# Download SDL2 and unpack it in its respective platform directory
wget -c https://www.libsdl.org/release/SDL2-devel-2.0.12-VC.zip -O /tmp/Win64SDL.zip
wget -c https://homebrew.bintray.com/bottles/sdl2-2.0.12_1.catalina.bottle.tar.gz -O /tmp/MacOSSDL.tar.gz

7z e -y -aoa /tmp/Win64SDL.zip                          -oSource/ThirdParty/Win64/Include/SDL2 "*/include/*"
7z e -y -aoa /tmp/Win64SDL.zip                          -oSource/ThirdParty/Win64/Libraries    "*/lib/x64/*"
tar     xzvf /tmp/MacOSSDL.tar.gz --strip-components 3 -C Source/ThirdParty/Mac/Include        "*/*/include/SDL2"
tar     xzvf /tmp/MacOSSDL.tar.gz --strip-components 3 -C Source/ThirdParty/Mac/Libraries      "*/*/lib"

# Acquire and move unversioned data from unversioned branch (Note this will break if you don't have a really new version of git)
git fetch origin unversioned:unversioned
git restore --source unversioned Content/
