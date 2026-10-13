#pragma once

#if WITH_DEV_AUTOMATION_TESTS

#include "RpgCharacterMovementComponent.h"
#include "RpgMovementProfile.h"
#include "UObject/Package.h"

namespace RpgMovementProfileTestTypes
{
	/**
	 * Transient profile that keeps the component's current acceleration, braking and friction without any taper, so a
	 * fixture can choose run and sprint speeds while the movement physics stay as before.
	 */
	inline URpgMovementProfile* MakeStockEquivalentProfile(const URpgCharacterMovementComponent& Movement, float RunSpeed, float SprintSpeed)
	{
		URpgMovementProfile* Profile = NewObject<URpgMovementProfile>(GetTransientPackage(), NAME_None, RF_Transient);
		Profile->RunSpeed = RunSpeed;
		Profile->SprintSpeed = SprintSpeed;
		Profile->Acceleration = Profile->SprintAcceleration = Movement.MaxAcceleration;
		Profile->BrakingDeceleration = Profile->BrakingDecelerationWithInput = Movement.BrakingDecelerationWalking;
		Profile->GroundFriction = Profile->SprintGroundFriction = Movement.GroundFriction;
		Profile->GroundRotationRate = Profile->SprintRotationRate = 0.f;
		return Profile;
	}
}

#endif
