#pragma once

#include "LibretroCoreInstance.h"

#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "Algo/AllOf.h"
#include "Layout/Visibility.h"
#include "Widgets/SWidget.h"
#include "Widgets/Views/SListView.h"
#include "IDetailCustomization.h"
#include "Misc/NotifyHook.h"

class IPropertyHandle;
class SComboButton;

/**
 * @brief Editor custom GUI code for the LibretroCoreInstance type
 * 
 * Features:
 *  - Set Core options through editor GUI
 *  - Set controllers for each port
 *  - Download Cores from buildbot.libretro.com automatically for each platform
 *  - Toggle label between "Core Path" and "Core Name" depending on the CorePath string set
 *  - Pick compatible ROM's from dropdown for current core
 */
class FLibretroCoreInstanceDetails : public IDetailCustomization
{

public:
    /** Makes a new instance of this detail layout class for a specific detail view requesting it */
    static TSharedRef<IDetailCustomization> MakeInstance();

    TSharedPtr<IPropertyHandle> CorePathProperty;

    TArray<TSharedRef<FText>> CoreAlreadyDownloadedListViewRows;
    TArray<TSharedRef<FText>> CoreAvailableOnBuildbotListViewRows;

    TSharedPtr<SListView<TSharedRef<FText>>> CoreAlreadyDownloadedListView;
    TSharedPtr<SListView<TSharedRef<FText>>> CoreAvailableOnBuildbotListView;

    TArray<TSharedRef<FText>> RomPathsText;

    TSharedPtr<SComboButton> PresentBuildbotDropdownButton;
    TSharedPtr<SComboButton> PresentRomListDropdownButton;
    TSharedPtr<SSearchBox> SearchBox;

    void RefreshCoreListViews();
    void StartBuildbotBatchDownload(TSharedPtr<FText> CoreIdentifier, ESelectInfo::Type);

    /** IDetailCustomization interface */
    virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

    TArray<FLibretroOptionDescription> LibretroOptions;
    TStaticArray<TArray<FLibretroControllerDescription>, PortCount> ControllerDescriptions;

    TWeakObjectPtr<ULibretroCoreInstance> LibretroCoreInstance;
    FString CoreLibraryName;
    FString SelectedLibraryName;

    FORCEINLINE_DEBUGGABLE void RemoveEditorPresetControllersForSelectedCoreIfAllAreDefault()
    {
        check(LibretroCoreInstance.IsValid());

        // If all controllers were set to default no longer bookkeep bound controllers since its not necessary
        // The important side effect here is that it is reflected to the user in the gui since they'll only care
        // to view which cores are configured with non-default controllers
        if (Algo::AllOf(LibretroCoreInstance->EditorPresetControllers[SelectedLibraryName].ControllerDescription,
            [](auto& D) { return D.ID == RETRO_DEVICE_DEFAULT; }))
        {
            LibretroCoreInstance->EditorPresetControllers.Remove(SelectedLibraryName);
        }
    }
    
    FORCEINLINE_DEBUGGABLE void MarkEditorNeedsSave()
    {
        check(LibretroCoreInstance.IsValid());

#if ENGINE_MAJOR_VERSION >= 5 || ENGINE_MINOR_VERSION >= 26
        LibretroCoreInstance->GetPackage()->SetDirtyFlag(true);
#else
        LibretroCoreInstance->MarkPackageDirty();
#endif
    }
};