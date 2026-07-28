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

/** Server-authoritative, owner-only replicated general character progression. */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgPlayerProgressionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	URpgPlayerProgressionComponent();

	virtual void BeginPlay() override;
	/** Designer-authored level curve, point awards, and maximum character level. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Progression")
	TObjectPtr<URpgPlayerProgressionData> ConfigData = nullptr;

	/** Server-authored state replicated only to the owning player and persisted by the host save. */
	UPROPERTY(ReplicatedUsing=OnRep_State, VisibleInstanceOnly, BlueprintReadOnly, Category = "Rpg|Progression")
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
	float GetXPToNextLevelForCurrentLevel() const;

	UFUNCTION(BlueprintCallable)
	int32 GetUnspentSkillPoints() const { return State.UnspentSkillPoints; }

	/** Server API */
	UFUNCTION(BlueprintCallable)
	void AddXP(float Amount);

	UFUNCTION(BlueprintCallable)
	bool SpendSkillPoints(int32 Amount);

	/** Pointer-free snapshot consumed by host persistence. */
	FPlayerProgressionState ExportProgressionState() const { return State; }

	/** Restores a prevalidated authoritative snapshot without granting level-up rewards again. */
	bool RestoreProgressionState(const FPlayerProgressionState& InState);

	/** Restores the default level 1, zero-XP state used by profiles from older save schemas. */
	void ResetProgressionStateToDefaults();

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_State();

	float GetXPToNextLevel(int32 Level) const;
	void TryLevelUp();
	void HandleLevelUp(int32 OldLevel, int32 NewLevel);
	void BroadcastStateChanged();
	void MarkOwnerSaveDirty() const;
};
