#pragma once

#include "CoreMinimal.h"
#include "GenericTeamAgentInterface.h"
#include "Subsystems/WorldSubsystem.h"
#include "RpgFactionSubsystem.generated.h"

class ARpgBasePlayerState;

UENUM(BlueprintType)
enum class ERpgFactionComparison : uint8
{
	SameFaction,
	DifferentFactions,
	Invalid
};

/**
 * Gameplay-facing wrapper around Unreal's GenericTeamId.
 *
 * The project keeps GenericTeamId internally for Lyra-style compatibility, while
 * combat and Blueprint-facing code can talk in terms of factions.
 */
UCLASS()
class SURVIVALRPG_API URpgFactionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Rpg|Faction", meta = (Keywords = "Team"))
	int32 FindFactionFromObject(const UObject* TestObject) const;

	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Rpg|Faction", meta = (Keywords = "Team"))
	void FindFactionFromActor(const UObject* TestObject, bool& bIsPartOfFaction, int32& FactionId) const;

	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Rpg|Faction", meta = (Keywords = "Team"))
	ERpgFactionComparison CompareFactions(const UObject* A, const UObject* B, int32& FactionIdA, int32& FactionIdB) const;

	ERpgFactionComparison CompareFactions(const UObject* A, const UObject* B) const;

	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Rpg|Faction", meta = (Keywords = "Team"))
	bool CanCauseDamage(const UObject* Instigator, const UObject* Target, bool bAllowSelfDamage = false) const;

	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Rpg|Faction", meta = (Keywords = "Team Hostile"))
	bool IsHostile(const UObject* A, const UObject* B) const;

private:
	static int32 GenericFactionIdToInteger(FGenericTeamId FactionId);
	const ARpgBasePlayerState* FindRpgPlayerStateFromActor(const AActor* Actor) const;
};
