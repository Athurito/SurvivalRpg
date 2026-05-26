// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SurvivalRpg/Core/Character/RpgCharacter.h"
#include "RpgAICharacter.generated.h"

class URpgExperienceRewardComponent;

UCLASS()
class SURVIVALRPG_API ARpgAICharacter : public ARpgCharacter
{
	GENERATED_BODY()

public:
	explicit ARpgAICharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void OnRep_PlayerState() override;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Progression", Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URpgExperienceRewardComponent> ExperienceRewardComponent;
};
