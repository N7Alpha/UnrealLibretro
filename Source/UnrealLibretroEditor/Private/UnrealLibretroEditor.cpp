#include "UnrealLibretroEditor.h"
#include "LibretroCoreInstanceDetails.h"

#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "DetailCustomizations.h"
#include "HttpModule.h"
#include "PlatformHttp.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/FileHelper.h"
#include "Misc/ScopedSlowTask.h"

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

#include "lzma/Alloc.h"
#include "lzma/7z.h"
#include "lzma/7zAlloc.h"
#include "lzma/7zTypes.h"
#include "lzma/7zFile.h"
#include "lzma/7zCrc.h"

#define kInputBufSize ((size_t)1 << 18)


/**
 * @brief Extracts files from a 7zip archive to the specified output path.
 * This function extracts files from a 7zip archive located at the specified ArchiveFilePath
 * to the specified OutputPath. The filter function should take a file path string as input and return a boolean value.
 * If IsTargetFile is not provided, all files in the archive will be extracted.
 * The path within the archive is ignored everything is copied to the root of OutputPath
 * 
 * 
 * @param ArchiveFilePath The path to the 7zip archive to extract files from.
 * @param OutputPath The path to the directory where extracted files will be written
 * @param IsTargetFile A function that takes a file path string as input and returns a boolean value
 * 
 * @return A non-empty string if an error occurred during extraction, otherwise it's empty
 */
FString Unzip7ZipArchive(FString ArchiveFilePath, FString OutputPath, TUniqueFunction<bool(FString)> IsTargetFile = [](FString){ return true; })
{
    ISzAlloc allocImp{g_Alloc};
    ISzAlloc allocTempImp{g_Alloc};

    CFileInStream archiveStream;
    CLookToRead2 lookStream;
    CSzArEx db;
    SRes res{SZ_OK};

#if defined(_WIN32) && !defined(USE_WINDOWS_FILE) && !defined(UNDER_CE)
    g_FileCodePage = AreFileApisANSI() ? CP_ACP : CP_OEMCP;
#endif

    {
        WRes wres = InFile_Open(&archiveStream.file, TCHAR_TO_UTF8(*ArchiveFilePath));

        if (wres != 0)
        {
            return FString::Format(TEXT("Cannot open input file '%s' code: %d"), { ArchiveFilePath, wres });
        }
    }

    FileInStream_CreateVTable(&archiveStream);
    archiveStream.wres = 0;
    LookToRead2_CreateVTable(&lookStream, False);

    Byte LookStreamBuf[kInputBufSize];

    lookStream.buf = LookStreamBuf;
    lookStream.bufSize = kInputBufSize;
    lookStream.realStream = &archiveStream.vt;
    LookToRead2_Init(&lookStream);

    SzArEx_Init(&db);

    res = SzArEx_Open(&db, &lookStream.vt, &allocImp, &allocTempImp);

    if (res == SZ_OK)
    {
        UInt32 blockIndex = 0xFFFFFFFF; /* it can have any value before first call (if outBuffer = 0) */
        Byte* outBuffer = 0; /* it must be 0 before first call for each new archive. */
        size_t outBufferSize = 0;  /* it can have any value before first call (if outBuffer = 0) */

        for (UInt32 i = 0; i < db.NumFiles; i++)
        {
            size_t OffsetIntoOutBufferThatTheDllWasWrittenToBytes = 0;
            size_t DllSizeBytes = 0;

            size_t offs = db.FileNameOffsets[i];
            size_t len = db.FileNameOffsets[i + 1] - offs;
            FString FilePathInsideArchive{(int) len, reinterpret_cast<TCHAR *>(db.FileNames) + offs};

            if (!IsTargetFile(FilePathInsideArchive)) continue;

            res = SzArEx_Extract(&db, &lookStream.vt, i,
                &blockIndex, &outBuffer, &outBufferSize,
                &OffsetIntoOutBufferThatTheDllWasWrittenToBytes, &DllSizeBytes,
                &allocImp, &allocTempImp);

            if (res != SZ_OK) break;

            FString DllSavePath = FPaths::Combine(OutputPath, FPaths::GetCleanFilename(FilePathInsideArchive));
            TArrayView<uint8> DllData{ outBuffer + OffsetIntoOutBufferThatTheDllWasWrittenToBytes, (int)DllSizeBytes };

            if (!FFileHelper::SaveArrayToFile(DllData, *DllSavePath))
            {
                UE_LOG(Libretro, Warning, TEXT("Failed to save downloaded dll to path '%s'"), *DllSavePath);
            }

#ifdef USE_WINDOWS_FILE
            if (SzBitWithVals_Check(&db.Attribs, i))
            {
                UInt32 attrib = db.Attribs.Vals[i];
                /* p7zip stores posix attributes in high 16 bits and adds 0x8000 as marker.
                    We remove posix bits, if we detect posix mode field */
                if ((attrib & 0xF0000000) != 0)
                    attrib &= 0x7FFF;
                SetFileAttributesW((LPCWSTR) *DllSavePath, attrib);
            }
#endif
        }
    }

    SzArEx_Free(&db, &allocImp);
    File_Close(&archiveStream.file);

    if (res == SZ_ERROR_UNSUPPORTED) return "Decoder doesn't support this archive";
    else if (res == SZ_ERROR_MEM)    return "Cannot allocate memory";
    else if (res == SZ_ERROR_CRC)    return "CRC error";
    else if (res == SZ_ERROR_READ)   return "Read error";
    else if (res != SZ_OK)           return FString::Format(TEXT("Error %d"), {res});
    else                             return "";
}

