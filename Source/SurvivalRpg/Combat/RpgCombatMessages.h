#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "RpgCombatMessages.generated.h"

class AActor;
class UGameplayEffect;

USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgCombatActorKilledMessage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	TObjectPtr<AActor> Victim = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	TObjectPtr<AActor> Killer = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	TObjectPtr<AActor> DamageCauser = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	float DamageMagnitude = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	bool bWasSelfDestruct = false;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	FGameplayTagContainer SourceTags;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	FGameplayTagContainer TargetTags;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	TSubclassOf<UGameplayEffect> DamageEffect;
};

/**
 * Presentation message on Rpg.Combat.Message.RangedDrawingChanged, local to each machine: the avatar's ranged shot
 * started or stopped showing as nocked or drawn there (URpgAbilitySystemComponent::IsRangedDrawingForPresentation). It
 * follows the owner's own prediction on its machine and the replicated State.Ranged.Drawing everywhere else, and it
 * repeats the current draw when an avatar initializes, so a late joiner shows a draw that began before it arrived.
 * Cosmetic: listeners such as a bow's nocked arrow present it; never use it for gameplay.
 */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgRangedDrawingMessage
{
	GENERATED_BODY()

	/** The pawn whose draw changed, the avatar of the sending ability system. Match it against an equipment actor's owner. */
	UPROPERTY(BlueprintReadOnly, Category = "Combat|Ranged")
	TObjectPtr<AActor> Avatar = nullptr;

	/** True while the shot shows as nocked or drawn; false once it was released or cancelled. */
	UPROPERTY(BlueprintReadOnly, Category = "Combat|Ranged")
	bool bDrawing = false;
};
