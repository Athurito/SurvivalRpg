#include "RpgWeaponSwingRecord.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgWeaponSwingRecord)

int32 FRpgWeaponSwingRecord::FindDueEnd(const int32 FirstIndex, const float Elapsed) const
{
	int32 Index = FMath::Clamp(FirstIndex, 0, Traces.Num());
	while (Index < Traces.Num() && Traces[Index].TimeOffset <= Elapsed)
	{
		++Index;
	}
	return Index;
}

bool FRpgEchoStrikeCharge::RegisterSwing(const bool bLandedHit, const int32 HitsToTrigger, const float ChargeTimeout, const float Now)
{
	Expire(ChargeTimeout, Now);
	if (!bLandedHit)
	{
		return false;
	}

	++Charge;
	LastChargeTime = Now;
	if (Charge >= FMath::Max(1, HitsToTrigger))
	{
		Charge = 0;
		return true;
	}
	return false;
}

bool FRpgEchoStrikeCharge::Expire(const float ChargeTimeout, const float Now)
{
	if (Charge > 0 && ChargeTimeout > 0.0f && Now - LastChargeTime > ChargeTimeout)
	{
		Charge = 0;
		return true;
	}
	return false;
}
