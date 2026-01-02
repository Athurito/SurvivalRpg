// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/RpgPlayerProgressionState.h"
#include "RpgPlayerProgressionComponent.generated.h"


class URpgPlayerProgressionData;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnPlayerLevelChanged,
	int32, NewLevel
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnPlayerXPChanged,
	float, CurrentXP,
	float, XPToNextLevel
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnSkillPointsChanged,
	int32, UnspentPoints
);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgPlayerProgressionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	URpgPlayerProgressionComponent();

	virtual void BeginPlay() override;
	/** Config */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Progression")
	URpgPlayerProgressionData* ConfigData = nullptr;

	/** Runtime State (OwnerOnly Replicated) */
	UPROPERTY(ReplicatedUsing=OnRep_State)
	FPlayerProgressionState State;

	/** UI Events */
	UPROPERTY(BlueprintAssignable)
	FOnPlayerLevelChanged OnLevelChanged;

	UPROPERTY(BlueprintAssignable)
	FOnPlayerXPChanged OnXPChanged;

	UPROPERTY(BlueprintAssignable)
	FOnSkillPointsChanged OnSkillPointsChanged;

	/** Queries */
	UFUNCTION(BlueprintCallable)
	int32 GetLevel() const { return State.Level; }

	UFUNCTION(BlueprintCallable)
	float GetXP() const { return State.XP; }

	UFUNCTION(BlueprintCallable)
	int32 GetUnspentSkillPoints() const { return State.UnspentSkillPoints; }

	/** Server API */
	UFUNCTION(BlueprintCallable)
	void AddXP(float Amount);

	UFUNCTION(BlueprintCallable)
	bool SpendSkillPoints(int32 Amount);

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_State();

	float GetXPToNextLevel(int32 Level) const;
	void TryLevelUp();
	void HandleLevelUp(int32 OldLevel, int32 NewLevel);
};
