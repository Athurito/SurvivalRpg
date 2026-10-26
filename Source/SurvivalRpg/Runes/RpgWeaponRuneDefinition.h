#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "RpgWeaponRuneDefinition.generated.h"

class UGameplayEffect;

/**
 * Echo behavior of a rune: every n-th swing that lands a hit repeats as a ghost strike, which replays that swing's
 * traces after a short delay and damages everything they hit, each target once. Designer-authored static data; the
 * server's weapon instance counts the charge and runs the strike.
 */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgEchoStrikeSettings
{
	GENERATED_BODY()

	/** Whether the rune echoes swings at all. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Echo")
	bool bEnabled = false;

	/** Swings that must land a hit to trigger, the triggering swing included. Swings into the air do not count. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Echo", meta = (EditCondition = "bEnabled", ClampMin = "1", UIMax = "10"))
	int32 HitsToTrigger = 3;

	/** Seconds after the triggering swing's attack window opened at which the ghost strike replays it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Echo", meta = (EditCondition = "bEnabled", ClampMin = "0.0", UIMax = "1.0", Units = "s"))
	float StrikeDelay = 0.2f;

	/** Seconds without a hitting swing after which the charge expires; zero keeps it until the weapon is unequipped. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Echo", meta = (EditCondition = "bEnabled", ClampMin = "0.0", Units = "s"))
	float ChargeTimeout = 5.0f;

	/** GameplayEffect each ghost hit applies; it should consume SetByCaller.Damage and SetByCaller.StaggerDamage. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Echo", meta = (EditCondition = "bEnabled"))
	TSubclassOf<UGameplayEffect> DamageEffect;

	/** Health damage per ghost hit, sent as SetByCaller.Damage; independent of the triggering swing. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Echo", meta = (EditCondition = "bEnabled", ClampMin = "0.0"))
	float Damage = 25.0f;

	/** Stagger damage per ghost hit, sent as SetByCaller.StaggerDamage. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Echo", meta = (EditCondition = "bEnabled", ClampMin = "0.0"))
	float StaggerDamage = 0.0f;

	/** Damage type tags appended to each ghost hit, for example Damage.Type.Melee so blocks apply. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Echo", meta = (EditCondition = "bEnabled", Categories = "Damage.Type"))
	FGameplayTagContainer DamageTypeTags;

	/** Impact cue of a ghost hit, as an attack's HitCueTag; the damage execution plays it for everyone. Empty plays none. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Echo", meta = (EditCondition = "bEnabled", Categories = "GameplayCue.Combat.Hit"))
	FGameplayTag HitCueTag;

	/**
	 * Cue the server executes on the owner when the strike triggers, for the flare and the afterimage on every machine.
	 * Its SourceObject is the weapon actor, RawMagnitude how many seconds the ghost runs behind the swing (at least the
	 * strike delay), and NormalizedMagnitude how many seconds from now the ghost plays. Empty plays none.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Echo", meta = (EditCondition = "bEnabled", Categories = "GameplayCue"))
	FGameplayTag StrikeCueTag;
};

/**
 * One rune that can sit on a weapon. Its effects are optional settings blocks the weapon instance runs natively, so a
 * new rune is a new asset; a new kind of effect adds a block here. Designer-authored static data.
 */
UCLASS(BlueprintType, Const)
class SURVIVALRPG_API URpgWeaponRuneDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Player-facing name. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rune")
	FText DisplayName;

	/** Player-facing description of what the rune does. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rune", meta = (MultiLine = true))
	FText Description;

	/** Every n-th hitting swing repeats as a ghost strike. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects")
	FRpgEchoStrikeSettings EchoStrike;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
