// Port of VRPawn blueprint from UE_5 VRTemplate
// @todo Menu stuff

#pragma once

#include "LibretroGrabComponent.h"

#include "CoreMinimal.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/Engine.h"
#include "Engine/StaticMeshActor.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameFramework/Pawn.h"
#include "Components/SplineMeshComponent.h"
#include "NavigationSystem.h"
#include "Components/InputComponent.h"
#include "Camera/CameraComponent.h"
#include "MotionControllerComponent.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "LibretroVRPawn.generated.h"


UCLASS()
class UNREALLIBRETRO_API ALibretroVRPawn : public APawn
{
    GENERATED_BODY()

public:
    ALibretroVRPawn()
    {
        PrimaryActorTick.bCanEverTick = false;

        static ConstructorHelpers::FClassFinder<AActor> VRTeleportVisualizerClassFinder(TEXT("/UnrealLibretro/Blueprints/LibretroVRTeleportVisualizer"));
        TeleportVisualizerClass = VRTeleportVisualizerClassFinder.Class;

        static ConstructorHelpers::FObjectFinder<UStaticMesh> BeamMeshFinder(TEXT("/UnrealLibretro/Mesh/BeamMesh"));
        SplineMesh = BeamMeshFinder.Object;

        auto* DefaultRootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultSceneRoot"));
        SetRootComponent(DefaultRootComponent);

        Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
        MotionControllerLeft = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("MotionControllerLeft"));
        MotionControllerRight = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("MotionControllerRight"));
        
        MotionControllerLeft->MotionSource = "Left";
        MotionControllerRight->MotionSource = "Right";

        // Controllers not displayed on 5.4 @todo
#if    ENGINE_MAJOR_VERSION <  5 \
    || ENGINE_MINOR_VERSION <  4
        MotionControllerLeft->bDisplayDeviceModel = true;
        MotionControllerRight->bDisplayDeviceModel = true;
#endif
        
#if    ENGINE_MAJOR_VERSION >  4 \
    || ENGINE_MINOR_VERSION >= 26
        MotionControllerLeftAim = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("MotionControllerLeftAim"));
        MotionControllerRightAim = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("MotionControllerRightAim"));
        MotionControllerLeftAim->MotionSource = "LeftAim";
        MotionControllerRightAim->MotionSource = "RightAim";
        MotionControllerLeftAim->SetupAttachment(DefaultRootComponent);
        MotionControllerRightAim->SetupAttachment(DefaultRootComponent);
#else
        MotionControllerLeftAim = MotionControllerLeft;
        MotionControllerRightAim = MotionControllerRight;
#endif

        Camera->SetupAttachment(DefaultRootComponent);
        MotionControllerLeft->SetupAttachment(DefaultRootComponent);
        MotionControllerRight->SetupAttachment(DefaultRootComponent);
    }

    UPROPERTY(BlueprintReadWrite, Category = "Libretro")
    FVector ProjectedTeleportLocation;

    UPROPERTY(BlueprintReadWrite, Category = "Libretro")
    bool bValidTeleportLocation = false;

    UPROPERTY(BlueprintReadWrite, Category = "Libretro")
    bool bTeleportTraceActive = false;

    UPROPERTY(BlueprintReadWrite, Category = "Libretro")
    float GrabRadiusFromGripPosition = 6.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Libretro")
    float AxisDeadzoneThreshold = 0.7f;

    UPROPERTY(EditAnywhere, Category = "Libretro")
    TSubclassOf<AActor> TeleportVisualizerClass = AActor::StaticClass();

    UPROPERTY(BlueprintReadWrite, Category = "Libretro")
    AActor* TeleportVisualizerReference = nullptr;

    UPROPERTY(BlueprintReadWrite, Category = "Libretro")
    float SnapTurnDegrees = -45.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Libretro")
    TArray<FVector> TeleportTracePathPositions;

    // This is a pool of spline mesh components we've created so far not all of them are always used
    UPROPERTY(EditAnywhere, Category = "Libretro")
    TArray<USplineMeshComponent*> TeleportTraceSplineMeshComponents;

    // You can change Query Extent to get different  results, a low value will "help" the player find a teleport location
    UPROPERTY(BlueprintReadWrite, Category = "Libretro")
    FVector TeleportProjectPointToNavigationQueryExtent;

    //UPROPERTY(BlueprintReadWrite)
    //AMenu

    UPROPERTY(EditAnywhere, Category = "Libretro")
    UCameraComponent* Camera;

    UPROPERTY(EditAnywhere, Category = "Libretro")
    UMotionControllerComponent* MotionControllerLeft;

    UPROPERTY(EditAnywhere, Category = "Libretro")
    UMotionControllerComponent* MotionControllerRight;

    UPROPERTY(EditAnywhere, Category = "Libretro")
    UMotionControllerComponent* MotionControllerLeftAim;
    
    UPROPERTY(EditAnywhere, Category = "Libretro")
    UMotionControllerComponent* MotionControllerRightAim;

    UPROPERTY(EditAnywhere, Category = "Libretro")
    UStaticMesh* SplineMesh;

    UPROPERTY()
    ULibretroGrabComponent* HeldComponentLeft = nullptr;

    UPROPERTY()
    ULibretroGrabComponent* HeldComponentRight = nullptr;

