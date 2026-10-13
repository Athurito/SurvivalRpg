#include "RpgMovementProfile.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgMovementProfile)

float URpgMovementProfile::GetSprintBlend(float Speed) const
{
	return SprintSpeed > RunSpeed ? FMath::Clamp((Speed - RunSpeed) / (SprintSpeed - RunSpeed), 0.f, 1.f) : 0.f;
}

float URpgMovementProfile::GetAccelerationAtSpeed(float Speed) const
{
	return FMath::Lerp(Acceleration, SprintAcceleration, GetSprintBlend(Speed));
}

float URpgMovementProfile::GetGroundFrictionAtSpeed(float Speed) const
{
	return FMath::Lerp(GroundFriction, SprintGroundFriction, GetSprintBlend(Speed));
}

bool URpgMovementProfile::ValidateConfiguration(FText& OutError) const
{
	const float Values[] = { RunSpeed, SprintSpeed, CrouchSpeed, Acceleration, SprintAcceleration, BrakingDeceleration,
		BrakingDecelerationWithInput, BrakingFrictionFactor, GroundFriction, SprintGroundFriction, GroundRotationRate,
		SprintRotationRate, AirRotationRate, AirControl, FallingBrakingDeceleration, JumpZVelocity };
	for (const float Value : Values)
	{
		if (!FMath::IsFinite(Value))
		{
			OutError = NSLOCTEXT("RpgMovementProfile", "NotFinite", "Every movement profile value must be finite.");
			return false;
		}
	}
	if (RunSpeed <= 0.f || CrouchSpeed <= 0.f || SprintSpeed <= RunSpeed)
	{
		OutError = NSLOCTEXT("RpgMovementProfile", "Speeds", "Run and crouch speeds must be positive, and sprint speed must exceed run speed.");
		return false;
	}
	if (Acceleration <= 0.f || SprintAcceleration <= 0.f || JumpZVelocity <= 0.f)
	{
		OutError = NSLOCTEXT("RpgMovementProfile", "Positive", "Acceleration, sprint acceleration, and jump Z velocity must be positive.");
		return false;
	}
	if (BrakingDeceleration < 0.f || BrakingDecelerationWithInput < 0.f || BrakingFrictionFactor < 0.f || GroundFriction < 0.f
		|| SprintGroundFriction < 0.f || FallingBrakingDeceleration < 0.f || AirControl < 0.f || AirControl > 1.f)
	{
		OutError = NSLOCTEXT("RpgMovementProfile", "Range", "Braking, friction, and falling deceleration must not be negative, and air control must lie in 0..1.");
		return false;
	}
	return true;
}

#if WITH_EDITOR
EDataValidationResult URpgMovementProfile::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult ParentResult = Super::IsDataValid(Context);
	FText Error;
	if (!ValidateConfiguration(Error))
	{
		Context.AddError(Error);
		return EDataValidationResult::Invalid;
	}
	return ParentResult == EDataValidationResult::Invalid ? ParentResult : EDataValidationResult::Valid;
}
#endif
