#!/usr/bin/env python

import os
import sys
import subprocess
import shutil
import json
import argparse

def get_unreal_version(engine_install_directory="C:/Program Files/Epic Games/UE_X.XX"):
    version_path = os.path.join(engine_install_directory, "Engine", "Build", "Build.version")

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

def git_get_version_name(revision="HEAD"):
    try:
        # If on a tagged commit, this will return just the tag
        # If not on a tagged commit, this will return <last tag>-<rev>-<short hash>
        result = subprocess.check_output(
            ["git", "describe", "--tags", "--always", "--abbrev=7", revision],
            stderr=subprocess.STDOUT
        ).decode("utf-8").strip()
    except subprocess.CalledProcessError:
        # This shouldn't really happen given the flags used above, 
        # but in case of some other unexpected error, fallback to just the short hash
        result = subprocess.check_output(
            ["git", "rev-parse", "--short", revision],
            stderr=subprocess.STDOUT
        ).decode("utf-8").strip()

    return result

if __name__ == "__main__":
    plugin_path = os.path.dirname(os.path.abspath(__file__))

    parser = argparse.ArgumentParser(description="Build and package UnrealLibretro plugin.")
    parser.add_argument("ue_path", help="Path to an Unreal Engine installation you want to build the plugin for.")
    parser.add_argument("package_path", nargs='?', help="Path where the plugin will be packaged.")

    args = parser.parse_args()
    ue_path = args.ue_path
    if args.package_path is None:
        package_path = plugin_path
    else:
        package_path = os.path.abspath(args.package_path) 

    win64_redist_path = os.path.join(plugin_path, "Binaries", "Win64", "ThirdParty", "libretro")

    # Check if the directory exists
    if not os.path.isdir(win64_redist_path):
        print(f"{__file__}: ERROR: Required directory not found: {win64_redist_path}")
        print(f"{__file__}: Please run setup.sh (Linux/macOS) or setup.cmd (Windows) first.")
        sys.exit(1)

    major, minor = get_unreal_version(ue_path)

    if major == 4 or minor <= 2:
        uplugin_json = json.load(open(f"UnrealLibretro.uplugin.UE5.2"))
    else:
        uplugin_json = json.load(open(f"UnrealLibretro.uplugin.UE5.3"))

    uplugin_json["VersionName"] = git_get_version_name(revision="HEAD")
    json.dump(uplugin_json, open("UnrealLibretro.uplugin", "w"), indent=4)

    status = os.system(
        f'"{ue_path}/Engine/Build/BatchFiles/RunUAT" BuildPlugin -Rocket'
        f' -Plugin={plugin_path}/UnrealLibretro.uplugin -TargetPlatforms=Win64'
        f' -Package={package_path}/UnrealLibretro-{major}.{minor}/UnrealLibretro -VS2019'
    )

    if status:
        sys.exit(status)

    def unix_touch(path):
        from pathlib import Path
        Path(os.path.dirname(path)).mkdir()
        Path(path).touch()

    unix_touch(f'{package_path}/UnrealLibretro-{major}.{minor}/UnrealLibretro/MyROMs/Place Your ROMs in this Directory')
    unix_touch(f'{package_path}/UnrealLibretro-{major}.{minor}/UnrealLibretro/MyCores/Place Your Libretro Cores in this Directory')

    os.system(f'tar -acf {package_path}/UnrealLibretro-{major}.{minor}.zip -C {package_path}/UnrealLibretro-{major}.{minor} UnrealLibretro')
