// Slate documentation is practically non-existent... Your best bet is plaintext searching for symbols and Widget Reflector.
// I try to do most stuff inline as lambda's so it's easier to follow (less ctrl+click). This can be hazardous
// if you capture the wrong type of reference in a capture list though so keep that in mind

#include "LibretroCoreInstanceDetails.h"
#include "LibretroInputDefinitions.h"
#include "LibretroSettings.h"
#include "UnrealLibretro.h"
#include "UnrealLibretroEditor.h" 

#include "CoreMinimal.h"
#include "PropertyHandle.h"
#include "DetailLayoutBuilder.h"
#include "SlateFwd.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Application/SlateApplication.h"
#include "IDetailPropertyRow.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "HttpModule.h"
#include "PlatformHttp.h"
#include "Interfaces/IHttpResponse.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"  // For FPlatformTime
#include "Misc/FileHelper.h"
#include "Templates/UnrealTemplate.h"
#if ENGINE_MAJOR_VERSION * 100 + ENGINE_MINOR_VERSION >= 501
#include "Styling/AppStyle.h"
typedef FAppStyle FStyleAlias;
#else
#include "EditorStyleSet.h"
typedef FEditorStyle FStyleAlias;
#endif
#include "ISettingsModule.h"

TSharedRef<IDetailCustomization> FLibretroCoreInstanceDetails::MakeInstance()
{
    return MakeShareable(new FLibretroCoreInstanceDetails);
}

void FLibretroCoreInstanceDetails::RefreshCoreListViews()
{
    const FText& InSearchText = SearchBox->GetText();
    CoreAlreadyDownloadedListViewRows.Empty();
    CoreAvailableOnBuildbotListViewRows.Empty();
    auto& CoreListViewDataSource = FModuleManager::GetModuleChecked<FUnrealLibretroEditorModule>("UnrealLibretroEditor").CoreListViewDataSource;
    for (auto& kv : CoreListViewDataSource)
    {
        if (kv.Key.Contains(InSearchText.ToString(), ESearchCase::IgnoreCase) || InSearchText.IsEmpty())
        {
            if (kv.Value.PlatformDownloadedBitField
                || kv.Value.DownloadsPending > 0)
            {
                this->CoreAlreadyDownloadedListViewRows.Add(MakeShared<FText>(FText::FromString(kv.Key)));
            }
            // Add to both lists if only a subset of the available cores for available platforms are in MyCores
            if (kv.Value.PlatformAvailableBitField & ~kv.Value.PlatformDownloadedBitField
                && kv.Value.DownloadsPending == 0)
            {
                this->CoreAvailableOnBuildbotListViewRows.Add(MakeShared<FText>(FText::FromString(kv.Key)));
            }
        }
    }

    this->CoreAlreadyDownloadedListView->RequestListRefresh();
    this->CoreAvailableOnBuildbotListView->RequestListRefresh();
}

