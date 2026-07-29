#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"

#include "RpgHarvestableTarget.generated.h"

class AActor;

/** Server-authored request passed from a magical harvesting ability to a harvestable actor or component. */
USTRUCT(BlueprintType)
struct GF_HARVESTING_MAGIC_API FRpgHarvestRequest
{
	GENERATED_BODY()

	/** Pawn or actor that cast the harvesting spell. Gameplay code must not trust a client-supplied value. */
	UPROPERTY(BlueprintReadOnly, Category = "Harvesting")
	TObjectPtr<AActor> Harvester = nullptr;

	/** Stable semantic ability id, used by a target to distinguish Stoneburst from future harvesting spells. */
	UPROPERTY(BlueprintReadOnly, Category = "Harvesting", meta = (Categories = "Ability.Harvesting"))
	FGameplayTag AbilityId;

	/** Authoritative server trace origin in world space, in centimeters. */
	UPROPERTY(BlueprintReadOnly, Category = "Harvesting")
	FVector TraceOrigin = FVector::ZeroVector;

	/** Authoritative server hit selected for this harvesting request. */
	UPROPERTY(BlueprintReadOnly, Category = "Harvesting")
	FHitResult Hit;

	/**
	 * Resource revision observed by the server before ability cost/commit.
	 * Revisioned targets require an exact match; INDEX_NONE is reserved for targets without revision semantics.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Harvesting")
	int32 ExpectedRevision = INDEX_NONE;

	/** Relative spell power passed to yield/durability logic. One is the baseline Stoneburst cast. */
	UPROPERTY(BlueprintReadOnly, Category = "Harvesting", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float HarvestPower = 1.0f;
};

/** Blueprint/C++ contract implemented by resource actors or components that accept magical harvesting. */
UINTERFACE(BlueprintType)
class GF_HARVESTING_MAGIC_API URpgHarvestableTarget : public UInterface
{
	GENERATED_BODY()
};

class GF_HARVESTING_MAGIC_API IRpgHarvestableTarget
{
	GENERATED_BODY()

public:
	/** Read-only server validation performed before the ability commits its GAS cost/cooldown. Unimplemented targets reject by default. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Rpg|Harvesting")
	bool CanAcceptHarvest(const FRpgHarvestRequest& Request) const;

	/** Applies harvest state and rewards on authority. Unimplemented targets reject; implementations must not mutate on false. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Rpg|Harvesting")
	bool CommitHarvest(const FRpgHarvestRequest& Request);
};
