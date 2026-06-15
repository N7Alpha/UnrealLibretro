using System;
using System.IO;
using UnrealBuildTool;

public class UnrealLibretro : ModuleRules
{
	public UnrealLibretro(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		// Handle bEnforceIWYU deprecation in UE 5.2+
		#if UE_5_2_OR_LATER
		IWYUSupport = IWYUSupport.Full;
		#else
		bEnforceIWYU = true;
		#endif

		PrivateIncludePaths.Add("UnrealLibretro/Public/libretro/include");

		RuntimeDependencies.Add("$(PluginDir)/MyRoms/*"); // Don't actually distribute roms this is just for the convinience of testing
		RuntimeDependencies.Add("$(PluginDir)/Saves/*");
		RuntimeDependencies.Add("$(PluginDir)/System/*");

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

		if (   Target.Version.MajorVersion == 5
		    && Target.Version.MinorVersion >= 3)
		{
			PublicDependencyModuleNames.Add("XRBase");
		}

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

		// MARK: ThirdParty Libraries
		// netarch
		PrivateDefinitions.Add("SAM2_ENABLE_LOGGING");
		PrivateIncludePaths.Add("$(PluginDir)/../ThirdParty/netarch");

		// NetImgui
		PrivateDependencyModuleNames.AddRange(new string[] { "Sockets", "Networking" });
		//PrivateIncludePaths.Add(PluginDirectory + "/Source/UnrealLibretro/netImgui");
		//PCHUsage = PCHUsageMode.NoSharedPCHs; // Prevents problem with Dear ImGui/NetImgui sources not including the right first header
		//PrivatePCHHeaderFile = "Public/UnrealLibretro.h";

			// imgui stuff
		PrivateDefinitions.Add("IMGUI_DEFINE_MATH_OPERATORS");  // We get unity build issues/packaging issues if this isn't defined
		PrivateIncludePaths.Add("$(PluginDir)/../ThirdParty/imgui");
		PrivateIncludePaths.Add("$(PluginDir)/../ThirdParty/netImgui/Code/Client");
		PrivateIncludePaths.Add("$(PluginDir)/../ThirdParty/implot");
	}
}
