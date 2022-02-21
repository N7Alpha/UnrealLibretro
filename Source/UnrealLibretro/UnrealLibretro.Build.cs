// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using UnrealBuildTool;

public class UnrealLibretro : ModuleRules
{
	public UnrealLibretro(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateIncludePaths.Add("UnrealLibretro/Private/libretro/include");

		RuntimeDependencies.Add("$(PluginDir)/MyRoms/*"	);
		RuntimeDependencies.Add("$(PluginDir)/Saves/*"	);
		RuntimeDependencies.Add("$(PluginDir)/System/*"	);


		if (Target.Platform.Equals(UnrealTargetPlatform.Mac))
        {
			RuntimeDependencies.Add("$(PluginDir)/MyCores/Mac/*");
			RuntimeDependencies.Add("$(PluginDir)/Binaries/Mac/ThirdParty/libSDL2*.dylib"); // globbed because libSDL2.dylib is an alias
			PublicAdditionalLibraries.Add("$(PluginDir)/Binaries/Mac/ThirdParty/libSDL2.dylib");
        }
        else if (Target.Platform.Equals(UnrealTargetPlatform.Win64))
        {
			RuntimeDependencies.Add("$(PluginDir)/MyCores/Win64/*");
            PublicAdditionalLibraries.Add("$(PluginDir)/Binaries/Win64/ThirdParty/SDL2.lib");
			RuntimeDependencies.Add("$(PluginDir)/Binaries/Win64/ThirdParty/SDL2.dll");
			RuntimeDependencies.Add("$(PluginDir)/Binaries/Win64/ThirdParty/libretro/*");
			PublicDelayLoadDLLs.Add("SDL2.dll");
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
				"Slate",
				"Projects",
			}
			);

		if (   Target.Version.MajorVersion >  4
			|| Target.Version.MinorVersion >= 26)
        {
			PublicDependencyModuleNames.Add("DeveloperSettings"); // Was moved into its own module in 4.26
		}
	}
}
