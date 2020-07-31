// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "LibretroInputComponent.h"
#include "LibretroBlueprintFunctionLibrary.generated.h"

class AActor;
class UActorComponent;

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
	UFUNCTION(BlueprintCallable, Category = "Libretro|Util|Actor", meta = (ExpandEnumAsExecs = "Branch", DeterminesOutputType = "ComponentClass"))
	static UActorComponent* HasComponent(AActor* Actor, TSubclassOf<UActorComponent> ComponentClass, EBranchNames &Branch);

	UFUNCTION(BlueprintPure, Category = "Libretro|Util|Map", meta = (BlueprintThreadSafe))
	static TMap<FKey, ERetroInput> CombineInputMaps(const TMap<FKey, ERetroInput> &InMap1, const TMap<FKey, ERetroInput> &InMap2);
};