protected:
    struct FHand
    {
        decltype(HeldComponentLeft)& HeldComponent;
        decltype(MotionControllerLeft)& MotionController;
        decltype(MotionControllerLeftAim)& MotionControllerAim;
    };

    constexpr FHand GetHand(EControllerHand ControllerHand)
    {
        if (ControllerHand == EControllerHand::Left)
        {
            return { HeldComponentLeft,  MotionControllerLeft,  MotionControllerLeftAim };
        }
        else 
        {
            return { HeldComponentRight, MotionControllerRight, MotionControllerRightAim };
        }
    }

    constexpr FHand GetOtherHand(EControllerHand ControllerHand)
    {
        if (ControllerHand == EControllerHand::Left)
        {
            return GetHand(EControllerHand::Right);
        }
        else
        {
            return GetHand(EControllerHand::Left);
        }
    }

    void BeginPlay() override
    {
        Super::BeginPlay();

        // Begin Play - Set Tracking Origin to floor
        if (UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled())
        {
            UHeadMountedDisplayFunctionLibrary::SetTrackingOrigin(
#if    ENGINE_MAJOR_VERSION == 5 \
    && ENGINE_MINOR_VERSION >= 4
                EHMDTrackingOrigin::LocalFloor
#else
                EHMDTrackingOrigin::Floor
#endif
            );
        }
    }

