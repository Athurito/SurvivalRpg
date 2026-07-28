// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgTradeSkillConfigData.h"

#if WITH_EDITOR

#include "Curves/CurveFloat.h"
#include "Misc/DataValidation.h"

namespace RpgTradeSkillConfigData
{
void ValidateConfig(
	const FTradeSkillConfig& Config,
	const FString& ConfigLabel,
	FDataValidationContext& Context,
	EDataValidationResult& Result)
{
	const auto AddError = [&Context, &Result, &ConfigLabel](const FString& Message)
	{
		Context.AddError(FText::FromString(FString::Printf(TEXT("%s: %s"), *ConfigLabel, *Message)));
		Result = EDataValidationResult::Invalid;
	};

	if (Config.MaxLevel < 1 || Config.MaxLevel > 100)
	{
		AddError(TEXT("MaxLevel must be between 1 and 100."));
	}
	if (Config.StationTierUnlockLevel < 0 || Config.StationTierUnlockLevel > 100)
	{
		AddError(TEXT("StationTierUnlockLevel must be between 0 and 100."));
	}

	const int32 SampleMaxLevel = FMath::Clamp(Config.MaxLevel, 1, 100);
	const auto ValidateCurve = [&AddError, SampleMaxLevel](
		const UCurveFloat* Curve,
		const TCHAR* CurveName,
		bool bRequireStrictlyPositive)
	{
		if (!Curve)
		{
			return;
		}

		for (const int32 SampleLevel : { 1, SampleMaxLevel })
		{
			const float Value = Curve->GetFloatValue(static_cast<float>(SampleLevel));
			if (!FMath::IsFinite(Value) || (bRequireStrictlyPositive && Value <= 0.0f))
			{
				AddError(FString::Printf(
					TEXT("%s must return a finite positive value at level %d."),
					CurveName,
					SampleLevel));
			}
		}
	};

	ValidateCurve(Config.XPToNextLevel, TEXT("XPToNextLevel"), true);
	ValidateCurve(Config.YieldMultiplierByLevel, TEXT("YieldMultiplierByLevel"), true);
	ValidateCurve(Config.RareFindMultiplierByLevel, TEXT("RareFindMultiplierByLevel"), true);
}
}

EDataValidationResult URpgTradeSkillConfigData::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(
		Super::IsDataValid(Context),
		EDataValidationResult::Valid);

	const FGameplayTag SkillRoot = FGameplayTag::RequestGameplayTag(TEXT("Skill"));
	for (const TPair<FGameplayTag, FTradeSkillConfig>& Pair : TaggedSkillConfigs)
	{
		if (!Pair.Key.IsValid() || !Pair.Key.MatchesTag(SkillRoot))
		{
			Context.AddError(FText::FromString(FString::Printf(
				TEXT("TaggedSkillConfigs contains '%s'; keys must be registered Skill.* gameplay tags."),
				*Pair.Key.ToString())));
			Result = EDataValidationResult::Invalid;
		}

		RpgTradeSkillConfigData::ValidateConfig(
			Pair.Value,
			FString::Printf(TEXT("TaggedSkillConfigs[%s]"), *Pair.Key.ToString()),
			Context,
			Result);
	}

	for (const TPair<ETradeSkill, FTradeSkillConfig>& Pair : SkillConfigs)
	{
		RpgTradeSkillConfigData::ValidateConfig(
			Pair.Value,
			FString::Printf(TEXT("Legacy SkillConfigs[%d]"), static_cast<int32>(Pair.Key)),
			Context,
			Result);
	}

	return Result;
}

#endif // WITH_EDITOR
