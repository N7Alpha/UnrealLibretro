using System;
using System.IO;
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
		
		PrivateDependencyModuleNames.AddRange(new string[] { "Sockets", "Networking" }); // NetImgui
		//PrivateIncludePaths.Add(PluginDirectory + "/Source/UnrealLibretro/netImgui");
		//PCHUsage = PCHUsageMode.NoSharedPCHs; // Prevents problem with Dear ImGui/NetImgui sources not including the right first header
		//PrivatePCHHeaderFile = "Public/UnrealLibretro.h";

		// libjuice stuff
		PrivateDefinitions.Add("USE_NETTLE=0");
		PrivateDefinitions.Add("JUICE_STATIC");
		PrivateDefinitions.Add("JUICE_DISABLE_CONSENT_FRESHNESS=0");
		PrivateDefinitions.Add("JUICE_ENABLE_LOCAL_ADDRESS_TRANSLATION=0");
		//PrivateDefinitions.Add("__STDC_VERSION__=201112L"); // Ehhhh
		PrivateIncludePaths.Add("$(PluginDir)/../ThirdParty/libjuice/include");
		PrivateIncludePaths.Add("$(PluginDir)/../ThirdParty/libjuice/include/juice"); // We #include libjuice implementation files which expect this

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			// Link against the bcrypt library
			PublicAdditionalLibraries.Add("bcrypt.lib");
		}

		// imgui stuff
		PrivateIncludePaths.Add("$(PluginDir)/../ThirdParty/imgui");
		PrivateIncludePaths.Add("$(PluginDir)/../ThirdParty/netImgui/Code/Client");
		PrivateIncludePaths.Add("$(PluginDir)/../ThirdParty/implot");

		// zstd stuff
		PublicIncludePaths.Add("$(PluginDir)/Source/ThirdParty/zstd/lib");
	}
}