public:
    ULibretroGrabComponent* GetGrabComponentNearMotionController(UMotionControllerComponent* MotionController)
    {
        ULibretroGrabComponent* NearestGrabComponent = nullptr;
        FVector GripPosition = MotionController->GetComponentLocation();

        if (UWorld* World = GEngine->GetWorldFromContextObject(this, EGetWorldErrorMode::LogAndReturnNull))
        {
            TArray<FHitResult> HitResults;
            bool bHasHitResults = World->SweepMultiByChannel(
                HitResults,
                GripPosition,
                GripPosition,
                FQuat::Identity,
                ECollisionChannel::ECC_PhysicsBody,
                FCollisionShape::MakeSphere(GrabRadiusFromGripPosition),
                FCollisionQueryParams() /* bTraceComplex = false */);

            if (bHasHitResults)
            {
                double NearestComponentDistance = MAX_dbl;

                for (const FHitResult& HitResult : HitResults)
                {
                    if (AActor* HitActor = HitResult.GetActor())
                    {
                        for (UActorComponent* Component : HitActor->GetComponents())
                        {
                            if (ULibretroGrabComponent* GrabComponent = Cast<ULibretroGrabComponent>(Component))
                            {
                                double GrabComponentDistance = (GrabComponent->GetComponentLocation() - GripPosition).Size();
                                if (GrabComponentDistance < NearestComponentDistance)
                                {
                                    NearestGrabComponent = GrabComponent;
                                    NearestComponentDistance = GrabComponentDistance;
                                }
                            }
                        }
                    }
                }
            }
        }

        return NearestGrabComponent;
    }

    // Invoke ILibretroVRInteractionInterface calls to the AActor that own's the ULibretroGrabComponent
    template<EControllerHand TControllerHand, bool bPressed>
    void Trigger()
    {
        auto Hand = GetHand(TControllerHand);
        if (Hand.HeldComponent)
        {
            if (Hand.HeldComponent->GetOwner()->Implements<ULibretroVRInteractionInterface>())
            {
                if (bPressed) ILibretroVRInteractionInterface::Execute_TriggerPressed (Hand.HeldComponent->GetOwner());
                else          ILibretroVRInteractionInterface::Execute_TriggerReleased(Hand.HeldComponent->GetOwner());
            }
        }
    }

    template<EControllerHand TControllerHand>
    void TriggerAxis(float Val)
    {
        auto Hand = GetHand(TControllerHand);
        if (Hand.HeldComponent)
        {
            if (Hand.HeldComponent->GetOwner()->Implements<ULibretroVRInteractionInterface>())
            {
                ILibretroVRInteractionInterface::Execute_TriggerAxis(Hand.HeldComponent->GetOwner(), Val);
            }
        }
    }

    template<EControllerHand TControllerHand, bool bPressed>
    void Grab()
    {
        auto Hand = GetHand(TControllerHand);

        if (bPressed)
        {
            if (auto* GrabComponent = GetGrabComponentNearMotionController(Hand.MotionController))
            {
                // John: This bias makes it so we don't aim up on Unreal Engine 4.26 or later since Epic 
                // changed how UMotionControllerComponent::MotionSource behaved in that version
                FRotator OrientationAimBias = FRotator(Hand.MotionController->GetRelativeRotation().Quaternion().Inverse() * Hand.MotionControllerAim->GetRelativeRotation().Quaternion());
                if (GrabComponent->TryGrab(Hand.MotionController, OrientationAimBias))
                {
                    Hand.HeldComponent = GrabComponent;

                    // If other hand was holding this component, clear our reference to it
                    if (HeldComponentLeft == HeldComponentRight)
                    {
                        GetOtherHand(TControllerHand).HeldComponent = nullptr;
                    }
                }
            }
        }
        else 
        {
            if (   Hand.HeldComponent != nullptr
                && Hand.HeldComponent->TryRelease(Hand.MotionController))
            {
                Hand.HeldComponent = nullptr;
            }
        }
        
    }

