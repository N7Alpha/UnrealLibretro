#!/usr/bin/env python

import os
import sys
import re
from pathlib import Path
import tempfile
import shutil
import json

def get_unreal_version(install_directory):
    # Path to the BaseEngine.ini file
    version_path = os.path.join(install_directory, "Engine", "Build", "Build.version")

    # Check if the file exists
    if not os.path.exists(version_path):
        raise Exception(f"Could not find {version_path}")

    # Read the file and search for the version info
    with open(version_path, 'r') as file:
        data = json.load(file)
        major_version = data.get("MajorVersion")
        minor_version = data.get("MinorVersion")

        if major_version is not None and minor_version is not None:
            return major_version, minor_version
        else:
            raise Exception("Could not find Unreal Engine version in 'Build.version'")

ue_path = sys.argv[1]

plugin_path = os.path.dirname(os.path.abspath(__file__))

major, minor = get_unreal_version(ue_path)

if major == 4 or minor <= 2:
    shutil.copy2(f"{plugin_path}/UnrealLibretro.uplugin.UE5.2", f"{plugin_path}/UnrealLibretro.uplugin")
else:
    shutil.copy2(f"{plugin_path}/UnrealLibretro.uplugin.UE5.3", f"{plugin_path}/UnrealLibretro.uplugin")

packaged_plugin_path = f'{plugin_path}/UE_{major}.{minor}/UnrealLibretro'

status = os.system('"{ue_path}/Engine/Build/BatchFiles/RunUAT" BuildPlugin -Rocket -Plugin={plugin_path}/UnrealLibretro.uplugin -TargetPlatforms=Win64 -Package={packaged_plugin_path} -VS2019'.format(
    **locals()
))

if status:
    exit(status)

def unix_touch(path):
    Path(os.path.dirname(path)).mkdir()
    Path(path).touch()

unix_touch("{packaged_plugin_path}/MyROMs/Place Your ROMs in this Directory".format(**locals()))
unix_touch("{packaged_plugin_path}/MyCores/Place Your Libretro Cores in this Directory".format(**locals()))
