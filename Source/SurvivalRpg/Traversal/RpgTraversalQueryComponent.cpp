#include "RpgTraversalQueryComponent.h"

#include "Animation/AnimMontage.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "SurvivalRpg/Core/Character/RpgCharacterMoverComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgTraversalQueryComponent)

URpgTraversalQueryComponent::URpgTraversalQueryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool URpgTraversalQueryComponent::IsAnimationAllowed(const ACharacter& Character, const FRpgTraversalQueryResult& Result) const
{
	return IsAnimationAllowed(static_cast<const APawn&>(Character), Result);
}

bool URpgTraversalQueryComponent::IsAnimationAllowed(const APawn& Pawn, const FRpgTraversalQueryResult& Result) const
{
	const ACharacter* Character = Cast<ACharacter>(&Pawn);
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	const URpgCharacterMoverComponent* Mover = Character ? nullptr : Pawn.FindComponentByClass<URpgCharacterMoverComponent>();
	const bool bGrounded = Movement ? Movement->IsMovingOnGround() : Mover && Mover->IsOnGround();
	const bool bFalling = Movement ? Movement->IsFalling() : Mover && Mover->IsFalling();
	if ((!Movement && !Mover) || (Mover && (!bGrounded || Result.ActionType != 3))
		|| !Result.ChosenMontage || !FMath::IsFinite(Result.StartTime) || !FMath::IsFinite(Result.PlayRate)
		|| !FMath::IsNearlyEqual(Result.PlayRate, 1.0, 0.001) || !FMath::IsFinite(Result.ObstacleHeight)
		|| !FMath::IsFinite(Result.ObstacleDepth) || Result.StartTime < 0.0 || Result.StartTime >= Result.ChosenMontage->GetPlayLength()) return false;
	const float Speed = Pawn.GetVelocity().Size2D();
	const float SpeedTolerance = FMath::Clamp(NetworkSpeedTolerance, 0.0f, 100.0f);
	const float HeightTolerance = bFalling ? FMath::Clamp(NetworkAirborneHeightTolerance, 0.0f, 50.0f) : 3.0f;
	const TArray<FRpgTraversalAnimationEntry>* Entries = Result.ActionType == 3 ? &AllowedMantleAnimations
		: Result.ActionType == 2 && bGrounded ? &AllowedVaultAnimations
		: Result.ActionType == 1 && bGrounded ? &AllowedHurdleAnimations : nullptr;
	if (!Entries) return false;
	for (const FRpgTraversalAnimationEntry& Entry : *Entries)
	{
		if (Entry.Montage != Result.ChosenMontage || Entry.bAirborne != bFalling) continue;
		if (!FMath::IsFinite(Entry.MinHeight) || !FMath::IsFinite(Entry.MaxHeight) || Entry.MaxHeight < Entry.MinHeight
			|| !FMath::IsFinite(Entry.MinDepth) || !FMath::IsFinite(Entry.MaxDepth) || Entry.MinDepth < 0.0f || Entry.MaxDepth < Entry.MinDepth
			|| !FMath::IsFinite(Entry.MinSpeed) || !FMath::IsFinite(Entry.MaxSpeed) || Entry.MaxSpeed < Entry.MinSpeed
			|| !FMath::IsFinite(Entry.MinStartTime) || !FMath::IsFinite(Entry.MaxStartTime) || Entry.MaxStartTime < Entry.MinStartTime
			|| !FMath::IsFinite(Entry.HandoffTime) || Entry.HandoffTime <= Result.StartTime
			|| Entry.HandoffTime > Result.ChosenMontage->GetPlayLength() + 0.001f
			|| !FMath::IsFinite(Entry.MovementInputHandoffTime) || Entry.MovementInputHandoffTime < 0.0f
			|| (Entry.MovementInputHandoffTime > 0.0f && (Entry.MovementInputHandoffTime <= Result.StartTime
				|| Entry.MovementInputHandoffTime > Entry.HandoffTime))) continue;
		if (Result.ObstacleHeight >= Entry.MinHeight - HeightTolerance && Result.ObstacleHeight <= Entry.MaxHeight + HeightTolerance
			&& Result.ObstacleDepth >= Entry.MinDepth && Result.ObstacleDepth <= Entry.MaxDepth
			&& Speed >= Entry.MinSpeed - SpeedTolerance && Speed <= Entry.MaxSpeed + SpeedTolerance
			&& Result.StartTime >= Entry.MinStartTime - 0.001 && Result.StartTime <= Entry.MaxStartTime + 0.001) return true;
	}
	return false;
}
