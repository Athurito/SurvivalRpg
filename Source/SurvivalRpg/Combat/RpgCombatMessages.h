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
