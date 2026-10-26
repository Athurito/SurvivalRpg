#include "RpgCombatReactionProfile.h"

#include "Abilities/GameplayAbilityTypes.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "SurvivalRpg/Core/Character/RpgPawnData.h"
#include "SurvivalRpg/Core/Character/RpgPawnExtensionComponent.h"

namespace RpgCombatReactionProfile
{
	const AActor* ResolveAttacker(const FGameplayEventData& EventData)
	{
		const AActor* Attacker = EventData.ContextHandle.GetEffectCauser();
		if (!Attacker)
		{
			Attacker = EventData.Instigator.Get();
		}

		// Player abilities live on the player state, so their instigator is not where the attacker stands.
		if (const APlayerState* PlayerState = Cast<APlayerState>(Attacker))
		{
			return PlayerState->GetPawn();
		}
		if (const AController* Controller = Cast<AController>(Attacker))
		{
			return Controller->GetPawn();
		}
		return Attacker;
	}
}

UAnimMontage* FRpgDirectionalReactionMontages::Select(bool bAttackerBehind) const
{
	return bAttackerBehind && Back ? Back.Get() : Front.Get();
}

const URpgCombatReactionProfile* URpgCombatReactionProfile::FindForActor(const AActor* Avatar)
{
	const URpgPawnExtensionComponent* PawnExtension = URpgPawnExtensionComponent::FindPawnExtensionComponent(Avatar);
	const URpgPawnData* PawnData = PawnExtension ? PawnExtension->GetPawnData<URpgPawnData>() : nullptr;
	return PawnData ? PawnData->CombatReactionProfile.Get() : nullptr;
}

bool URpgCombatReactionProfile::IsAttackerBehind(const AActor* Avatar, const FGameplayEventData* EventData)
{
	const AActor* Attacker = EventData ? RpgCombatReactionProfile::ResolveAttacker(*EventData) : nullptr;
	if (!Avatar || !Attacker || Attacker == Avatar)
	{
		return false;
	}
	return IsLocationBehind(Avatar->GetActorLocation(), Avatar->GetActorForwardVector(), Attacker->GetActorLocation());
}

bool URpgCombatReactionProfile::IsLocationBehind(const FVector& Location, const FVector& Forward, const FVector& AttackerLocation)
{
	const FVector PlanarForward = FVector(Forward.X, Forward.Y, 0.0).GetSafeNormal();
	const FVector ToAttacker = FVector(AttackerLocation.X - Location.X, AttackerLocation.Y - Location.Y, 0.0);
	return !PlanarForward.IsZero() && FVector::DotProduct(PlanarForward, ToAttacker) < 0.0;
}
