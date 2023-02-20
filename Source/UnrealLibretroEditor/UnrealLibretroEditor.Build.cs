using System;
using UnrealBuildTool;

// We need a separate module for editor stuff since it's prohibited by Unreal to package editor modules when packaging for distribution
public class UnrealLibretroEditor : ModuleRules
{
    public UnrealLibretroEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        bEnforceIWYU = true;

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