#include "RpgMantleAnchorComponent.h"

#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Character.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgMantleAnchorComponent)

URpgMantleAnchorComponent::URpgMantleAnchorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetMobility(EComponentMobility::Static);
}

UPrimitiveComponent* URpgMantleAnchorComponent::GetTraversedComponent() const
{
	AActor* Owner = GetOwner();
	if (!Owner || !IsRegistered() || Mobility != EComponentMobility::Static)
	{
		return nullptr;
	}
	UPrimitiveComponent* Result = Cast<UPrimitiveComponent>(Owner->GetRootComponent());
	if (!ColliderComponentName.IsNone())
	{
		Result = nullptr;
		TInlineComponentArray<UPrimitiveComponent*> Components(Owner);
		for (UPrimitiveComponent* Component : Components)
		{
			if (Component && Component->GetFName() == ColliderComponentName)
			{
				Result = Component;
				break;
			}
		}
	}
	return Result && Result->IsRegistered() && Result->Mobility == EComponentMobility::Static
		&& Result->IsQueryCollisionEnabled() && !Result->IsSimulatingPhysics()
		&& Result->GetCollisionResponseToChannel(ECC_GameTraceChannel1) == ECR_Block ? Result : nullptr;
}

FVector URpgMantleAnchorComponent::GetLandingLocation() const
{
	return GetComponentTransform().TransformPositionNoScale(LandingOffset);
}

bool URpgMantleAnchorComponent::IsEntryInRange(const ACharacter& Character) const
{
	const UCapsuleComponent* Capsule = Character.GetCapsuleComponent();
	if (!Capsule || !GetTraversedComponent() || GetComponentLocation().ContainsNaN()
		|| LandingOffset.ContainsNaN() || !FMath::IsFinite(MaxApproachDistance) || MaxApproachDistance <= 0.0f
		|| !FMath::IsFinite(EntryHalfWidth) || EntryHalfWidth < 0.0f
		|| !FMath::IsFinite(MinFacingDot) || MinFacingDot < 0.0f || MinFacingDot > 1.0f
		|| !FMath::IsFinite(MinHeight) || !FMath::IsFinite(MaxHeight) || MinHeight < 0.0f || MaxHeight < MinHeight
		|| FVector::DotProduct(GetUpVector(), FVector::UpVector) < 0.999f)
	{
		return false;
	}
	const FVector Feet = Character.GetActorLocation() - FVector(0, 0, Capsule->GetScaledCapsuleHalfHeight());
	const FVector RelativeFeet = GetComponentTransform().InverseTransformPositionNoScale(Feet);
	const float Height = -RelativeFeet.Z;
	// The full capsule must start on the approach side; an entry never pulls a pawn out of the obstacle.
	return RelativeFeet.X <= -Capsule->GetScaledCapsuleRadius()
		&& -RelativeFeet.X <= MaxApproachDistance
		&& FMath::Abs(RelativeFeet.Y) <= EntryHalfWidth
		&& Height >= MinHeight && Height <= MaxHeight
		&& FVector::DotProduct(Character.GetActorForwardVector().GetSafeNormal2D(), GetForwardVector().GetSafeNormal2D()) >= MinFacingDot;
}