void FLibretroCoreInstanceDetails::StartBuildbotBatchDownload(TSharedPtr<FText> CoreIdentifierText, ESelectInfo::Type)
{
    if (!CoreIdentifierText.IsValid()) return;
    auto WeakCoreDetails = TWeakPtr<FLibretroCoreInstanceDetails>(TSharedRef<FLibretroCoreInstanceDetails>(AsShared(), this));

    FString CoreIdentifier = CoreIdentifierText->ToString();
    auto& CoreListViewDataSource = FModuleManager::GetModuleChecked<FUnrealLibretroEditorModule>("UnrealLibretroEditor").CoreListViewDataSource;
    
    for (int PlatformIndex = 0; PlatformIndex < sizeof(CoreLibMetadata) / sizeof(CoreLibMetadata[0]); PlatformIndex++)
    {
        // Only start downloads for platforms that have cores available
        if (~CoreListViewDataSource[CoreIdentifier].PlatformAvailableBitField & (1UL << PlatformIndex)) continue;

        CoreListViewDataSource[CoreIdentifier].DownloadsPending++;
        FString CoreBuildbotFilename = CoreIdentifier + CoreLibMetadata[PlatformIndex].Extension + TEXT(".zip");
        auto BuildbotCoreDownloadRequest = FHttpModule::Get().CreateRequest();
        BuildbotCoreDownloadRequest->SetVerb(TEXT("GET"));
        BuildbotCoreDownloadRequest->SetURL(CoreLibMetadata[PlatformIndex].BuildbotPath + CoreBuildbotFilename);

        BuildbotCoreDownloadRequest->OnProcessRequestComplete().BindLambda(
            [WeakCoreDetails, PlatformIndex, CoreIdentifier](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
            {
                if (bSucceeded)
                {
                    uint8* UnzippedData;
                    size_t UnzippedDataSize;
                    const char* ErrorString = FUnrealLibretroEditorModule::UnzipArchive(HttpResponse->GetContent(),
                        &UnzippedData, &UnzippedDataSize); // @todo This can block the gamethread for really big cores

                    if (ErrorString)
                    {
                        UE_LOG(Libretro, Warning, TEXT("Unzipping of '%s' failed: %s"), *HttpRequest->GetURL(), *FString(ErrorString));
                    }
                    else
                    {
                        auto UnzippedDataArray = TArrayView<uint8>(UnzippedData, UnzippedDataSize);
                        auto CoreSavePath = FUnrealLibretroModule::ResolveCorePath(CoreLibMetadata[PlatformIndex].DistributionPath 
                            + CoreIdentifier + CoreLibMetadata[PlatformIndex].Extension);

                        if (!FFileHelper::SaveArrayToFile(UnzippedDataArray, *CoreSavePath))
                        {
                            UE_LOG(Libretro, Warning, TEXT("Failed to save downloaded core to path '%s': %s"), *HttpRequest->GetURL(), *FString(ErrorString));
                        }

                        free(UnzippedData);

                        check(IsInGameThread()); // We have to be on this thread when we update Slate
                        auto& CoreListViewDataSource = FModuleManager::GetModuleChecked<FUnrealLibretroEditorModule>("UnrealLibretroEditor").CoreListViewDataSource;
                        CoreListViewDataSource[CoreIdentifier].PlatformDownloadedBitField |= (1UL << PlatformIndex);

                        if (--CoreListViewDataSource[CoreIdentifier].DownloadsPending == 0)
                        {
                            auto CoreDetails = WeakCoreDetails.Pin();
                            if (CoreDetails.IsValid())
                            {
                                CoreDetails->RefreshCoreListViews();
                            }
                        }
                    }
                }
                else
                {
                    UE_LOG(Libretro, Warning, TEXT("Couldn't download core from %s"), *HttpRequest->GetURL());
                }
            });

        BuildbotCoreDownloadRequest->ProcessRequest();
    }

    // Move the row we started downloading to the MyCores Table
    RefreshCoreListViews();
}

static TArray<FLibretroOptionDescription> static_LibretroOptions;

static TStaticArray<TArray<FLibretroControllerDescription>, PortCount> static_ControllerDescriptions;

static bool core_environment(unsigned cmd, void* data) 
{
    switch (cmd) {
        case RETRO_ENVIRONMENT_SET_VARIABLES: {
            static_LibretroOptions = FUnrealLibretroModule::EnvironmentParseOptions((const struct retro_variable*)data);

            return true;
        }
        case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO: {
            static_ControllerDescriptions = FUnrealLibretroModule::EnvironmentParseControllerInfo((const struct retro_controller_info*)data);

            return true;
        }
    }

    return false;
}

void FLibretroCoreInstanceDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
    check(IsInGameThread()); // We access some static data, but as long as we're single threaded theres no data race here

    // This is how you get a weak pointer to this... In newer Unreal versions you can call AsSharedSubobject(this) if you want a cleaner look
    auto WeakCoreDetails = TWeakPtr<FLibretroCoreInstanceDetails>(TSharedRef<FLibretroCoreInstanceDetails>(AsShared(), this));

    // This represents a hook to the Libretro Panel and let's us customize the UI elements within it
    IDetailCategoryBuilder& LibretroCategory = DetailBuilder.EditCategory("Libretro");

    TArray<TWeakObjectPtr<UObject>> ObjectsCustomized;
    DetailBuilder.GetObjectsBeingCustomized(ObjectsCustomized);
    if (ObjectsCustomized.Num() > 1) return; // Don't customize the GUI when multiple ULibretroCoreInstances are selected
    
    LibretroCoreInstance = CastChecked<ULibretroCoreInstance>(ObjectsCustomized[0].Get());

    FString RomPathValidExtensionsDelimited; // Contains a string like "bin|rom|iso"
    if (LibretroCoreInstance.IsValid())
    {
        // This next call will probably block for some time thus making the editor lag temporarily
        // once it's in cache it should be plenty fast though and this is acceptable in the editor
        void* core_handle = FPlatformProcess::GetDllHandle(*FUnrealLibretroModule::ResolveCorePath(LibretroCoreInstance->CorePath));
        if (core_handle != nullptr) {
            void (*set_environment)(retro_environment_t) = (decltype(set_environment)) FPlatformProcess::GetDllExport(core_handle, TEXT("retro_set_environment"));
            if (set_environment != nullptr) {
                // We're hoping the core calls us immediately after setting this callback and gives us it's options
                set_environment(core_environment);
                ControllerDescriptions = MoveTemp(static_ControllerDescriptions);
            }

            retro_system_info system_info = {0};
            void (*get_system_info)(retro_system_info*) = nullptr;
            get_system_info = (decltype(get_system_info)) FPlatformProcess::GetDllExport(core_handle, TEXT("retro_get_system_info"));
            if (get_system_info != nullptr) {
                get_system_info(&system_info);
                RomPathValidExtensionsDelimited = system_info.valid_extensions;
                SelectedLibraryName = CoreLibraryName = system_info.library_name;
            }

            FPlatformProcess::FreeDllHandle(core_handle);
        }
    }

    IDetailCategoryBuilder& ControllerCategory = DetailBuilder.EditCategory("Libretro Core Controllers");

    auto &widget = ControllerCategory.AddCustomRow(FText::GetEmpty())
        .NameContent()
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("retro_system_info::library_name")))
            .Font(IDetailLayoutBuilder::GetDetailFont())
        ]
        .ValueContent()
        [
            SNew(SComboButton)
            .ButtonContent()
            [
                SNew(STextBlock)
                .Text_Lambda([this]()
                    {
                        return FText::FromString(SelectedLibraryName);
                    })
                .Font(IDetailLayoutBuilder::GetDetailFont())
            ]
            .OnGetMenuContent_Lambda([this]()
                {
                    FMenuBuilder MenuBuilder(true, nullptr);

                    if (LibretroCoreInstance.IsValid())
                    {
                        TArray<FString> Keys;
                        LibretroCoreInstance->EditorPresetControllers.GetKeys(Keys);

                        // Make the CorePath core always selectable
                        if (!LibretroCoreInstance->EditorPresetControllers.Contains(CoreLibraryName))
                        {
                            Keys.Add(CoreLibraryName);
                        }

                        for (auto &Key : Keys)
                        {
                            FUIAction ItemAction(FExecuteAction::CreateLambda([this, Key]{ SelectedLibraryName = Key; }));
                            MenuBuilder.AddMenuEntry(FText::FromString(Key), TAttribute<FText>(), FSlateIcon(), ItemAction);
                        }
                    }

                    return MenuBuilder.MakeWidget();
                })
        ];

    for (int Port = 0; Port < PortCount; Port++)
    {
        ControllerCategory.AddCustomRow(FText::GetEmpty())
#if ENGINE_MAJOR_VERSION >= 5
            .OverrideResetToDefault(FResetToDefaultOverride::Create
                (
                    FIsResetToDefaultVisible::CreateLambda([Port, this](TSharedPtr<IPropertyHandle> CoreControllerProperty)
                        {
                            return    LibretroCoreInstance.IsValid() 
                                   && SelectedLibraryName == CoreLibraryName
                                   && LibretroCoreInstance->EditorPresetControllers.Contains(SelectedLibraryName)
                                   && LibretroCoreInstance->EditorPresetControllers[SelectedLibraryName][Port].ID != RETRO_DEVICE_DEFAULT;
                        }),
                    FResetToDefaultHandler::CreateLambda([Port, this](TSharedPtr<IPropertyHandle> CoreControllerProperty)
                        {
                            if (LibretroCoreInstance.IsValid())
                            {
                                auto &EditorPresetControllersForSelectedCore = LibretroCoreInstance->EditorPresetControllers.FindChecked(SelectedLibraryName);
                                
                                // Set to unspecified default then try to set to specified one if there is one
                                EditorPresetControllersForSelectedCore[Port] = FLibretroControllerDescription();
                                for (auto& ControllerDescription : ControllerDescriptions[Port])
                                {
                                    if (ControllerDescription.ID == RETRO_DEVICE_DEFAULT)
                                    {
                                        EditorPresetControllersForSelectedCore[Port] = ControllerDescription;
                                    }
                                }

                                RemoveEditorPresetControllersForSelectedCoreIfAllAreDefault();
                            }
                        })
                    ))
#endif
            // Setting this makes it so our reset to default handlers are called I believe
            .PropertyHandleList({ DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULibretroCoreInstance, EditorPresetControllers)) })
            .NameContent()
            .MinDesiredWidth(150.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(FString::Format(TEXT("Port {0}"), { Port })))
                .Font(IDetailLayoutBuilder::GetDetailFont())
            ]
            .ValueContent()
            [
                SNew(SComboButton).IsEnabled_Lambda([this, WeakCoreDetails]() { return CoreLibraryName == SelectedLibraryName;  })
                .OnGetMenuContent_Lambda([this, WeakCoreDetails, Port]()
                    {
                        FMenuBuilder MenuBuilder(true, nullptr);

                        for (int32 j = 0; j < ControllerDescriptions[Port].Num(); j++)
                        {
                            FUIAction ItemAction(FExecuteAction::CreateLambda([WeakCoreDetails, RowPort = Port, j] 
                                {
                                    auto CoreDetails = WeakCoreDetails.Pin();

                                    // Early return if we're not actually changing selected controllers for a port
                                    if (   !CoreDetails.IsValid()
                                        || !CoreDetails->LibretroCoreInstance.IsValid()
                                        || !CoreDetails->LibretroCoreInstance->EditorPresetControllers.Contains(CoreDetails->SelectedLibraryName)
                                        && CoreDetails->ControllerDescriptions[RowPort][j].ID == RETRO_DEVICE_DEFAULT
                                        || CoreDetails->LibretroCoreInstance->EditorPresetControllers.Contains(CoreDetails->SelectedLibraryName)
                                        && CoreDetails->ControllerDescriptions[RowPort][j].ID == CoreDetails->LibretroCoreInstance->EditorPresetControllers[CoreDetails->SelectedLibraryName][RowPort].ID)
                                    {
                                        return;
                                    }

                                    FLibretroControllerDescriptions &EditorPresetControllersForCore = 
                                        CoreDetails->LibretroCoreInstance->EditorPresetControllers.FindOrAdd(CoreDetails->SelectedLibraryName);

                                    EditorPresetControllersForCore[RowPort] = CoreDetails->ControllerDescriptions[RowPort][j];

                                    // Copy over whatever special names the Core uses for the default
                                    for (int Port = 0; Port < PortCount; Port++)
                                    {
                                        if (EditorPresetControllersForCore[Port].ID != RETRO_DEVICE_DEFAULT) continue;
                                        for (auto& ControllerDescription : CoreDetails->ControllerDescriptions[Port])
                                        {
                                            if (ControllerDescription.ID == RETRO_DEVICE_DEFAULT)
                                            {
                                                EditorPresetControllersForCore[Port] = ControllerDescription;
                                            }
                                        }
                                    }

                                    CoreDetails->RemoveEditorPresetControllersForSelectedCoreIfAllAreDefault();
                                    CoreDetails->MarkEditorNeedsSave();
                                }));
                            MenuBuilder.AddMenuEntry(FText::FromString(ControllerDescriptions[Port][j].Description), TAttribute<FText>(), FSlateIcon(), ItemAction);
                        }

                        return MenuBuilder.MakeWidget();
                    })
                .ContentPadding(FMargin(2.0f, 2.0f))
                .ButtonContent()
                [
                    SNew(STextBlock)
                    .Text_Lambda([this, Port]()
                        {
                            FLibretroControllerDescription DisplayControllerDescription;

                            if (LibretroCoreInstance.IsValid())
                            {
                                auto* EditorPresetControllersForCore = LibretroCoreInstance->EditorPresetControllers.Find(SelectedLibraryName);

                                // If we have non-default controllers bound to a port
                                if (EditorPresetControllersForCore)
                                {
                                    DisplayControllerDescription = (*EditorPresetControllersForCore)[Port];
                                }
                                // Else relay the default description the core told us
                                else
                                {
                                    for (auto& ControllerDescription : ControllerDescriptions[Port])
                                    {
                                        if (ControllerDescription.ID == RETRO_DEVICE_DEFAULT)
                                        {
                                            DisplayControllerDescription = ControllerDescription;
                                        }
                                    }
                                }
                            }

                            return FText::FromString(FString::Format(TEXT("{0} [ID={1}]"), { DisplayControllerDescription.Description,
                                                                                             DisplayControllerDescription.ID }));
                        })
                    .Font(IDetailLayoutBuilder::GetDetailFont())
                ]
            ];
    }

    // Construct Libreto Core Options Panel... It's directly under the Libretro section
    auto IsOptionOverriddenByGlobalSetting = [](const FString& OptionKey, const TWeakObjectPtr<ULibretroCoreInstance>& LibretroCoreInstance)
        {
            auto* LibretroSettings = GetMutableDefault<ULibretroSettings>();
            bool OptionIsSetGlobally = LibretroSettings->GlobalCoreOptions.Contains(OptionKey);
            bool OptionIsSetLocally = LibretroCoreInstance.IsValid() && LibretroCoreInstance->EditorPresetOptions.Contains(OptionKey);
            return OptionIsSetGlobally && !OptionIsSetLocally;
        };
    IDetailCategoryBuilder& OptionsCategory = DetailBuilder.EditCategory("Libretro Core Options");
    LibretroOptions = MoveTemp(static_LibretroOptions);
    for (const FLibretroOptionDescription& Option : LibretroOptions)
    {
        // Build a single search-string that includes key, description and all values
        FString SearchString = Option.Key;
        SearchString += TEXT(" ");
        SearchString += Option.Description;
        for (const FString& Val : Option.Values)
        {
            SearchString += TEXT(" ");
            SearchString += Val;
        }
        FText FilterText = FText::FromString(SearchString);

        OptionsCategory.AddCustomRow(FilterText)
#if ENGINE_MAJOR_VERSION >= 5
                .OverrideResetToDefault(FResetToDefaultOverride::Create
                    (
                        FIsResetToDefaultVisible::CreateLambda([&Option, this](TSharedPtr<IPropertyHandle> EditorPresetOptionsOverrideProperty)
                            {
                                return LibretroCoreInstance.IsValid() && LibretroCoreInstance->EditorPresetOptions.Contains(Option.Key);
                            }),
                        FResetToDefaultHandler::CreateLambda([&Option, this](TSharedPtr<IPropertyHandle> EditorPresetOptionsOverrideProperty)
                            {
                                if (LibretroCoreInstance.IsValid() && LibretroCoreInstance->EditorPresetOptions.Contains(Option.Key))
                                {
                                    LibretroCoreInstance->EditorPresetOptions.FindAndRemoveChecked(Option.Key);
                                    MarkEditorNeedsSave();
                                }
                            })
                     ))
#endif
                // Setting this makes it so our reset to default handlers are called I believe
                .PropertyHandleList({ DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULibretroCoreInstance, EditorPresetOptions)) })
                .NameContent()
                .MinDesiredWidth(150.0f)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(Option.Description))
                    .Font(IDetailLayoutBuilder::GetDetailFont())
                ]
                .ValueContent()
                [
                SNew(SHorizontalBox)
                +SHorizontalBox::Slot()
                .FillWidth(1.0f)
                [
                    SNew(SComboButton)
                    // The following is fired when the user clicks an option to modify it this callback fills the drop down with options
                    .OnGetMenuContent_Lambda([&Option, WeakCoreDetails, this]()
                        {
                            FMenuBuilder MenuBuilder(true, nullptr);

                            for (int32 i = 0; i < Option.Values.Num(); i++)
                            {
                                // This is fired when the user clicks one of the possible options from the drop down
                                FUIAction ItemAction(FExecuteAction::CreateLambda([WeakCoreDetails, &Option, i] 
                                    {
                                        auto CoreDetails = WeakCoreDetails.Pin();
                                        if (CoreDetails.IsValid() && CoreDetails->LibretroCoreInstance.IsValid())
                                        {
                                            // If default is selected remove the key from core options since no key is implicit default
                                            if (i == FLibretroOptionDescription::DefaultOptionIndex)
                                            {
                                                if (CoreDetails->LibretroCoreInstance->EditorPresetOptions.Contains(Option.Key))
                                                {
                                                    CoreDetails->LibretroCoreInstance->EditorPresetOptions.Remove(Option.Key);
                                                    CoreDetails->MarkEditorNeedsSave();
                                                }
                                            }
                                            // Otherwise add the selected option
                                            else
                                            {
                                                CoreDetails->LibretroCoreInstance->EditorPresetOptions.Add(Option.Key) = Option.Values[i];
                                                CoreDetails->MarkEditorNeedsSave();
                                            }
                                        }
                                    }));
                                MenuBuilder.AddMenuEntry(FText::FromString(Option.Values[i]), FText::GetEmpty(), FSlateIcon(), ItemAction);
                            }

                            return MenuBuilder.MakeWidget();
                        })
                    .ContentPadding(FMargin(2.0f, 2.0f))
                    .ButtonContent()
                    [
                        SNew(STextBlock)
                        .ColorAndOpacity_Lambda([this, &Option, IsOptionOverriddenByGlobalSetting]()
                            {
                                return IsOptionOverriddenByGlobalSetting(Option.Key, LibretroCoreInstance) ?
                                    FSlateColor::UseSubduedForeground() : FSlateColor::UseForeground();
                            })
                        .Text_Lambda([WeakCoreDetails, &Option]()
                            {
                                auto CoreDetails = WeakCoreDetails.Pin();
                                if (CoreDetails.IsValid() && CoreDetails->LibretroCoreInstance.IsValid())
                                {
                                    FString* SelectedOptionString = CoreDetails->LibretroCoreInstance->EditorPresetOptions.Find(Option.Key);
                                    if (SelectedOptionString == nullptr)
                                    {
                                        return FText::FromString(Option.Values[FLibretroOptionDescription::DefaultOptionIndex]);
                                    }
                                    else
                                    {
                                        return FText::FromString(*SelectedOptionString);
                                    }
                                }
                                return FText::FromString(TEXT("Error"));
                            })
                        .Font(IDetailLayoutBuilder::GetDetailFont())
                    ]
                ]
                +SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(2.0f, 0.0f, 0.0f, 0.0f)
                .VAlign(VAlign_Center)
                    [
                        SNew(SButton)
                        .ButtonStyle(FStyleAlias::Get(), "HoverHintOnly")
                        .Visibility_Lambda([this, &Option, IsOptionOverriddenByGlobalSetting]()
                        {
                            return IsOptionOverriddenByGlobalSetting(Option.Key, LibretroCoreInstance) ?
                                EVisibility::Visible : EVisibility::Collapsed;
                        })
                        .OnClicked_Lambda([&Option]()
                        {
                            // Navigate user to the Unreal Libretro details panel in the project settings
                            FModuleManager::LoadModuleChecked<ISettingsModule>("Settings")
                                .ShowViewer("Project", "Plugins", "Unreal Libretro");
                            return FReply::Handled();
                        })
                        .ToolTipText_Lambda([&Option]()
                        {
                            auto* LibretroSettings = GetMutableDefault<ULibretroSettings>();
                            if (LibretroSettings->GlobalCoreOptions.Contains(Option.Key))
                            {
                                return FText::Format(
                                    FText::FromString("Global override ({0})"),
                                    FText::FromString(LibretroSettings->GlobalCoreOptions[Option.Key]));
                            }
                            return FText::GetEmpty();
                        })
                        [
                            SNew(SImage)
                            .Image(FStyleAlias::GetBrush("SceneOutliner.World"))
                        ]
                    ]
                ];
    }

    CorePathProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULibretroCoreInstance, CorePath));
    // Any time we modify the Core Path property we want to regenerate this entire UI since it determines our Libretro Options / Compatible cores
    CorePathProperty->SetOnPropertyValueChanged(FSimpleDelegate::CreateRaw(&DetailBuilder, &IDetailLayoutBuilder::ForceRefreshDetails));

    LibretroCategory.AddProperty("CorePath").CustomWidget()
        .NameContent()
        [
            SNew(STextBlock)
            .Text_Lambda([WeakCoreDetails]()
                {
                    FString CorePath;
                    
                    if (   WeakCoreDetails.IsValid()
                        && FPropertyAccess::Result::Success == WeakCoreDetails.Pin()->CorePathProperty->GetValueAsFormattedString(CorePath)
                        && FUnrealLibretroModule::IsCoreName(CorePath))
                    {
                        return FText::FromString(TEXT("Core Name"));
                    }
                    else
                    {
                        return FText::FromString(TEXT("Core Path"));
                    }
                })
            .Font(IDetailLayoutBuilder::GetDetailFont())
        ]
        .ValueContent()
        [  
            SAssignNew(PresentBuildbotDropdownButton, SComboButton)
            //.ToolTipText(FText::FromString(TEXT("Select a Core")))
            .OnGetMenuContent_Lambda([this, WeakCoreDetails]()
            {
                if (!WeakCoreDetails.IsValid()) return SNullWidget::NullWidget;

                FMenuBuilder MenuBuilder(true, nullptr, TSharedPtr<FExtender>(), false, &FCoreStyle::Get(), true);

                // Add a search box to the top of the dropdown
                MenuBuilder.AddWidget(
                    SNew(SBox) // Use this to coherce width
                    .WidthOverride(300.0f)
                    [
                        SAssignNew(SearchBox, SSearchBox)
                        .DelayChangeNotificationsWhileTyping(true)
                        .OnTextChanged_Lambda([this](auto) { RefreshCoreListViews(); })
                    ],
                FText(), false);

                auto GenerateTableViewRow = [this](TSharedRef<FText> Item, const TSharedRef<STableViewBase>& OwnerTable)
                { 
                    const float SupportedPlatformImageWidthAndHeight = 48.f;
                    auto& CoreListViewDataSource = FModuleManager::GetModuleChecked<FUnrealLibretroEditorModule>("UnrealLibretroEditor").CoreListViewDataSource;
                    auto& RowDataSource = CoreListViewDataSource[Item.Get().ToString()];
                    TSharedRef<SHorizontalBox> SupportedPlatformsImageList = SNew(SHorizontalBox);

                    bool bForBuildbot = &OwnerTable.Get() == this->CoreAvailableOnBuildbotListView.Get(); //OwnerTable->GetId() == this->CoreAvailableOnBuildbotListView->GetId();
                    for (int PlatformIndex = 0; PlatformIndex < sizeof(CoreLibMetadata) / sizeof(CoreLibMetadata[0]); PlatformIndex++)
                    {
                        if ( bForBuildbot && ~RowDataSource.PlatformAvailableBitField  & (1UL << PlatformIndex)) continue;
                        if (!bForBuildbot && ~RowDataSource.PlatformDownloadedBitField & (1UL << PlatformIndex)) continue;

                        SupportedPlatformsImageList->AddSlot()
                            .MaxWidth(SupportedPlatformImageWidthAndHeight)
                            [
                                SNew(SImage)
                                .Image(FStyleAlias::GetBrush(CoreLibMetadata[PlatformIndex].ImageName))
                                .ToolTipText(FText::FromString(bForBuildbot ?              CoreLibMetadata[PlatformIndex].BuildbotPath 
                                                                            : "MyCores/" + CoreLibMetadata[PlatformIndex].DistributionPath))
                            ];
                    }

                    FText RowText = FText::FromString(FString(RowDataSource.DownloadsPending == 0 ? "" : "Downloading... ") + Item.Get().ToString());
                    return SNew(STableRow< TSharedRef<FText> >, OwnerTable)
                    [
                        SNew(SBox) // Use this to coherce height
                        .HAlign(HAlign_Fill)
                        //.VAlign(VAlign_Center)
                        .HeightOverride(SupportedPlatformImageWidthAndHeight)
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot()
                            .VAlign(VAlign_Center)
                            [
                                SNew(STextBlock)
                                .Text(RowText)
                                .Margin(2.f)
                            ]
                            + SHorizontalBox::Slot()
                            .VAlign(VAlign_Center)
                            [
                                SupportedPlatformsImageList
                            ]
                        ]
                    ];
                };

                MenuBuilder.BeginSection("MyCoresSection", FText::FromString(TEXT("MyCores")));
                MenuBuilder.AddWidget(
                    SNew(SBox) // Use this to coherce height
                    .MaxDesiredHeight(300.f)
                    .Padding(4.f)
                    [
                        SAssignNew(CoreAlreadyDownloadedListView, SListView<TSharedRef<FText>>)
                        .ListItemsSource(&this->CoreAlreadyDownloadedListViewRows)
                        .SelectionMode(ESelectionMode::Single)
                        .OnGenerateRow_Lambda(GenerateTableViewRow)
                        .OnSelectionChanged_Lambda([WeakCoreDetails](TSharedPtr<FText> NewValue, ESelectInfo::Type)
                        {
                            if (!NewValue.IsValid())
                            {
                                UE_LOG(Libretro, Warning, TEXT("Invalid core selected. It is null. It is unknown why this occurs"));
                                return;
                            }

                            auto& CoreListViewDataSource = FModuleManager::GetModuleChecked<FUnrealLibretroEditorModule>("UnrealLibretroEditor").CoreListViewDataSource;
                            auto& RowDataSource = CoreListViewDataSource[NewValue->ToString()];

                            if (RowDataSource.DownloadsPending == 0)
                            {
                                auto CoreDetails = WeakCoreDetails.Pin();
                                if (CoreDetails.IsValid())
                                {
                                    CoreDetails->CorePathProperty->SetValueFromFormattedString(NewValue->ToString());
                                    CoreDetails->PresentBuildbotDropdownButton->SetIsOpen(false); // Close presented dropdown
                                }
                            }
                        })
                    ], FText(), false);
                
                MenuBuilder.EndSection();
                MenuBuilder.BeginSection("BuildbotLibretroComSection", FText::FromString(TEXT("buildbot.libretro.com")));

                MenuBuilder.AddWidget(
                    SNew(SBox) // Use this to coherce height
                    .MaxDesiredHeight(300.f)
                    .Padding(4.f)
                    [
                    SAssignNew(CoreAvailableOnBuildbotListView, SListView<TSharedRef<FText>>)
                        .ListItemsSource(&this->CoreAvailableOnBuildbotListViewRows)
                        .SelectionMode(ESelectionMode::Single)
                        .OnGenerateRow_Lambda(GenerateTableViewRow)
                        .OnSelectionChanged_Lambda([WeakCoreDetails](auto&&... Args)
                            {
                                auto CoreDetails = WeakCoreDetails.Pin();
                                if (CoreDetails.IsValid())
                                {
                                    CoreDetails->StartBuildbotBatchDownload(Forward<decltype(Args)>(Args)...);
                                }
                            })
                    ],
                    FText(), false
                );

                //FUIAction ItemAction(FExecuteAction::CreateLambda([]() {}));
                //MenuBuilder.AddMenuEntry(FText::FromString(TEXT("Number 9 Large")), TAttribute<FText>(), FSlateIcon(), ItemAction);
                MenuBuilder.EndSection();

                RefreshCoreListViews();

                // Copied boilerplate that focuses the keyboard on the search box when the drop down is presented
                TSharedRef<SWidget> CoreBuildbotDropdownWidget = MenuBuilder.MakeWidget();
                CoreBuildbotDropdownWidget->RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateLambda([WeakCoreDetails](double InCurrentTime, float InDeltaTime)
                    {
                        auto CoreDetails = WeakCoreDetails.Pin();
                        if (!CoreDetails.IsValid())
                        {
                            return EActiveTimerReturnType::Stop;
                        }
                        else if (CoreDetails->SearchBox.IsValid())
                        {
                            FWidgetPath WidgetToFocusPath;
                            FSlateApplication::Get().GeneratePathToWidgetUnchecked(CoreDetails->SearchBox.ToSharedRef(), WidgetToFocusPath);
                            FSlateApplication::Get().SetKeyboardFocus(WidgetToFocusPath, EFocusCause::SetDirectly);
                            WidgetToFocusPath.GetWindow()->SetWidgetToFocusOnActivate(CoreDetails->SearchBox);

                            return EActiveTimerReturnType::Stop;
                        }
                        else
                        {
                            return EActiveTimerReturnType::Continue;
                        }
                    }));


                return CoreBuildbotDropdownWidget;
            })
            .ContentPadding(FMargin(2.0f, 2.0f))
            .ButtonContent()
           [
               SNew(SEditableTextBox)
               //.MinDesiredWidth(InArgs._MinDesiredValueWidth)
               .RevertTextOnEscape(true)
               .SelectAllTextWhenFocused(true)
               .Text_Lambda([WeakCoreDetails]()
                   { 
                       //return FText::FromString(this->LibretroCoreInstance->CorePath); // This causes a segfault not sure why exactly
                       FText CorePath;
                       auto CoreDetails = WeakCoreDetails.Pin();
                       if (   CoreDetails.IsValid()
                           && FPropertyAccess::Result::Success == CoreDetails->CorePathProperty->GetValueAsDisplayText(CorePath))
                       {
                           return CorePath;
                       }
                       else
                       {
                           return FText::FromString("");
                       }
                   })
               .ToolTipText_Lambda([this]() -> FText {
                   FString CorePath;
                   if(FPropertyAccess::Result::Success == CorePathProperty->GetValueAsFormattedString(CorePath))
                   {
                       return FText::FromString(FUnrealLibretroModule::ResolveCorePath(CorePath));
                   }
                   return FText::GetEmpty();
               })
               .OnTextCommitted_Lambda([WeakCoreDetails](const FText& NewValue, ETextCommit::Type)
                   {
                       auto CoreDetails = WeakCoreDetails.Pin();
                       if (CoreDetails.IsValid() && CoreDetails->CorePathProperty->IsValidHandle())
                       {
                           CoreDetails->CorePathProperty->SetValueFromFormattedString(NewValue.ToString());
                       }
                   })
           ]
        ];

    // This section just handles displaying ROMs for the current core
    TArray<FString> RomPathValidExtensions;
    RomPathValidExtensionsDelimited.ParseIntoArray(RomPathValidExtensions, TEXT("|"), false);

    TSharedRef<IPropertyHandle> RomPathProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULibretroCoreInstance, RomPath));
    FDetailWidgetRow& RomPathWidget = LibretroCategory.AddProperty("RomPath").CustomWidget();

    FText LeftRomPathText = FText::FromString(TEXT("Rom Path [") + RomPathValidExtensionsDelimited + TEXT("]"));
    FText RomPathText;
    RomPathProperty->GetValueAsFormattedText(RomPathText);
    RomPathWidget
        .PropertyHandleList({ RomPathProperty })
        .NameContent()
        [
            SNew(STextBlock)
            .Text(LeftRomPathText)
            .ToolTipText(LeftRomPathText)
            .Font(IDetailLayoutBuilder::GetDetailFont())
        ]
        .ValueContent()
        [
            SAssignNew(PresentRomListDropdownButton, SComboButton)
            .OnGetMenuContent_Lambda([this, WeakCoreDetails, RomPathValidExtensions, RomPathProperty]()
                {
                    FMenuBuilder MenuBuilder(true, nullptr, TSharedPtr<FExtender>(), false, &FCoreStyle::Get(), true);

                    auto MyROMsPath = FUnrealLibretroModule::ResolveROMPath("");
                    TArray<FString> RomPaths;
                    {
                        double StartTime = FPlatformTime::Seconds();

                        IFileManager::Get().FindFilesRecursive(RomPaths, *MyROMsPath, TEXT("*.*"), true, false);

                        double TimeTakenSeconds = FPlatformTime::Seconds() - StartTime;

                        if (TimeTakenSeconds > 0.25)
                        {
                            UE_LOG(Libretro, Warning, TEXT("FindFilesRecursive took %g seconds to find %i files"), TimeTakenSeconds, RomPaths.Num());
                        }
                    }

                    // Remove all ROMs with an incompatible extension... FilterByPredicate is O(n) and simple
                    RomPaths = RomPaths.FilterByPredicate([&](FString RomPath) {
                        return RomPathValidExtensions.ContainsByPredicate([&](FString ValidExtension) {
                            return ValidExtension.Compare(FPaths::GetExtension(RomPath), ESearchCase::IgnoreCase) == 0; });
                        });

                    // Make paths relative instead of absolute
                    for (int i = 0; i < RomPaths.Num(); i++)
                    {
                        FString PathPart = FPaths::GetPath(RomPaths[i]).RightChop(MyROMsPath.Len());
                        FString RomRelativePath;
                        
                        // If PathPart is empty, the ROM is at the root level
                        if (PathPart.IsEmpty())
                        {
                            RomRelativePath = FPaths::GetCleanFilename(RomPaths[i]);
                        }
                        else
                        {
                            RomRelativePath = PathPart + TEXT("/") + FPaths::GetCleanFilename(RomPaths[i]);
                        }
                        
                        RomPaths[i] = MoveTemp(RomRelativePath);
                        continue;
                    }

                    RomPathsText.Empty();
                    for (auto& Path : RomPaths)
                    {
                        RomPathsText.Add(MakeShared<FText>(FText::FromString(Path)));
                    }

                    FilteredRomPathsText = RomPathsText; // Initially, both lists are the same.

                    // Add a search box
                    MenuBuilder.AddWidget(
                        SNew(SBox) // Use this to coherce width
                        .WidthOverride(300.0f)
                        [
                            SAssignNew(SearchBox, SSearchBox)
                            .DelayChangeNotificationsWhileTyping(true)
                            .OnTextChanged_Lambda([this](const FText& SearchText)
                            {
                                FilteredRomPathsText.Empty();
                                if (RomPathsListView.IsValid())
                                {
                                    for (const auto& Path : RomPathsText)
                                    {
                                        if (   Path->ToString().Contains(SearchText.ToString())
                                            || SearchText.ToString().IsEmpty())
                                        {
                                            FilteredRomPathsText.Add(Path);
                                        }
                                    }
                                }

                                UE_LOG(Libretro, Warning, TEXT("Search text size %i files %i"), RomPathsText.Num(), FilteredRomPathsText.Num());
                                RomPathsListView->RequestListRefresh();
                            })
                        ],
                        FText(), false
                    );
                    
                    MenuBuilder.AddWidget(
                        SNew(SBox) // Use this to coherce height
                        .MaxDesiredHeight(300.f)
                        .Padding(4.f)
                        [
                            SAssignNew(RomPathsListView, SListView<TSharedRef<FText>>)
                            .ListItemsSource(&FilteredRomPathsText)
                            .SelectionMode(ESelectionMode::Single)
                            .OnGenerateRow_Lambda([this](TSharedRef<FText> Item, const TSharedRef<STableViewBase>& OwnerTable)
                                {
                                    return SNew(STableRow< TSharedRef<FText> >, OwnerTable)
                                           [
                                               SNew(SBox)
                                               .HAlign(HAlign_Fill)
                                               //.VAlign(VAlign_Center)
                                               .HeightOverride(30.f)
                                               [
                                                   SNew(STextBlock)
                                                   .Text(Item.Get())
                                                   .Margin(2.f)
                                               ]
                                           ];
                                })
                            .OnSelectionChanged_Lambda([WeakCoreDetails, RomPathProperty](TSharedPtr<FText> RomSelectedPath, ESelectInfo::Type)
                                {
                                    if (!RomSelectedPath.IsValid())
                                    {
                                        UE_LOG(Libretro, Warning, TEXT("Invalid ROM selected. It is null. It is unknown why this occurs"));
                                        return;
                                    }

                                    RomPathProperty->SetValueFromFormattedString(RomSelectedPath.Get()->ToString());
                                    auto CoreDetails = WeakCoreDetails.Pin();
                                    if (CoreDetails.IsValid())
                                    {
                                        CoreDetails->PresentRomListDropdownButton->SetIsOpen(false); // Close presented Rom dropdown
                                    }
                                })
                        ], FText(), false);

                    // Copied boilerplate that focuses the keyboard on the search box when the drop down is presented
                    TSharedRef<SWidget> RomDropdownWidget = MenuBuilder.MakeWidget();
                    RomDropdownWidget->RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateLambda([WeakCoreDetails](double InCurrentTime, float InDeltaTime)
                        {
                            auto CoreDetails = WeakCoreDetails.Pin();
                            if (!CoreDetails.IsValid())
                            {
                                return EActiveTimerReturnType::Stop;
                            }
                            else if (CoreDetails->SearchBox.IsValid())
                            {
                                FWidgetPath WidgetToFocusPath;
                                FSlateApplication::Get().GeneratePathToWidgetUnchecked(CoreDetails->SearchBox.ToSharedRef(), WidgetToFocusPath);
                                FSlateApplication::Get().SetKeyboardFocus(WidgetToFocusPath, EFocusCause::SetDirectly);
                                WidgetToFocusPath.GetWindow()->SetWidgetToFocusOnActivate(CoreDetails->SearchBox);

                                return EActiveTimerReturnType::Stop;
                            }
                            else
                            {
                                return EActiveTimerReturnType::Continue;
                            }
                        }));

                    return RomDropdownWidget;
                })
            .ToolTipText(RomPathText)
            .ButtonContent()
            [
                SNew(SEditableTextBox)
                .Text(RomPathText)
                .Font(IDetailLayoutBuilder::GetDetailFont())
                .OnTextCommitted_Lambda([RomPathProperty](const FText& NewText, ETextCommit::Type CommitType)
                    {
                        RomPathProperty->SetValueFromFormattedString(NewText.ToString());
                    })
                .ForegroundColor_Lambda([RomPathProperty, RomPathValidExtensions]()
                {    
                    FString RomPath;
                    RomPathProperty->GetValueAsFormattedString(RomPath);

                    bool bExtensionIsValid = RomPathValidExtensions.ContainsByPredicate([&](FString Extension)
                        { return Extension.Compare(FPaths::GetExtension(RomPath), ESearchCase::IgnoreCase) == 0; });

                    if (bExtensionIsValid && FPaths::FileExists(FUnrealLibretroModule::ResolveROMPath(RomPath)))
                    {
                        return FSlateColor::UseForeground();
                    }

                    return FSlateColor(FLinearColor(1.0f, 0.5f, 0.0f));  
                })
            ]
        ];
}
