#include "RpgFactionSubsystem.h"

#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "SurvivalRpg/Core/Player/RpgBasePlayerState.h"

int32 URpgFactionSubsystem::GenericFactionIdToInteger(FGenericTeamId FactionId)
{
	return FactionId == FGenericTeamId::NoTeam ? INDEX_NONE : static_cast<int32>(FactionId.GetId());
}

int32 URpgFactionSubsystem::FindFactionFromObject(const UObject* TestObject) const
{
	if (!TestObject)
	{
		return INDEX_NONE;
	}

	if (const IGenericTeamAgentInterface* TeamAgent = Cast<const IGenericTeamAgentInterface>(TestObject))
	{
		return GenericFactionIdToInteger(TeamAgent->GetGenericTeamId());
	}

	const AActor* Actor = Cast<const AActor>(TestObject);
	if (!Actor)
	{
		return INDEX_NONE;
	}

	if (const ARpgBasePlayerState* RpgPlayerState = Cast<const ARpgBasePlayerState>(Actor))
	{
		return RpgPlayerState->GetTeamId();
	}

	if (const APawn* Pawn = Cast<const APawn>(Actor))
	{
		if (const ARpgBasePlayerState* RpgPlayerState = Pawn->GetPlayerState<ARpgBasePlayerState>())
		{
			return RpgPlayerState->GetTeamId();
		}

		if (const IGenericTeamAgentInterface* ControllerTeamAgent = Cast<const IGenericTeamAgentInterface>(Pawn->GetController()))
		{
			return GenericFactionIdToInteger(ControllerTeamAgent->GetGenericTeamId());
		}
	}

	if (const AController* Controller = Cast<const AController>(Actor))
	{
		if (const ARpgBasePlayerState* RpgPlayerState = Controller->GetPlayerState<ARpgBasePlayerState>())
		{
			return RpgPlayerState->GetTeamId();
		}
	}

	if (const AActor* InstigatorActor = Actor->GetInstigator())
	{
		if (InstigatorActor != Actor)
		{
			const int32 InstigatorFaction = FindFactionFromObject(InstigatorActor);
			if (InstigatorFaction != INDEX_NONE)
			{
				return InstigatorFaction;
			}
		}
	}

	if (const AActor* OwnerActor = Actor->GetOwner())
	{
		if (OwnerActor != Actor)
		{
			const int32 OwnerFaction = FindFactionFromObject(OwnerActor);
			if (OwnerFaction != INDEX_NONE)
			{
				return OwnerFaction;
			}
		}
	}

	return INDEX_NONE;
}

void URpgFactionSubsystem::FindFactionFromActor(const UObject* TestObject, bool& bIsPartOfFaction, int32& FactionId) const
{
	FactionId = FindFactionFromObject(TestObject);
	bIsPartOfFaction = FactionId != INDEX_NONE;
}

ERpgFactionComparison URpgFactionSubsystem::CompareFactions(const UObject* A, const UObject* B, int32& FactionIdA, int32& FactionIdB) const
{
	FactionIdA = FindFactionFromObject(A);
	FactionIdB = FindFactionFromObject(B);

	if (FactionIdA == INDEX_NONE || FactionIdB == INDEX_NONE)
	{
		return ERpgFactionComparison::Invalid;
	}

	return FactionIdA == FactionIdB
		? ERpgFactionComparison::SameFaction
		: ERpgFactionComparison::DifferentFactions;
}

ERpgFactionComparison URpgFactionSubsystem::CompareFactions(const UObject* A, const UObject* B) const
{
	int32 FactionIdA = INDEX_NONE;
	int32 FactionIdB = INDEX_NONE;
	return CompareFactions(A, B, FactionIdA, FactionIdB);
}

bool URpgFactionSubsystem::CanCauseDamage(const UObject* Instigator, const UObject* Target, bool bAllowSelfDamage) const
{
	if (!Instigator || !Target)
	{
		return false;
	}

	if (bAllowSelfDamage)
	{
		if (Instigator == Target)
		{
			return true;
		}

		const AActor* InstigatorActor = Cast<const AActor>(Instigator);
		const AActor* TargetActor = Cast<const AActor>(Target);
		const ARpgBasePlayerState* InstigatorPlayerState = FindRpgPlayerStateFromActor(InstigatorActor);
		const ARpgBasePlayerState* TargetPlayerState = FindRpgPlayerStateFromActor(TargetActor);
		if (InstigatorPlayerState && InstigatorPlayerState == TargetPlayerState)
		{
			return true;
		}
	}

	return CompareFactions(Instigator, Target) == ERpgFactionComparison::DifferentFactions;
}

bool URpgFactionSubsystem::IsHostile(const UObject* A, const UObject* B) const
{
	return CompareFactions(A, B) == ERpgFactionComparison::DifferentFactions;
}

const ARpgBasePlayerState* URpgFactionSubsystem::FindRpgPlayerStateFromActor(const AActor* Actor) const
{
	if (!Actor)
	{
		return nullptr;
	}

	if (const ARpgBasePlayerState* RpgPlayerState = Cast<const ARpgBasePlayerState>(Actor))
	{
		return RpgPlayerState;
	}

	if (const APawn* Pawn = Cast<const APawn>(Actor))
	{
		if (const ARpgBasePlayerState* RpgPlayerState = Pawn->GetPlayerState<ARpgBasePlayerState>())
		{
			return RpgPlayerState;
		}
	}

	if (const AController* Controller = Cast<const AController>(Actor))
	{
		if (const ARpgBasePlayerState* RpgPlayerState = Controller->GetPlayerState<ARpgBasePlayerState>())
		{
			return RpgPlayerState;
		}
	}

	if (const AActor* InstigatorActor = Actor->GetInstigator())
	{
		if (InstigatorActor != Actor)
		{
			if (const ARpgBasePlayerState* RpgPlayerState = FindRpgPlayerStateFromActor(InstigatorActor))
			{
				return RpgPlayerState;
			}
		}
	}

	if (const AActor* OwnerActor = Actor->GetOwner())
	{
		if (OwnerActor != Actor)
		{
			return FindRpgPlayerStateFromActor(OwnerActor);
		}
	}

	return nullptr;
}