protected: bool bCanSnapTurn = true;
public:
    void SnapTurn(float Val)
    {
        bool bPassedDeadZone = FMath::Abs(Val) > AxisDeadzoneThreshold;
        if (bPassedDeadZone && bCanSnapTurn)
        {
            float YawDelta = FMath::Sign(-Val) * SnapTurnDegrees;

            FVector    CameraLocation = Camera->GetComponentLocation();
            FTransform CameraRelativeTransform = Camera->GetRelativeTransform();

            FTransform NewTransform{UKismetMathLibrary::ComposeRotators(GetActorRotation(), {0.0f, YawDelta, 0.0f}), GetActorLocation()};

            AddActorWorldRotation({0.0f, YawDelta, 0.0f}, false, nullptr, ETeleportType::TeleportPhysics);
            SetActorLocation(
                /* NewLocation = */ GetActorLocation() + CameraLocation
                - (UKismetMathLibrary::ComposeTransforms(CameraRelativeTransform, NewTransform)).GetLocation(),
                /* bSweep = */ false,
                /* OutSweepHitResult = */ nullptr,
                ETeleportType::TeleportPhysics
            );
        }

        bCanSnapTurn = !bPassedDeadZone;
    }

    void TeleportTrace(FVector StartPos, FVector ForwardVector)
    {
        float TeleportLaunchSpeed = 650.0f;
        float TeleportProjectileRadius = 3.6f;
        FPredictProjectilePathResult PredictProjectilePathResult;
        UGameplayStatics::PredictProjectilePath(this, 
            {TeleportProjectileRadius, StartPos, TeleportLaunchSpeed * ForwardVector, /* MaxSimTime = */ MAX_FLT, ECC_WorldStatic},
            PredictProjectilePathResult);

        // John: This is already done in UGameplayStatics::PredictProjectilePath it seems no need to keep it
        //PredictProjectilePathResult.PathData.InsertDefaulted(0);
        //PredictProjectilePathResult.PathData[0].Location = StartPos;

#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 2
        TeleportTracePathPositions.SetNumUninitialized(PredictProjectilePathResult.PathData.Num(), EAllowShrinking::No);
#else
        TeleportTracePathPositions.SetNumUninitialized(PredictProjectilePathResult.PathData.Num(), false);
#endif
        for (uint64 i = 0; i < PredictProjectilePathResult.PathData.Num(); i++)
        {
            TeleportTracePathPositions[i] = PredictProjectilePathResult.PathData[i].Location;
        }

        FNavLocation ProjectedNavLocation;

        bool bIsValidTeleportLocation = UNavigationSystemV1::GetNavigationSystem(this)->ProjectPointToNavigation(
            PredictProjectilePathResult.HitResult.Location,
            ProjectedNavLocation,
            TeleportProjectPointToNavigationQueryExtent);

        if (bIsValidTeleportLocation)
        {
            // Debug Teleport Location
            //UKismetSystemLibrary::DrawDebugCylinder(this, 
            //    ProjectedTeleportLocation,
            //    ProjectedTeleportLocation + FVector{0.0, 0.0, 25.0f},
            //    25.0f);

            float NavMeshCellHeight = 8.0f; // John: This is a member of RecastNavMesh
            ProjectedTeleportLocation = ProjectedNavLocation.Location - FVector{0.0f, 0.0f, NavMeshCellHeight};

            // If the value hasn't changed we don't have to set it or toggle visibility
            if (bValidTeleportLocation != bIsValidTeleportLocation)
            {
                bValidTeleportLocation = bIsValidTeleportLocation;
                TeleportVisualizerReference->GetRootComponent()->SetVisibility(bValidTeleportLocation, /* bPropagateToChildren = */ true);
            }
        }
        
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 2
        TeleportTraceSplineMeshComponents.SetNum(FMath::Max(TeleportTraceSplineMeshComponents.Num(), TeleportTracePathPositions.Num()-1), EAllowShrinking::No);
#else
        TeleportTraceSplineMeshComponents.SetNum(FMath::Max(TeleportTraceSplineMeshComponents.Num(), TeleportTracePathPositions.Num()-1), false);
#endif
        { // John: I use spline mesh components for drawing the teleport trace rather than Niagara like the VRTemplate does
            int i = 0;
            for (; i < TeleportTracePathPositions.Num()-1; i++)
            {
                if (TeleportTraceSplineMeshComponents[i] == nullptr)
                {
                    TeleportTraceSplineMeshComponents[i] = NewObject<USplineMeshComponent>(this);

                    TeleportTraceSplineMeshComponents[i]->SetMobility(EComponentMobility::Movable);
                    TeleportTraceSplineMeshComponents[i]->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                    TeleportTraceSplineMeshComponents[i]->SetForwardAxis(ESplineMeshAxis::X, false);
                    TeleportTraceSplineMeshComponents[i]->bTickInEditor = true;
                    TeleportTraceSplineMeshComponents[i]->bCastDynamicShadow = false;
                    TeleportTraceSplineMeshComponents[i]->CastShadow = false;
                    TeleportTraceSplineMeshComponents[i]->RegisterComponent();
                }

                // Draw teleport trace as line segments
                TeleportTraceSplineMeshComponents[i]->SetVisibility(bValidTeleportLocation);
                TeleportTraceSplineMeshComponents[i]->SetStaticMesh(SplineMesh);
                TeleportTraceSplineMeshComponents[i]->SetStartAndEnd(
                    TeleportTracePathPositions[i],
                    TeleportTracePathPositions[i+1] - TeleportTracePathPositions[i],
                    TeleportTracePathPositions[i+1],
                    TeleportTracePathPositions[i+1] - TeleportTracePathPositions[i]
                );
            }
        
            // Hide leftover line segments we aren't using
            for (; i < TeleportTraceSplineMeshComponents.Num(); i++)
            {
                TeleportTraceSplineMeshComponents[i]->SetVisibility(false);
            }
        }

        TeleportVisualizerReference->SetActorLocation(ProjectedTeleportLocation);
    }

    void TeleportAxis(float Val)
    {
        if (Val > AxisDeadzoneThreshold)
        {
            if (!bTeleportTraceActive)
            {
                bTeleportTraceActive = true;

                { // StartTeleportTrace();
                    if (auto* World = GEngine->GetWorldFromContextObject(this, EGetWorldErrorMode::LogAndReturnNull))
                    {
                        TeleportVisualizerReference = World->SpawnActor<AActor>(TeleportVisualizerClass);
                    }
                }
            }

            TeleportTrace(MotionControllerRightAim->GetComponentLocation(), MotionControllerRightAim->GetForwardVector());
        }
        else
        {
            if (bTeleportTraceActive)
            {
                { // EndTeleportTrace();
                    bTeleportTraceActive = false;

                    if (TeleportVisualizerReference != nullptr)
                    {
                        TeleportVisualizerReference->Destroy();
                        TeleportVisualizerReference = nullptr;
                    }

                    for (auto* TeleportTraceSplineMeshComponent : TeleportTraceSplineMeshComponents)
                    {
                        TeleportTraceSplineMeshComponent->SetVisibility(false);
                    }
                }

                { // TryTeleport();
                    if (bValidTeleportLocation)
                    {
                        bValidTeleportLocation = false;

                        FVector CameraRelativeLocation = Camera->GetRelativeLocation();
                        CameraRelativeLocation.Z = 0.0f; // Ignore height
                        FRotator VRPawnRotation(0.0f, GetActorRotation().Yaw, 0.0f);
                        this->K2_TeleportTo(
                            ProjectedTeleportLocation - VRPawnRotation.RotateVector(CameraRelativeLocation), // Subtract HMD (Camera) for correct Pawn destination
                            VRPawnRotation);
                    }
                }
            }
        }
    }

    void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
    {
        Super::SetupPlayerInputComponent(PlayerInputComponent);

        InputComponent->BindAxis("LibretroMovementAxisLeft_X",  this, &ALibretroVRPawn::SnapTurn);
        InputComponent->BindAxis("LibretroMovementAxisRight_Y", this, &ALibretroVRPawn::TeleportAxis);

        InputComponent->BindAxis("LibretroTriggerAxisLeft",  this, &ALibretroVRPawn::TriggerAxis<EControllerHand::Left>);
        InputComponent->BindAxis("LibretroTriggerAxisRight", this, &ALibretroVRPawn::TriggerAxis<EControllerHand::Right>);
        
        
        InputComponent->BindAction("LibretroGrabLeft",  IE_Pressed,  this, &ALibretroVRPawn::Grab<EControllerHand::Left,  true>);
        InputComponent->BindAction("LibretroGrabRight", IE_Pressed,  this, &ALibretroVRPawn::Grab<EControllerHand::Right, true>);
        InputComponent->BindAction("LibretroGrabLeft",  IE_Released, this, &ALibretroVRPawn::Grab<EControllerHand::Left,  false>);
        InputComponent->BindAction("LibretroGrabRight", IE_Released, this, &ALibretroVRPawn::Grab<EControllerHand::Right, false>);

        InputComponent->BindAction("LibretroTriggerLeft",  IE_Pressed,  this, &ALibretroVRPawn::Trigger<EControllerHand::Left,  true>);
        InputComponent->BindAction("LibretroTriggerRight", IE_Pressed,  this, &ALibretroVRPawn::Trigger<EControllerHand::Right, true>);
        InputComponent->BindAction("LibretroTriggerLeft",  IE_Released, this, &ALibretroVRPawn::Trigger<EControllerHand::Left,  false>);
        InputComponent->BindAction("LibretroTriggerRight", IE_Released, this, &ALibretroVRPawn::Trigger<EControllerHand::Right, false>);
    }
};
