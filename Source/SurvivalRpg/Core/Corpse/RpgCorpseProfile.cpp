#include "RpgCorpseProfile.h"

#if WITH_EDITOR
#include "Engine/CollisionProfile.h"
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgCorpseProfile)

#if WITH_EDITOR
EDataValidationResult URpgCorpseProfile::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(
		Super::IsDataValid(Context),
		EDataValidationResult::Valid);

	auto AddError = [&Context, &Result](const FText& Error)
	{
		Context.AddError(Error);
		Result = EDataValidationResult::Invalid;
	};

	if (RagdollBoneName.IsNone())
	{
		AddError(NSLOCTEXT("RpgCorpse", "MissingRagdollBone", "Corpse profiles require a Ragdoll Bone Name."));
	}

	if (AnchorBoneName.IsNone())
	{
		AddError(NSLOCTEXT("RpgCorpse", "MissingAnchorBone", "Corpse profiles require an Anchor Bone Name."));
	}

	if (RagdollCollisionProfileName.IsNone())
	{
		AddError(NSLOCTEXT("RpgCorpse", "MissingCollisionProfile", "Corpse profiles require a ragdoll collision profile."));
	}
	else
	{
		FCollisionResponseTemplate CollisionTemplate;
		if (!UCollisionProfile::Get()->GetProfileTemplate(RagdollCollisionProfileName, CollisionTemplate))
		{
			AddError(FText::Format(
				NSLOCTEXT("RpgCorpse", "UnknownCollisionProfile", "Corpse collision profile '{0}' does not exist."),
				FText::FromName(RagdollCollisionProfileName)));
		}
		else if (CollisionTemplate.CollisionEnabled != ECollisionEnabled::QueryAndPhysics ||
			CollisionTemplate.ResponseToChannels.GetResponse(ECC_WorldStatic) != ECR_Block)
		{
			AddError(FText::Format(
				NSLOCTEXT(
					"RpgCorpse",
					"NonPhysicalRagdollProfile",
					"Corpse collision profile '{0}' must enable QueryAndPhysics and block WorldStatic so ragdolls cannot fall through the world."),
				FText::FromName(RagdollCollisionProfileName)));
		}
	}

	if (!FMath::IsFinite(RagdollVelocityMultiplier) || RagdollVelocityMultiplier < 0.0f)
	{
		AddError(NSLOCTEXT("RpgCorpse", "InvalidVelocityMultiplier", "Ragdoll Velocity Multiplier must be finite and non-negative."));
	}

	if (!FMath::IsFinite(MaximumRagdollSpeed) || MaximumRagdollSpeed < 0.0f)
	{
		AddError(NSLOCTEXT("RpgCorpse", "InvalidMaximumSpeed", "Maximum Ragdoll Speed must be finite and non-negative."));
	}

	if (!FMath::IsFinite(SettleDelaySeconds) || SettleDelaySeconds < 0.0f)
	{
		AddError(NSLOCTEXT("RpgCorpse", "InvalidSettleDelay", "Settle Delay must be finite and non-negative."));
	}

	if (!FMath::IsFinite(InteractionRadius) || InteractionRadius <= 0.0f)
	{
		AddError(NSLOCTEXT("RpgCorpse", "InvalidInteractionRadius", "Corpse Interaction Radius must be finite and greater than zero."));
	}

	if (!FMath::IsFinite(EmptyDespawnDelaySeconds) || EmptyDespawnDelaySeconds < 0.0f)
	{
		AddError(NSLOCTEXT("RpgCorpse", "InvalidEmptyDelay", "Empty Despawn Delay must be finite and non-negative."));
	}

	if (!FMath::IsFinite(MaximumLifetimeSeconds) || MaximumLifetimeSeconds <= 0.0f)
	{
		AddError(NSLOCTEXT("RpgCorpse", "InvalidMaximumLifetime", "Maximum Corpse Lifetime must be finite and greater than zero."));
	}
	else if (MaximumLifetimeSeconds < SettleDelaySeconds)
	{
		AddError(NSLOCTEXT("RpgCorpse", "LifetimeBeforeSettle", "Maximum Corpse Lifetime must not be shorter than Settle Delay."));
	}

	return Result;
}
#endif
