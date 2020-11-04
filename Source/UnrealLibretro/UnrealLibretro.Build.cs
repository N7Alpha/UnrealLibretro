// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using UnrealBuildTool;

public class UnrealLibretro : ModuleRules
{
	public UnrealLibretro(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        if (Target.Platform.Equals(UnrealTargetPlatform.Mac))
        {
			RuntimeDependencies.Add("$(PluginDir)/Binaries/Win64/ThirdParty/libSDL2*.dylib"); // globbed because libSDL2.dylib is an alias
			PublicAdditionalLibraries.Add("$(PluginDir)/Binaries/Mac/ThirdParty/libSDL2.dylib");
        }
        else if (Target.Platform.Equals(UnrealTargetPlatform.Win64))
        {
            PublicAdditionalLibraries.Add("$(PluginDir)/Binaries/Win64/ThirdParty/SDL2.lib");

			RuntimeDependencies.Add("$(PluginDir)/Binaries/Win64/ThirdParty/SDL2.dll");
			PublicDelayLoadDLLs.Add("SDL2.dll");
        }
        else
        {
			throw new System.PlatformNotSupportedException("Only building for Windows 64-bit and MacOS is supported");
        }
		
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"RenderCore",
				"RHI",
				"UnrealEd",
				"CoreUObject",
				"Engine",
				"InputCore",
				"Slate",
				"Projects"
			}
			);
	}
}
