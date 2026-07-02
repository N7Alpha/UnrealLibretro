#pragma once

#include "Commandlets/Commandlet.h"
#include "DumpBlueprintCommandlet.generated.h"

/**
 * Dumps every object in the given asset packages to UE's canonical T3D text form -- the same
 * serialization behind graph copy/paste (FEdGraphUtilities::ExportNodesToText) and Epic's P4V
 * asset diff. For Blueprints this captures the full node graph: node classes, pins, connections
 * (LinkedTo GUID references), default values, node positions, and comments.
 *
 * Usage:
 *   UnrealEditor-Cmd <project> -run=DumpBlueprint <PackageNameOrUassetPath>... [-Output=<dir>]
 *
 * Accepts /LongPackage/Names and filesystem .uasset paths. Without -Output the .t3d is written
 * next to each asset.
 */
UCLASS()
class UDumpBlueprintCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    virtual int32 Main(const FString& Params) override;
};
