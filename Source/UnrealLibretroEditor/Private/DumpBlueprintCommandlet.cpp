#include "DumpBlueprintCommandlet.h"

#include "Exporters/Exporter.h"
#include "UnrealExporter.h"
#include "Misc/FileHelper.h"
#include "Misc/OutputDevice.h"
#if __has_include("Misc/StringOutputDevice.h")
#include "Misc/StringOutputDevice.h" // FStringOutputDevice lives here since UE 5.7
#endif
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/PropertyPortFlags.h"
#include "UObject/UObjectIterator.h"
#include "UnrealLibretro.h"

int32 UDumpBlueprintCommandlet::Main(const FString& Params)
{
    TArray<FString> Tokens;
    TArray<FString> Switches;
    TMap<FString, FString> ParamsMap;
    ParseCommandLine(*Params, Tokens, Switches, ParamsMap);

    const FString OutputDir = ParamsMap.FindRef(TEXT("Output"));

    int32 DumpedCount = 0;
    int32 ExitCode = 0;
    for (const FString& Token : Tokens)
    {
        // Accept both /Long/Package/Names and filesystem paths
        FString PackageFilename = Token;
        if (FPackageName::IsValidLongPackageName(Token)
#if ENGINE_MAJOR_VERSION >= 5
            && !FPackageName::DoesPackageExist(Token, &PackageFilename))
#else // UE4 takes an optional package GUID before the filename out-param
            && !FPackageName::DoesPackageExist(Token, nullptr, &PackageFilename))
#endif
        {
            UE_LOG(Libretro, Error, TEXT("Package '%s' does not exist"), *Token);
            ExitCode = 1;
            continue;
        }

        // Same load/gather/export sequence as UDiffAssetsCommandlet, which is what P4V's
        // Blueprint text diff uses -- LOAD_ForDiff avoids touching the live package in memory
        UPackage* Package = LoadPackage(nullptr, *PackageFilename, LOAD_ForDiff);
        if (!Package)
        {
            UE_LOG(Libretro, Error, TEXT("Could not load '%s'"), *PackageFilename);
            ExitCode = 1;
            continue;
        }

        TArray<UObject*> Objects;
        for (TObjectIterator<UObject> It; It; ++It)
        {
            if (It->GetOuter() == Package)
            {
                Objects.Add(*It);
            }
        }
        Objects.Sort([](const UObject& A, const UObject& B) { return A.GetName() < B.GetName(); });

        FStringOutputDevice Buffer;
        const FExportObjectInnerContext Context;
        for (UObject* Object : Objects)
        {
            UExporter* Exporter = UExporter::FindExporter(Object, TEXT("t3d"));
            if (!Exporter)
            {
                UE_LOG(Libretro, Warning, TEXT("No t3d exporter for '%s' (%s); skipped"),
                    *Object->GetName(), *Object->GetClass()->GetName());
                continue;
            }
            UExporter::ExportToOutputDevice(&Context, Object, Exporter, Buffer, TEXT("t3d"), 0,
                PPF_ExportsNotFullyQualified, false);
        }

        if (!Buffer.Len())
        {
            UE_LOG(Libretro, Error, TEXT("No text was exported for '%s'"), *PackageFilename);
            ExitCode = 1;
            continue;
        }

        const FString OutPath = OutputDir.IsEmpty()
            ? FPaths::ChangeExtension(PackageFilename, TEXT("t3d"))
            : OutputDir / FPaths::GetBaseFilename(PackageFilename) + TEXT(".t3d");
        if (FFileHelper::SaveStringToFile(Buffer, *OutPath))
        {
            UE_LOG(Libretro, Display, TEXT("Dumped '%s' -> '%s' (%d chars)"), *PackageFilename, *OutPath, Buffer.Len());
            DumpedCount++;
        }
        else
        {
            UE_LOG(Libretro, Error, TEXT("Failed to write '%s'"), *OutPath);
            ExitCode = 1;
        }
    }

    if (DumpedCount == 0 && ExitCode == 0)
    {
        UE_LOG(Libretro, Error, TEXT("Usage: -run=DumpBlueprint <PackageNameOrUassetPath>... [-Output=<dir>]"));
        ExitCode = 1;
    }

    return ExitCode;
}
