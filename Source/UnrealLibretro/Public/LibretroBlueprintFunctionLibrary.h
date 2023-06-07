#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "LibretroInputDefinitions.h"
#include "Templates/SubclassOf.h"
#include "Components/ActorComponent.h"

#include "LibretroBlueprintFunctionLibrary.generated.h"

class AActor;
class UCameraComponent;

UENUM(BlueprintType)
enum class EBranchNames : uint8 
{
    Yes,
    No
};


UCLASS()
class UNREALLIBRETRO_API ULibretroBlueprintFunctionLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
    
public:
    UFUNCTION(BlueprintCallable, Category = "Libretro|Util", meta = (ExpandEnumAsExecs = "Branch"))
    static AActor* LookingAtActor(class UCameraComponent* CameraComponent, EBranchNames& Branch);
    
    UFUNCTION(BlueprintCallable, Category = "Libretro|Util|Actor", meta = (ExpandEnumAsExecs = "Branch", DeterminesOutputType = "ComponentClass"))
    static UActorComponent* HasComponent(AActor* Actor, TSubclassOf<UActorComponent> ComponentClass, EBranchNames &Branch);

    UFUNCTION(BlueprintPure, Category = "Libretro|Util")
    static bool IsSupportUVFromHitResultsEnabledInConfig();

    UFUNCTION(BlueprintCallable, Category = "Libretro|Util")
    static FTransform GetPlayAreaTransform();
};
