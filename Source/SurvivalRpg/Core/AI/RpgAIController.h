// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GenericTeamAgentInterface.h"
#include "ModularAIController.h"
#include "RpgAIController.generated.h"

class URpgAIPawnData;

UCLASS()
class SURVIVALRPG_API ARpgAIController : public AModularAIController
{
	GENERATED_BODY()

public:
	explicit ARpgAIController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void InitPlayerState() override;
	virtual void OnUnPossess() override;

	virtual void SetGenericTeamId(const FGenericTeamId& NewTeamID) override;
	virtual FGenericTeamId GetGenericTeamId() const override;
	virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;

	const URpgAIPawnData* GetDefaultPawnData() const { return DefaultPawnData; }

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|AI")
	TObjectPtr<const URpgAIPawnData> DefaultPawnData;
};
