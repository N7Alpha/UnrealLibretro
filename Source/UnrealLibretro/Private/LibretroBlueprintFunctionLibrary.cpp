// Fill out your copyright notice in the Description page of Project Settings.
#include "LibretroBlueprintFunctionLibrary.h"

#include "GameFramework/Actor.h"

#include "Components/ActorComponent.h"
#include "Kismet/BlueprintMapLibrary.h"


UActorComponent* ULibretroBlueprintFunctionLibrary::HasComponent(AActor* Actor, TSubclassOf<UActorComponent> ComponentClass, EBranchNames& Branch)
{
	auto Component = Actor->GetComponentByClass(ComponentClass);
	Branch = Component ? EBranchNames::Yes : EBranchNames::No;
	return Component;
}

TMap<FKey, ERetroInput> ULibretroBlueprintFunctionLibrary::CombineInputMaps(const TMap<FKey, ERetroInput>& InMap1, const TMap<FKey, ERetroInput>& InMap2)
{
	auto OutMap(InMap1);
	OutMap.Append(InMap2);
	return OutMap;
}