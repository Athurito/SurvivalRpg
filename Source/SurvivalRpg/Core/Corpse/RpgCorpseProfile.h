#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"

#include "RpgCorpseProfile.generated.h"

/**
 * Static designer tuning for a server-authoritative corpse lifecycle.
 *
 * The profile is definition data. Runtime state, timers and completed requirements live on
 * URpgCorpseLifecycleComponent and are never written back to this asset.
 */
UCLASS(BlueprintType, Const)
class SURVIVALRPG_API URpgCorpseProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	/** First simulated skeletal bone. The named bone and all of its children enter ragdoll. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Corpse|Ragdoll")
	FName RagdollBoneName = TEXT("pelvis");

	/** Skeletal bone followed by the query-only corpse interaction anchor. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Corpse|Interaction")
	FName AnchorBoneName = TEXT("pelvis");

	/** Collision profile applied to the skeletal mesh while it simulates as a corpse. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Corpse|Ragdoll")
	FName RagdollCollisionProfileName = TEXT("Ragdoll");

	/** Multiplies the server-observed movement velocity used to start local ragdoll simulation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Corpse|Ragdoll", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float RagdollVelocityMultiplier = 1.0f;

	/** Maximum replicated ragdoll start speed in centimeters per second. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Corpse|Ragdoll", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm/s"))
	float MaximumRagdollSpeed = 1200.0f;

	/** Seconds after death finish before rigid bodies sleep and corpse interactions become available. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Corpse|Lifetime", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float SettleDelaySeconds = 1.0f;

	/** Radius in centimeters used by the corpse interaction anchor and authoritative access checks. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Corpse|Interaction", meta = (ClampMin = "1.0", UIMin = "1.0", Units = "cm"))
	float InteractionRadius = 350.0f;

	/** Seconds a completed or fully looted corpse remains before it starts expiring. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Corpse|Lifetime", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float EmptyDespawnDelaySeconds = 2.0f;

	/** Hard server lifetime in seconds; it continues while inventory or harvest interactions are active. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Corpse|Lifetime", meta = (ClampMin = "0.1", UIMin = "1.0", Units = "s"))
	float MaximumLifetimeSeconds = 120.0f;

	/** Requires the owning corpse inventory to report empty before normal completion. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Corpse|Completion")
	bool bRequireInventoryEmpty = true;

	/** Server-only completion signals required in addition to the inventory gate, such as harvesting. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Corpse|Completion")
	FGameplayTagContainer RequiredExternalCompletionTags;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
