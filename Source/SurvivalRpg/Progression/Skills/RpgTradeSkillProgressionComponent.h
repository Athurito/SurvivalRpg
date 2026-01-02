// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RpgTradeSkillProgressionComponent.generated.h"

struct FTradeSkillConfig;
enum class ETradeSkill : uint8;
struct FTradeSkillState;
class URpgTradeSkillConfigData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTradeSkillChanged, ETradeSkill, Skill, const FTradeSkillState&, NewState);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgTradeSkillProgressionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	URpgTradeSkillProgressionComponent();
	
	/** Config (Editor / Design-time) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tradeskills")
	URpgTradeSkillConfigData* ConfigData = nullptr;

	/** Runtime state – Index = (int32)ETradeSkill */
	UPROPERTY(ReplicatedUsing=OnRep_SkillStates, VisibleAnywhere, BlueprintReadOnly)
	TArray<FTradeSkillState> SkillStates;

	/** UI / Gameplay events */
	UPROPERTY(BlueprintAssignable)
	FOnTradeSkillChanged OnTradeSkillChanged;

	/** Query API */
	UFUNCTION(BlueprintCallable)
	int32 GetSkillLevel(ETradeSkill Skill) const;

	UFUNCTION(BlueprintCallable)
	float GetSkillXP(ETradeSkill Skill) const;

	/** Server-only API */
	UFUNCTION(BlueprintCallable)
	void AddSkillXP(ETradeSkill Skill, float Amount);

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Rep notify */
	UFUNCTION()
	void OnRep_SkillStates();

	/** Internal helpers */
	const FTradeSkillConfig* GetConfig(ETradeSkill Skill) const;
	void TryLevelUp(ETradeSkill Skill);
	void HandleSkillLevelUp(ETradeSkill Skill, int32 NewLevel);
};
