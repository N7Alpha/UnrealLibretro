// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using UnrealBuildTool;

public class UnrealLibretro : ModuleRules
{
	public UnrealLibretro(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		bEnforceIWYU = true;

		PrivateIncludePaths.Add("UnrealLibretro/Public/libretro/include");

		RuntimeDependencies.Add("$(PluginDir)/MyRoms/*"	); // Don't actually distribute roms this is just for the convinience of testing
		RuntimeDependencies.Add("$(PluginDir)/Saves/*"	);
		RuntimeDependencies.Add("$(PluginDir)/System/*"	);

		if (Target.Platform.Equals(UnrealTargetPlatform.Win64))
        {
			RuntimeDependencies.Add("$(PluginDir)/MyCores/Win64/*");
			RuntimeDependencies.Add("$(PluginDir)/Binaries/Win64/ThirdParty/libretro/*");
        }
		else if (Target.Platform.Equals(UnrealTargetPlatform.Android))
        {
			RuntimeDependencies.Add("$(PluginDir)/MyCores/Android/armeabi-v7a/*");
			RuntimeDependencies.Add("$(PluginDir)/MyCores/Android/arm64-v8a/*");
		}

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"RenderCore",
				"RHI",
				"CoreUObject",
				"Engine",
				"InputCore",
				"Projects",
				"HeadMountedDisplay",
				"NavigationSystem",
			}
			);

		if (   Target.Version.MajorVersion == 4
		    || Target.Version.MinorVersion == 0)
        {
			PrivateDependencyModuleNames.Add("OculusHMD");
		}
		else
		{
			PrivateDependencyModuleNames.Add("OpenXRInput");
		}

		if (   Target.Version.MajorVersion >  4
			|| Target.Version.MinorVersion >= 26)
        {
			PublicDependencyModuleNames.Add("DeveloperSettings"); // Was moved into its own module in 4.26
		}

		if (   Target.Version.MajorVersion == 5
		    && Target.Version.MinorVersion >= 2)
		{
			PrivateDependencyModuleNames.Add("AudioExtensions");
		}
	}
}
