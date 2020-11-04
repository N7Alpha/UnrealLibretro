#!/bin/bash
# This file will setup the required binaries, headers, and create user folders. After running this you can drag and drop the UnrealLibretro folder into your project's plugins folder
# and you should be able to load the plugin, and the example blueprint.

# Prompt user to install 7zip if necessary
if ! command -v 7z
then
    echo "7zip not installed"
    echo "Do you wish to install this program?"
    select yn in "Yes" "No"
    case $yn in
        Yes ) echo "Attempting to install";;
        No )  echo "7zip needs to be installed to run setup"; exit;;
    esac

    if   command -v brew
    then
        brew install p7zip
    elif command -v pacman
    then
        pacman -S --noconfirm --needed p7zip
    else
        echo "Couldn't find a package manager for this platform"
        exit;
    fi
fi

# Make directories that won't be dynamically generated
mkdir -p MyROMs MyCores
mkdir -p libretro

# Get general binaries for running Libretro Cores on Windows
wget -c https://buildbot.libretro.com/nightly/windows/x86_64/redist.7z --directory /tmp
7z x -aoa -o./libretro /tmp/redist.7z

# Acquire and move unversioned data from unversioned branch (Note this will break if you don't have a really new version of git)
git fetch -f origin unversioned:unversioned
git restore --source unversioned Source/ Content/ Binaries/
