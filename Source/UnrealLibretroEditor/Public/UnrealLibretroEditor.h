#pragma once

#include "UnrealLibretro.h"
#include "Modules/ModuleInterface.h"
#include "CoreMinimal.h"

struct FCoreListViewRowDataSource
{
    uint32 PlatformAvailableBitField;
    uint32 PlatformDownloadedBitField;
    int32  DownloadsPending;
};

static_assert(sizeof(CoreLibMetadata) / sizeof(CoreLibMetadata[0]) < 8 * sizeof(FCoreListViewRowDataSource::PlatformAvailableBitField),
    "PlatformAvailableBitField size is exceeded");

class FUnrealLibretroEditorModule : public IModuleInterface
{
public:
    TMap<FString, FCoreListViewRowDataSource> CoreListViewDataSource;

    // This should work for DEFLATE and gz2 but not much else I think
    // Note: Supports only single file zips
    static const char* UnzipArchive(const TArray<uint8>& ZippedData, uint8** UnzippedData, size_t* UnzippedDataSize);

    /** IModuleInterface implementation */
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
