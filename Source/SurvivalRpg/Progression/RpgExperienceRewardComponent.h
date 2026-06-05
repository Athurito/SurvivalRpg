#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RpgExperienceRewardComponent.generated.h"

UCLASS(ClassGroup = (Custom), Blueprintable, meta = (BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgExperienceRewardComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URpgExperienceRewardComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Progression")
	int32 GetEnemyLevel() const { return EnemyLevel; }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Progression")
	void SetEnemyLevel(int32 NewEnemyLevel);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Progression")
	FName GetXPRewardRowName() const { return XPRewardRowName; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Progression")
	float GetFallbackXPReward() const { return XPReward; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Progression")
	float GetXPReward() const { return GetFallbackXPReward(); }

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "Progression", meta = (AllowPrivateAccess = true, ClampMin = "1", UIMin = "1"))
	int32 EnemyLevel = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progression", meta = (AllowPrivateAccess = true))
	FName XPRewardRowName = TEXT("Enemy.Basic");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progression", meta = (AllowPrivateAccess = true, ClampMin = "0.0", DisplayName = "Fallback XP Reward"))
	float XPReward = 10.0f;
};
