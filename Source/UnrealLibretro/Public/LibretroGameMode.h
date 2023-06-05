#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Engine/World.h"
#include "LibretroVRPawn.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/HUD.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "LibretroGameMode.generated.h"



UCLASS()
class UNREALLIBRETRO_API ALibretroGameMode : public AGameModeBase
{
    GENERATED_BODY()
    
public:
    // @todo There is probably some smarter way of determining this
    // This returns true if you launch PIE in "VR Preview" mode
    // If you start your packaged game with the flag -vr then this will also be true
    static CONSTEXPR auto ShouldStartPlayerInVRPawn = UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Classes)
    TSubclassOf<APawn> DefaultVRPawnClass = ALibretroVRPawn::StaticClass();

    ALibretroGameMode()
    {
        static ConstructorHelpers::FClassFinder<APawn> CharacterClassFinder(TEXT("/UnrealLibretro/Blueprints/LibretroCharacter"));
        DefaultPawnClass = CharacterClassFinder.Class;

        static ConstructorHelpers::FClassFinder<AHUD> HUDClassFinder(TEXT("/UnrealLibretro/Blueprints/LibretroHUD"));
        HUDClass = HUDClassFinder.Class;
    }

protected:
    virtual void InitializeHUDForPlayer_Implementation(APlayerController* NewPlayer) override
    {
        if (ShouldStartPlayerInVRPawn())
        {
            // The HUD currently displays information irrelevant to VR Players... so disable it
            NewPlayer->ClientSetHUD(nullptr);
        }
        else
        {
            NewPlayer->ClientSetHUD(HUDClass);
        }
    }

    // I could not get this procedure to be called in blueprints that's the primary reason why I wrote this class in C++
    virtual APawn* SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot) override
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        SpawnParams.Owner = NewPlayer;
        SpawnParams.Instigator = nullptr;

        auto Pawn = GetWorld()->SpawnActor<APawn>(ShouldStartPlayerInVRPawn() ? DefaultVRPawnClass : DefaultPawnClass, StartSpot->GetActorTransform(), SpawnParams);

        return Pawn;
    }
};
