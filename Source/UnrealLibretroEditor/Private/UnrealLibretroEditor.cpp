#include "UnrealLibretroEditor.h"
#include "LibretroCoreInstanceDetails.h"

#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "DetailCustomizations.h"
#include "HttpModule.h"
#include "PlatformHttp.h"
#include "Interfaces/IHttpResponse.h"

#include "miniz.h"

#define LOCTEXT_NAMESPACE "FUnrealLibretroEditorModule"

const char* FUnrealLibretroEditorModule::UnzipArchive(const TArray<uint8>& ZippedData, uint8** UnzippedData, size_t* UnzippedDataSize)
{
    mz_zip_archive zip_archive = {0};

    if (!mz_zip_reader_init_mem(&zip_archive, ZippedData.GetData(), ZippedData.Num(), 0)) goto cleanup;

    // Custom failure case we expect only 1 file in the archive
    if (mz_zip_reader_get_num_files(&zip_archive) != 1)
    {
        mz_zip_reader_end(&zip_archive);
        return "Zip file contains more than one file";
    }

    mz_zip_archive_file_stat file_stat;
    if (!mz_zip_reader_file_stat(&zip_archive, 0, &file_stat)) goto cleanup;

    // Actual decompression happens here
    *UnzippedDataSize = file_stat.m_uncomp_size;
    *UnzippedData = (uint8 *) mz_zip_reader_extract_file_to_heap(&zip_archive, file_stat.m_filename, UnzippedDataSize, 0);
    if (*UnzippedData == nullptr) goto cleanup;

cleanup:
    mz_zip_error zip_error = mz_zip_get_last_error(&zip_archive);

    // Close the archive, freeing any resources it was using
    mz_zip_reader_end(&zip_archive);

    return zip_error == MZ_ZIP_NO_ERROR ? nullptr : mz_zip_get_error_string(zip_error);
}


void FUnrealLibretroEditorModule::StartupModule()
{
    // This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
    FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(FName("PropertyEditor"));
    PropertyModule.RegisterCustomClassLayout("LibretroCoreInstance", FOnGetDetailCustomizationInstance::CreateStatic(&FLibretroCoreInstanceDetails::MakeInstance));

    PropertyModule.NotifyCustomizationModuleChanged();

    // Download Libretro Core list for each platform
    for (int PlatformIndex = 0; PlatformIndex < sizeof(CoreLibMetadata) / sizeof(CoreLibMetadata[0]); PlatformIndex++)
    {
        auto BuildbotRequest = FHttpModule::Get().CreateRequest();
        BuildbotRequest->SetVerb(TEXT("GET"));
        BuildbotRequest->SetURL(CoreLibMetadata[PlatformIndex].BuildbotPath + TEXT(".index-extended"));

        BuildbotRequest->OnProcessRequestComplete().BindLambda(
            [this, PlatformIndex](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
            {
                if (bSucceeded)
                {
                    // Handle the result on the GameThread because we're modifying datastructures that Slate uses too
                    FFunctionGraphTask::CreateAndDispatchWhenReady([=]
                        {
                            // Each row in this array looks something like this "2022-10-21 bb0926bb boom3_libretro.dll.zip"
                            // CoreIdentifer = "boom3" for this example
                            TArray<FString> CoreIndexRows;
                            HttpResponse->GetContentAsString().ParseIntoArrayLines(CoreIndexRows);

                            for (auto CoreIndexRow : CoreIndexRows)
                            {
                                TArray<FString> CoreIndexColumnEntries;
                                CoreIndexRow.ParseIntoArrayWS(CoreIndexColumnEntries);

                                auto CoreIdentiferEnd = CoreIndexColumnEntries.Last().Find(CoreLibMetadata[PlatformIndex].Extension, ESearchCase::CaseSensitive);
                                auto CoreIdentifer = FString(CoreIdentiferEnd, *CoreIndexColumnEntries.Last());
                                CoreListViewDataSource.FindOrAdd(CoreIdentifer).PlatformAvailableBitField |= (1UL << PlatformIndex);

                                // The logic here is naive I make no attempt to periodically poll the directory for external file creation/deletion
                                auto RelativeCorePath = CoreLibMetadata[PlatformIndex].DistributionPath + CoreIdentifer + CoreLibMetadata[PlatformIndex].Extension;
                                if (FPaths::FileExists(FUnrealLibretroModule::ResolveCorePath(RelativeCorePath)))
                                {
                                    CoreListViewDataSource[CoreIdentifer].PlatformDownloadedBitField |= (1UL << PlatformIndex);
                                }
                            }
                        }, TStatId(), nullptr, ENamedThreads::GameThread);		
                }
                else
                {
                    UE_LOG(Libretro, Warning, TEXT("Failed to get core list for platform"));
                }
            });
        
        BuildbotRequest->ProcessRequest();
    }
}

void FUnrealLibretroEditorModule::ShutdownModule()
{
    FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
    PropertyModule.UnregisterCustomClassLayout("LibretroCoreInstance");
    PropertyModule.NotifyCustomizationModuleChanged();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUnrealLibretroEditorModule, UnrealLibretroEditor)