#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "LibretroInputComponent.h"

#include "LibretroBlueprintFunctionLibrary.generated.h"

class AActor;
class UActorComponent;
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

	UFUNCTION(BlueprintPure, Category = "Libretro|Util|Map", meta = (BlueprintThreadSafe))
	static TMap<FKey, ERetroInput> CombineInputMaps(const TMap<FKey, ERetroInput> &InMap1, const TMap<FKey, ERetroInput> &InMap2);
	
	UFUNCTION(BlueprintCallable, Category = "Libretro|Util|PlayerController")
	static void SetAutoManageActiveCameraTarget(APlayerController* PlayerController, bool AutoManageActiveCameraTarget)
	{
		PlayerController->bAutoManageActiveCameraTarget = AutoManageActiveCameraTarget;
	}
};
