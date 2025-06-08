using System;
using UnrealBuildTool;

// We need a separate module for editor stuff since it's prohibited by Unreal to package editor modules when packaging for distribution
public class UnrealLibretroEditor : ModuleRules
{
    public UnrealLibretroEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        
        // Handle bEnforceIWYU deprecation in UE 5.2+
        #if UE_5_2_OR_LATER
        IWYUSupport = IWYUSupport.Full;
        #else
        bEnforceIWYU = true;
        #endif

        PrivateIncludePaths.AddRange(
        new string[]
        {
            System.IO.Path.Combine(ModuleDirectory, "miniz"),
        });
        

        PublicDependencyModuleNames.AddRange(
        new string[]
        {
            "UnrealLibretro",
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",

            // Editor stuff
            "EditorWidgets",
            "EditorStyle",
            "SlateCore",
            "Slate",
            "HTTP",
            "Projects",
        });
    }
}
