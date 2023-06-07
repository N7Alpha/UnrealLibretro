#include "LibretroBlueprintFunctionLibrary.h"

#include "GameFramework/Actor.h"

#include "Engine/Engine.h"
#include "Camera/CameraComponent.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "Runtime/Launch/Resources/Version.h"

#if    ENGINE_MAJOR_VERSION != 5 \
    || ENGINE_MINOR_VERSION <  1
#include "OculusFunctionLibrary.h"
#else
#include "HeadMountedDisplayFunctionLibrary.h"
#endif

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

FTransform ULibretroBlueprintFunctionLibrary::GetPlayAreaTransform()
{
	FTransform Transform;

#if    ENGINE_MAJOR_VERSION == 5 \
    && ENGINE_MINOR_VERSION >= 1
	FVector2D Rect;
	UHeadMountedDisplayFunctionLibrary::GetPlayAreaRect(Transform, Rect);
	Transform.SetScale3D({Rect.Y / 100.0, Rect.X / 100.0, 1.0}); // The cube we're scaling is 100 cm^3 which is why we divide by 100 since I'm assuming that's the units we're given
																 // I'm not really sure why the coordinates are swapped maybe OpenXR uses a different axes convention?
#else
	Transform = UOculusFunctionLibrary::GetPlayAreaTransform();
#endif

	return Transform;
}