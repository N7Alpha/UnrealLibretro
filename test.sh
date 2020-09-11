#!/bin/bash

if which brew
then
    install=brew\ install
fi

if which pacman 
then
    install=pacman\ -S\ --noconfirm\ --needed
fi

if [ -z "$install" ]
then
    echo "couldn't find a package manager"
    install=echo\ "Skipping install of "
fi

$install p7zip