#pragma once

#include "Engine/EngineTypes.h"
#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility.h"

#include "RpgGameplayAbility_Stoneburst.generated.h"

struct FRpgHarvestRequest;

/**
 * Server-authoritative magical harvesting spell granted by GF_Harvesting_Magic/progression.
 *
 * The ability traces from the authoritative player view, validates IRpgHarvestableTarget, commits normal GAS
 * costs/cooldowns, and then asks exactly one actor or component to harvest. It never depends on a physical tool item.
 */
UCLASS(Blueprintable, meta = (DisplayName = "Stoneburst Harvesting Ability"))
class GF_HARVESTING_MAGIC_API URpgGameplayAbility_Stoneburst : public URpgGameplayAbility
{
	GENERATED_BODY()

public:
	explicit URpgGameplayAbility_Stoneburst(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Stable id saved by Quick Access and resolved to exactly one granted GAS spec. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Harvesting|Stoneburst")
	static FGameplayTag GetStoneburstAbilityId();

protected:
	//~ UGameplayAbility interface
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
	//~ End UGameplayAbility interface

	/** Called on authority after the target attempt for feature-side telemetry/orchestration. Use SuccessGameplayCue for client presentation. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Rpg|Harvesting|Stoneburst", meta = (DisplayName = "On Stoneburst Resolved"))
	void K2_OnStoneburstResolved(const FHitResult& TargetHit, bool bHarvestSucceeded);

	/** Maximum server trace distance from the controller/pawn view, in centimeters. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Stoneburst", meta = (ClampMin = "0.0", ClampMax = "10000.0", UIMin = "0.0", UIMax = "5000.0", Units = "cm"))
	float MaxRange = 1500.0f;

	/** Sweep radius around the aim ray, in centimeters. Zero performs an exact line trace. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Stoneburst", meta = (ClampMin = "0.0", ClampMax = "500.0", UIMin = "0.0", UIMax = "250.0", Units = "cm"))
	float TargetRadius = 75.0f;

	/** Collision channel used by the authoritative target sweep. Harvestable meshes must block this channel. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Stoneburst")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	/** Baseline power passed to the target for designer-owned yield and durability calculations. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Stoneburst", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float HarvestPower = 1.0f;

	/** Optional replicated cue executed by the caster ASC only after an authoritative harvest succeeds. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Stoneburst", meta = (Categories = "GameplayCue"))
	FGameplayTag SuccessGameplayCue;

private:
	bool FindHarvestTarget(const FGameplayAbilityActorInfo& ActorInfo, FHitResult& OutHit, UObject*& OutReceiver, FRpgHarvestRequest& OutRequest) const;
	static UObject* FindHarvestReceiver(AActor* HitActor);
};
