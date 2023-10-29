# Contributing

## Debugging Libretro Cores

Debugging libretro cores is kind of a nightmare since a lot of times your game will crash inside the cores code and if you don't have symbols good luck figuring that out. Luckily this [tutorial](https://docs.libretro.com/development/retroarch/compilation/windows/) mostly explains how to get symbolized debugging working. Note: You absolutely must use the MinGW64 terminal not the MSYS2 terminal when building Libretro cores for Windows. Also don't forget to enable the debug flags they mention when building with ```make```.

The one thing that isn't mentioned is that once you successfully build the DLL you have to use [cv2pdb](https://github.com/rainers/cv2pdb/releases/latest) to make a Windows debug symbol database file (pdb).

You produce the pdb by running something like this in MinGW64

```
cv2pdb.exe {core_name}.exe
```

The pdb is produced in the same directory as the DLL. Copy both of them over to MyCores. Once that's done Visual Studio should automagically load the pdb  and will ask you to point to the source files. And now you might get more insight into whats going wrong... hopefully.

## My Approach to Source Control

`git` is not good with binary assets. `.uasset` files are binary assets. If you want to modify existing blueprints or assets this has to be communicated through a github issue or pull request.

Basically only tagged commits on master are safe to check out this is because if I expect to have a lot of churn modifying assets I'll...

1. do the work on a `dev` branch and do as many commits as I feel is needed
2. once the assets are as I like them I'll rebase the `dev` branch onto `master` and squash the blueprints into one commit at the top. The net effect here is the source changes are saved in history although you can't directly check them out and expect it to work since the history will be lost on `master` (by the squashed commits)
3. I'll rename `dev` to something like `dev-vX.X.X` so I can keep the complete history around
4. If this repo ever gets too bloated I'll move the archive branches to some archive repo

This is complicated, but I'll figure out how to merge changes if people have them.

## Random notes

To get faster iteration times use [Live Coding](https://docs.unrealengine.com/5.0/en-US/using-live-coding-to-recompile-unreal-engine-applications-at-runtime/) and add `bUseUnityBuild = false;` to your `[Project Name].Target.cs` and `[Project Name]Editor.Target.cs` build files. Mind that this might slow down the initial build the recompiles should be much faster.

Q: Visual Studio loads the source files for the wrong core when debugging

A: Remove other pdbs AND source files and clean the build and rerun

Stage changes that aren't line ending changes in git
`git diff -U0 -w --no-color | git apply --cached --ignore-whitespace --unidiff-zero -`

### Android

OBB files must be less than 100 MB?

.so libraries are packaged in `/data/data/[Java Package]/lib` there are files named `_APL.xml` in the Unreal Engine soure that cover how to do this

Android studio device explorer will show files without needing to be root

### Continous Integration testing

Probably can use [this](https://github.com/pfist/Nano) to bootstrap a project environment 

`E:\UE_4.26/Engine/Build/BatchFiles/RunUAT BuildCookRun -project=E:\Build\Nano\Nano.uproject -build -run -unattended -editortest
-nocompile -map=LibretroWorld`
