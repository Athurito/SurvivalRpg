#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RpgCombatReactionProfile.generated.h"

class AActor;
class UAnimMontage;
struct FGameplayEventData;

/**
 * Montages of one heavy combat reaction, chosen by the side the attacker stands on relative to the reacting
 * character's facing. Designer-authored static presentation data; the server picks the montage and the ability
 * system replicates its playback.
 */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgDirectionalReactionMontages
{
	GENERATED_BODY()

	/** Played when the attacker stands in front of the character or is unknown. Null keeps the ability's own default. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction")
	TObjectPtr<UAnimMontage> Front = nullptr;

	/** Played when the attacker stands behind the character. Null falls back to Front. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction")
	TObjectPtr<UAnimMontage> Back = nullptr;

	/** Back for an attacker behind when it is set, otherwise Front; null when neither is set. */
	UAnimMontage* Select(bool bAttackerBehind) const;
};

/**
 * Static montages for the heavy combat reactions of one pawn type: stagger, guard break and death.
 *
 * PawnData references the profile, so every enemy type plays reactions authored for its own body while the shared
 * stagger and death abilities keep their lifecycle. Presentation only: stagger duration, immunity and the death and
 * corpse lifecycle stay with the abilities, the defense attributes and the health component. The server reads it when
 * those server-only abilities activate.
 */
UCLASS(BlueprintType, Const)
class SURVIVALRPG_API URpgCombatReactionProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Montages when the stagger meter breaks outside a block. The stagger ability stops them after its duration, so a looping last section is fine. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reactions")
	FRpgDirectionalReactionMontages Stagger;

	/** Montages when the meter breaks while the character blocks (guard break). An empty side falls back to Stagger. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reactions")
	FRpgDirectionalReactionMontages GuardBreak;

	/** Death montages. They should hold their last pose until the corpse ragdoll takes over. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reactions")
	FRpgDirectionalReactionMontages Death;

	/** The profile on the PawnData of Avatar's pawn extension, or null. */
	static const URpgCombatReactionProfile* FindForActor(const AActor* Avatar);

	/**
	 * True when the event's attacker stands behind Avatar's facing; false when it is unknown. Uses the effect causer of
	 * the event's context, else its instigator, and resolves a player state or controller to its pawn.
	 */
	static bool IsAttackerBehind(const AActor* Avatar, const FGameplayEventData* EventData);

	/** True when AttackerLocation lies behind the plane through Location along the planar Forward direction. */
	static bool IsLocationBehind(const FVector& Location, const FVector& Forward, const FVector& AttackerLocation);
};
