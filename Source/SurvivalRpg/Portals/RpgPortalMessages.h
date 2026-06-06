#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "RpgPortalMessages.generated.h"

class AActor;
class URpgPortalEncounterDefinition;

USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgPortalCompletedMessage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Portal")
	TObjectPtr<AActor> Portal = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Portal")
	TObjectPtr<AActor> Instigator = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Portal")
	TObjectPtr<const URpgPortalEncounterDefinition> EncounterDefinition = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Portal")
	FGameplayTagContainer CompletionTags;

	UPROPERTY(BlueprintReadOnly, Category = "Portal")
	float FinalStability = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Portal")
	bool bRewardsRequireBossDefeat = true;

	UPROPERTY(BlueprintReadOnly, Category = "Portal")
	bool bBossDefeated = false;

	UPROPERTY(BlueprintReadOnly, Category = "Portal")
	bool bRewardsEligible = false;
};
