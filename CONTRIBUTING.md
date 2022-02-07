## Debugging Libretro Cores
Debugging libretro cores is kind of a nightmare since a lot of times your game will crash inside the cores code and if you don't have symbols good luck figuring that out. Luckily this [tutorial](https://docs.libretro.com/development/retroarch/compilation/windows/) mostly explains how to get symbolized debugging working. Note: You absolutely must use the MinGW64 terminal not the MSYS2 terminal when building Libretro cores for Windows. Also don't forget to enable the debug flags they mention when building with ```make```.

The one thing that isn't mentioned is that once you successfully build the DLL you have to use [cv2pdb](https://github.com/rainers/cv2pdb/releases/latest) to make a Windows debug symbol database file (pdb).

You produce the pdb by running something like this in MinGW64

```
cv2pdb.exe {core_name}.exe
```

The pdb is produced in the same directory as the DLL. Copy both of them over to MyCores. Once that's done Visual Studio should automagically load the pdb  and will ask you to point to the source files. And now you might get more insight into whats going wrong... hopefully.

## Random notes
Q: Visual Studio loads the source files for the wrong core when debugging 

A: Remove other pdbs AND source files and clean the build and rerun

### Android
OBB files must be less than 100 MB?

.so libraries are packaged in `/data/data/[Java Package]/lib` there are files named `_APL.xml` in the Unreal Engine soure that cover how to do this

Android studio device explorer will show files without needing to be root