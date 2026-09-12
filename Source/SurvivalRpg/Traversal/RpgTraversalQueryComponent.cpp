#include "RpgTraversalQueryComponent.h"

#include "Animation/AnimMontage.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgTraversalQueryComponent)

URpgTraversalQueryComponent::URpgTraversalQueryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool URpgTraversalQueryComponent::IsAnimationAllowed(const ACharacter& Character, const FRpgTraversalQueryResult& Result) const
{
	const UCharacterMovementComponent* Movement = Character.GetCharacterMovement();
	if (!Movement || !Result.ChosenMontage || !FMath::IsFinite(Result.StartTime) || !FMath::IsFinite(Result.PlayRate)
		|| !FMath::IsNearlyEqual(Result.PlayRate, 1.0, 0.001) || !FMath::IsFinite(Result.ObstacleHeight)
		|| Result.StartTime < 0.0 || Result.StartTime >= Result.ChosenMontage->GetPlayLength()) return false;
	const float Speed = Character.GetVelocity().Size2D();
	const float SpeedTolerance = FMath::Clamp(NetworkSpeedTolerance, 0.0f, 100.0f);
	const float HeightTolerance = Movement->IsFalling() ? FMath::Clamp(NetworkAirborneHeightTolerance, 0.0f, 50.0f) : 3.0f;
	for (const FRpgTraversalAnimationEntry& Entry : AllowedMantleAnimations)
	{
		if (Entry.Montage != Result.ChosenMontage || Entry.bAirborne != Movement->IsFalling()) continue;
		if (!FMath::IsFinite(Entry.MinHeight) || !FMath::IsFinite(Entry.MaxHeight) || Entry.MaxHeight < Entry.MinHeight
			|| !FMath::IsFinite(Entry.MinSpeed) || !FMath::IsFinite(Entry.MaxSpeed) || Entry.MaxSpeed < Entry.MinSpeed
			|| !FMath::IsFinite(Entry.MinStartTime) || !FMath::IsFinite(Entry.MaxStartTime) || Entry.MaxStartTime < Entry.MinStartTime
			|| !FMath::IsFinite(Entry.HandoffTime) || Entry.HandoffTime <= Result.StartTime
			|| Entry.HandoffTime > Result.ChosenMontage->GetPlayLength() + 0.001f) continue;
		if (Result.ObstacleHeight >= Entry.MinHeight - HeightTolerance && Result.ObstacleHeight <= Entry.MaxHeight + HeightTolerance
			&& Speed >= Entry.MinSpeed - SpeedTolerance && Speed <= Entry.MaxSpeed + SpeedTolerance
			&& Result.StartTime >= Entry.MinStartTime - 0.001 && Result.StartTime <= Entry.MaxStartTime + 0.001) return true;
	}
	return false;
}
