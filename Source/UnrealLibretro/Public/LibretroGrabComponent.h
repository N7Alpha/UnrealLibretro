// Let's you grab stuff with VR controllers
// Heavily based on the GrabComponent blueprint from the UE_5 VRTemplate
// I had to port it back to an older version of UE so I figured I'd make it C++ since the blueprint version already exists

#pragma once

#include "LibretroVRInteractionInterface.h"

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "Components/SceneComponent.h"
#include "MotionControllerComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "LibretroGrabComponent.generated.h"

UENUM(BlueprintType)
enum class LibretroGrabType : uint8
{
    None,
    Free,
    Snap,
    Custom,
};

// John: I modified the code to return the grab component when these events
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGrabbed, class ULibretroGrabComponent*, GrabComponent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDropped, class ULibretroGrabComponent*, GrabComponent);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UNREALLIBRETRO_API ULibretroGrabComponent : public USceneComponent, public ILibretroVRInteractionInterface
{
    GENERATED_BODY()

public:	
    ULibretroGrabComponent()
    {
        // Tick is disabled in Class Defaults
        PrimaryComponentTick.bCanEverTick = false;
    }

    UPROPERTY(BlueprintReadWrite, AdvancedDisplay)
    UMotionControllerComponent* MotionControllerRef;

    UPROPERTY(BlueprintReadWrite)
    bool bIsHeld{false};
    
    UPROPERTY(BlueprintReadWrite)
    FRotator PrimaryGrabRelativeRotation;
    
    UPROPERTY(BlueprintReadWrite)
    bool bSimulateOnDrop{false};
    
    UPROPERTY(EditAnywhere)
    LibretroGrabType GrabType;
    
    UPROPERTY(EditAnywhere)
    UHapticFeedbackEffect_Base* OnGrabHapticEffect;

    UPROPERTY(BlueprintAssignable)
    FOnGrabbed OnGrabbed;

    UPROPERTY(BlueprintAssignable)
    FOnDropped OnDropped;

    UFUNCTION(BlueprintCallable)
    void SetShouldSimulateOnDrop()
    {
        if (auto* Parent = Cast<UPrimitiveComponent>(GetAttachParent()))
        {
            if (Parent->IsAnySimulatingPhysics())
            {
                bSimulateOnDrop = true;
            }
        }
    };

    UFUNCTION(BlueprintCallable)
    void SetPrimitiveCompPhysics(bool bSimulate) 
    {
        if (auto* Parent = Cast<UPrimitiveComponent>(GetAttachParent()))
        {
            Parent->SetSimulatePhysics(true);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("GrabComponent->SetSimulatingParent->Cast To PrimitiveComponent FAILED"));
        }
    }

    UFUNCTION(BlueprintCallable)
    bool TryGrab(UMotionControllerComponent* MotionController)
    {
        // Then 0: Try to grab
        switch (GrabType) 
        {
        case LibretroGrabType::Free:
        case LibretroGrabType::Snap: {
            SetPrimitiveCompPhysics(false);
            { // Attach Parent to Motion Controller
            
                bool bAttachSuccessful = GetAttachParent()->AttachToComponent(
                    MotionController,
                    FAttachmentTransformRules(
                        EAttachmentRule::KeepWorld,
                        EAttachmentRule::KeepWorld,
                        EAttachmentRule::KeepWorld,
                        /*bWeldSimulatedBodies = */ true
                    ));
            
                if (!bAttachSuccessful)
                {
                    // Debug Print String - Attachment failed.
                    // It's good practice to leave debug prints on conditions that are expected to be True
                    UE_LOG(LogTemp, Warning, TEXT("Attaching %s to %s FAILED - object not grabbed"), 
                        *UKismetSystemLibrary::GetDisplayName(GetAttachParent()),
                        *UKismetSystemLibrary::GetDisplayName(MotionController));
                    break;
                }
            
                bIsHeld = true;
            
                if (GrabType == LibretroGrabType::Snap)
                {
                    // Orient the held Actor to match GrabComponent's relative location

                    GetAttachParent()->SetRelativeRotation(
                        GetRelativeRotation().GetInverse(),
                        /* Sweep = */ false,
                        /* OutSweepHitResult = */ nullptr,
                        ETeleportType::TeleportPhysics);

                    GetAttachParent()->SetWorldLocation(
                        MotionController->GetComponentLocation() + (GetAttachParent()->GetComponentLocation() - GetComponentLocation()),
                        /* Sweep = */ false,
                        /* OutSweepHitResult = */ nullptr,
                        ETeleportType::TeleportPhysics);
                }
            }
            break;
        }
        case LibretroGrabType::Custom: {
            bIsHeld = true;
            break;
        }
        case LibretroGrabType::None:
        default: break;
        }

        // Then 1: then continue
        // Did the grab succeed?
        if (bIsHeld)
        {
            MotionControllerRef = MotionController;

            OnGrabbed.Broadcast(this);

            if (auto* PlayerController = UGameplayStatics::GetPlayerController(GetWorld(), MotionController->PlayerIndex))
            {
                PlayerController->PlayHapticEffect(
                    OnGrabHapticEffect, // Fine if nullptr
                    MotionController->GetTrackingSource(),
                    /* Scale = */ 1.0,
                    /* bLoop = */ false);
            }
        }

        return bIsHeld;
    }

    UFUNCTION(BlueprintCallable)
    bool TryRelease(UMotionControllerComponent* MotionController)
    {
        // Then 0: Try to release
        switch (GrabType)
        {
        case LibretroGrabType::Free:
        case LibretroGrabType::Snap: {
            // If primary hand drops, release and check if we should simulate physics
            if (bSimulateOnDrop)
            {
                SetPrimitiveCompPhysics(true);
            }
            else
            {
                // This will detach the GrabComponent's Parent from the MotionControllerComponent that it's attached to.
                // If Get Attach Parent is the Root of the Actor, the actor is detached. 
                // However, you can also attach GrabComponents to components that aren't the root of the actor.
                GetAttachParent()->DetachFromComponent(
                    FDetachmentTransformRules(
                        EDetachmentRule::KeepWorld,
                        EDetachmentRule::KeepWorld,
                        EDetachmentRule::KeepWorld,
                        /* bCallModify = */ true
                    ));
            }

            // fallthrough
        }
        case LibretroGrabType::Custom: bIsHeld = false;
        case LibretroGrabType::None:
        default: break;
        }

        // Then 1: then continue
        bool bReleased = !bIsHeld;
        if (bReleased)
        {
            OnDropped.Broadcast(this);
        }

        return bReleased;
    }

    // ILibretroVRInteractionInterface -- John: More or less this amounts to forwarding trigger input to held actor
    void TriggerAxis_Implementation(float Scale) override 
    {
        if (auto* Owner = Cast<ILibretroVRInteractionInterface>(GetOwner()))
        {
            Owner->TriggerAxis(Scale);
        }
    }
    
    void TriggerPressed_Implementation() override 
    {
        if (auto* Owner = Cast<ILibretroVRInteractionInterface>(GetOwner()))
        {
            Owner->TriggerPressed();
        }
    }
    
    void TriggerReleased_Implementation() override 
    {
        if (auto* Owner = Cast<ILibretroVRInteractionInterface>(GetOwner()))
        {
            Owner->TriggerReleased();
        }
    }

protected:
    // Called when the game starts
    void BeginPlay()
    {
        Super::BeginPlay();
        
        SetShouldSimulateOnDrop();

        if (auto* Parent = Cast<UPrimitiveComponent>(GetAttachParent()))
        {
            Parent->SetCollisionProfileName("PhysicsActor"/*, bUpdateOverlaps = true*/);
        }
    }
};
