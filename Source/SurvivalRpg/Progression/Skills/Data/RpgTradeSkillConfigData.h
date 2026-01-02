// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RpgTradeSkillConfigData.generated.h"

UENUM(BlueprintType)
enum class ETradeSkill : uint8
{
	Blacksmithing,
	Woodworking,
	Mining,
	Harvesting,

	MAX UMETA(Hidden)
};

USTRUCT(BlueprintType)
struct FTradeSkillConfig
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UCurveFloat* XPToNextLevel = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	int32 MaxLevel = 200;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	int32 StationTierUnlockLevel = 0;
};

UCLASS()
class SURVIVALRPG_API URpgTradeSkillConfigData : public UDataAsset
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TMap<ETradeSkill, FTradeSkillConfig> SkillConfigs;
};
