// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UnrealLibretro : ModuleRules
{
	public UnrealLibretro(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicAdditionalLibraries.Add(System.IO.Path.Combine(ModuleDirectory, "../../Binaries/Win64/SDL2.lib"));
		PublicAdditionalLibraries.Add(System.IO.Path.Combine(ModuleDirectory, "../../Binaries/Win64/SDL2test.lib"));
        PublicAdditionalLibraries.Add(System.IO.Path.Combine(ModuleDirectory, "../../Binaries/Mac/libSDL2-2.0.0.dylib"));
        

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