void FUnrealLibretroEditorModule::StartupModule()
{
    // lzma-sdk needs to init this at some point prior to using it
    CrcGenerateTable();
    
    // Download Windows Redist dll's
    auto UnrealLibretro = IPluginManager::Get().FindPlugin("UnrealLibretro"); check(IsInGameThread());

    auto Win64BuildbotRequest = FHttpModule::Get().CreateRequest();
    Win64BuildbotRequest->SetVerb(TEXT("GET"));
    Win64BuildbotRequest->SetURL(TEXT("https://buildbot.libretro.com/nightly/windows/x86_64/RetroArch_update.7z"));

    Win64BuildbotRequest->OnProcessRequestComplete().BindLambda(
        [this, BaseDir = UnrealLibretro->GetBaseDir()](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
        {
            if (bSucceeded)
            {
                FString Temp7ZipFilePath = FPaths::Combine(BaseDir, FPaths::GetCleanFilename(HttpRequest->GetURL()));
                if (!FFileHelper::SaveArrayToFile(HttpResponse->GetContent(), *Temp7ZipFilePath))
                {
                    UE_LOG(Libretro, Warning, TEXT("Failed to save downloaded zip to path '%s'"), *Temp7ZipFilePath);
                }

                FString LibretroWin64RedistPath = FPaths::Combine(BaseDir, TEXT("Binaries/Win64/ThirdParty/libretro"));
                FString ErrorMessage = Unzip7ZipArchive(Temp7ZipFilePath, LibretroWin64RedistPath,
                    [](FString FilePathInsideArchive) { return FPaths::GetExtension(FilePathInsideArchive) == FString("dll"); });

                if (!ErrorMessage.IsEmpty())
                {
                    UE_LOG(Libretro, Warning, TEXT("Failed to extract Libretro Win64 dependencies most Libretro Cores will not work as a result (lzma-sdk: %s)"), *ErrorMessage);
                }

                IPlatformFile::GetPlatformPhysical().DeleteFile(*Temp7ZipFilePath);
            }
            else
            {
                UE_LOG(Libretro, Warning, TEXT("Failed to download redist dll's from %s"), *HttpRequest->GetURL());
            }
        });

    Win64BuildbotRequest->ProcessRequest();

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
