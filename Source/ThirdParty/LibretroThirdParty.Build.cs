using UnrealBuildTool;
using System.IO;
using System;

// This doesn't build anything at the moment it's only purpose is so third party libraries can be indexed by IntelliSense
public class LibretroThirdParty : ModuleRules
{
	public LibretroThirdParty(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External; // Don't allow UBT to automagically compile all source files it sees
	}
}
