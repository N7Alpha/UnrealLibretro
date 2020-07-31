#!/bin/bash
# So I don't accidently force push to master

git add -f Content/ Binaries/*/ThirdParty/* Source/UnrealLibretro/*/SDL2/*
git add -f push.sh
git reset Binaries/Win*/ThirdParty/libretro/*
git commit --amend --no-edit
git push -f origin unversioned