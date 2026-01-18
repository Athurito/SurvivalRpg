// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgHealingExecution.h"

#include "SurvivalRpg/AbilitySystem/Attributes/RpgCombatSet.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgVitalSet.h"
#include "SurvivalRpg/GameplayTags/GameplayTags.h"


namespace
{
	struct FRpgHealingStatics
	{
		FGameplayEffectAttributeCaptureDefinition BaseHealDef;

		FRpgHealingStatics()
		{
			BaseHealDef = FGameplayEffectAttributeCaptureDefinition(
				URpgCombatSet::GetBaseHealAttribute(),
				EGameplayEffectAttributeCaptureSource::Source,
				true // snapshot (wie Lyra). Für live scaling -> false
			);
		}
	};

	static const FRpgHealingStatics& HealStatics()
	{
		static FRpgHealingStatics Statics;
		return Statics;
	}
}

URpgHealingExecution::URpgHealingExecution()
{
	RelevantAttributesToCapture.Add(HealStatics().BaseHealDef);
}

void URpgHealingExecution::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
#if WITH_SERVER_CODE
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();

	const FGameplayTagContainer* SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	const FGameplayTagContainer* TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

	FAggregatorEvaluateParameters Params;
	Params.SourceTags = SourceTags;
	Params.TargetTags = TargetTags;

	// 1) Input heal from SetByCaller
	const float InHeal = Spec.GetSetByCallerMagnitude(RpgGameplayTags::SetByCaller_Heal, /*WarnIfNotFound*/ false, 0.f);

	// 2) Optional: BaseHeal from CombatSet
	float BaseHeal = 0.f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(HealStatics().BaseHealDef, Params, BaseHeal);

	float Heal = FMath::Max(0.f, InHeal + BaseHeal);
	if (Heal <= 0.f)
	{
		return;
	}

	// 3) Output -> VitalSet.Healing (Meta)
	OutExecutionOutput.AddOutputModifier(
		FGameplayModifierEvaluatedData(URpgVitalSet::GetHealingAttribute(), EGameplayModOp::Additive, Heal)
	);
#endif
}
