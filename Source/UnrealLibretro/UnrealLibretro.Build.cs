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
            PrivateIncludePaths.Add(System.IO.Path.Combine(ModuleDirectory, "ThirdParty/Mac/Include"));
            PublicAdditionalLibraries.Add(System.IO.Path.Combine(ModuleDirectory, "ThirdParty/Mac/Libraries/libSDL2-2.0.0.dylib"));
        }
        else if (Target.Platform.Equals(UnrealTargetPlatform.Win64))
        {
            PrivateIncludePaths.Add(System.IO.Path.Combine(ModuleDirectory, "ThirdParty/Win64/Include"));
            PublicAdditionalLibraries.Add(System.IO.Path.Combine(ModuleDirectory, "ThirdParty/Win64/Libraries/SDL2.lib"));
            PublicAdditionalLibraries.Add(System.IO.Path.Combine(ModuleDirectory, "ThirdParty/Win64/Libraries/SDL2test.lib"));
        }
        else
        {
			throw new System.PlatformNotSupportedException("Only building for Windows 64-bit and MacOS is supported");
        }

        PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
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
				"HeadMountedDisplay",
				"Projects"
                
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				// ... add private dependencies that you statically link with here ...	
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
