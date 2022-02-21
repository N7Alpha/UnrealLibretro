#!/usr/bin/env python

import os
import sys
from pathlib import Path

directory_containing_this_script = os.path.dirname(os.path.abspath(__file__))

ue_path = sys.argv[1]
plugin_path = os.path.dirname(os.path.abspath(__file__))
ue_version = os.path.basename(ue_path)

packaged_plugin_path = '{plugin_path}/{ue_version}/UnrealLibretro'.format(**locals())

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