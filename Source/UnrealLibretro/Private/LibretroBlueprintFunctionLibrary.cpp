#include "LibretroBlueprintFunctionLibrary.h"

#include "GameFramework/Actor.h"

#include "Engine/Engine.h"
#include "Camera/CameraComponent.h"
#include "PhysicsEngine/PhysicsSettings.h"

AActor* ULibretroBlueprintFunctionLibrary::LookingAtActor(UCameraComponent* CameraComponent, EBranchNames& Branch)
{
	UWorld* const World = GEngine->GetWorldFromContextObjectChecked(CameraComponent);
	FVector const Start = CameraComponent->GetComponentLocation();
	FVector const End = Start + 300 * CameraComponent->GetComponentRotation().Vector();
	FHitResult    OutHit;
	World->LineTraceSingleByChannel(OutHit,
		Start,
		End,
		ECollisionChannel::ECC_Visibility);
	
	Branch = OutHit.GetActor() ? EBranchNames::Yes : EBranchNames::No;
	return   OutHit.GetActor();
}

UActorComponent* ULibretroBlueprintFunctionLibrary::HasComponent(AActor* Actor, TSubclassOf<UActorComponent> ComponentClass, EBranchNames& Branch)
{
	auto Component = Actor->GetComponentByClass(ComponentClass);
	Branch = Component ? EBranchNames::Yes : EBranchNames::No;
	return Component;
}

bool ULibretroBlueprintFunctionLibrary::IsSupportUVFromHitResultsEnabledInConfig()
{
	return UPhysicsSettings::Get()->bSupportUVFromHitResults;
}