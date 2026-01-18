// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgDamageExecution.h"

#include "SurvivalRpg/AbilitySystem/Attributes/RpgCombatSet.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgVitalSet.h"
#include "SurvivalRpg/GameplayTags/GameplayTags.h"


namespace
{
	struct FRpgDamageStatics
	{
		// Optional: BaseDamage aus CombatSet
		FGameplayEffectAttributeCaptureDefinition BaseDamageDef;

		// Defense
		FGameplayEffectAttributeCaptureDefinition ArmorDef;
		FGameplayEffectAttributeCaptureDefinition ArmorPenDef;

		// Crit
		FGameplayEffectAttributeCaptureDefinition CritChanceDef;
		FGameplayEffectAttributeCaptureDefinition CritDamageDef;
		FGameplayEffectAttributeCaptureDefinition CritResistDef;

		FRpgDamageStatics()
		{
			// Source stats (Snapshot = true wie Lyra; für "live scaling" setz false)
			BaseDamageDef  = FGameplayEffectAttributeCaptureDefinition(URpgCombatSet::GetBaseDamageAttribute(), EGameplayEffectAttributeCaptureSource::Source, true);

			CritChanceDef  = FGameplayEffectAttributeCaptureDefinition(URpgCombatSet::GetCriticalHitChanceAttribute(), EGameplayEffectAttributeCaptureSource::Source, true);
			CritDamageDef  = FGameplayEffectAttributeCaptureDefinition(URpgCombatSet::GetCriticalHitDamageAttribute(), EGameplayEffectAttributeCaptureSource::Source, true);

			// Target stats
			ArmorDef       = FGameplayEffectAttributeCaptureDefinition(URpgCombatSet::GetArmorAttribute(), EGameplayEffectAttributeCaptureSource::Target, true);
			ArmorPenDef    = FGameplayEffectAttributeCaptureDefinition(URpgCombatSet::GetArmorPenetrationAttribute(), EGameplayEffectAttributeCaptureSource::Source, true);

			CritResistDef  = FGameplayEffectAttributeCaptureDefinition(URpgCombatSet::GetCriticalHitResistanceAttribute(), EGameplayEffectAttributeCaptureSource::Target, true);
		}
	};

	static const FRpgDamageStatics& DamageStatics()
	{
		static FRpgDamageStatics Statics;
		return Statics;
	}

	static float Clamp01(float V) { return FMath::Clamp(V, 0.f, 1.f); }
}

URpgDamageExecution::URpgDamageExecution()
{
	RelevantAttributesToCapture.Add(DamageStatics().BaseDamageDef);

	RelevantAttributesToCapture.Add(DamageStatics().ArmorDef);
	RelevantAttributesToCapture.Add(DamageStatics().ArmorPenDef);

	RelevantAttributesToCapture.Add(DamageStatics().CritChanceDef);
	RelevantAttributesToCapture.Add(DamageStatics().CritDamageDef);
	RelevantAttributesToCapture.Add(DamageStatics().CritResistDef);
}

void URpgDamageExecution::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams,
                                                 FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	#if WITH_SERVER_CODE
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();

	const FGameplayTagContainer* SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	const FGameplayTagContainer* TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

	FAggregatorEvaluateParameters Params;
	Params.SourceTags = SourceTags;
	Params.TargetTags = TargetTags;

	// -------------------------
	// 1) Input Damage (SetByCaller)
	// -------------------------
	const float InDamage = Spec.GetSetByCallerMagnitude(RpgGameplayTags::SetByCaller_Damage, /*WarnIfNotFound*/ false, 0.f);

	// Optional: BaseDamage aus CombatSet (Source) dazu
	float BaseDamage = 0.f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().BaseDamageDef, Params, BaseDamage);

	float Damage = FMath::Max(0.f, InDamage + BaseDamage);

	if (Damage <= 0.f)
	{
		return;
	}

	// -------------------------
	// 2) Armor Mitigation (sehr simpel)
	// -------------------------
	float Armor = 0.f;
	float ArmorPen = 0.f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().ArmorDef, Params, Armor);
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().ArmorPenDef, Params, ArmorPen);

	Armor = FMath::Max(0.f, Armor);
	ArmorPen = FMath::Max(0.f, ArmorPen);

	const float EffectiveArmor = FMath::Max(0.f, Armor - ArmorPen);

	// Diablo-light: Mitigation = Armor / (Armor + K)
	// K kannst du später level-abhängig machen
	const float K = 100.f;
	const float Mitigation = (EffectiveArmor > 0.f) ? (EffectiveArmor / (EffectiveArmor + K)) : 0.f;

	Damage *= (1.f - Clamp01(Mitigation));

	// -------------------------
	// 3) Crit (sehr simpel)
	// -------------------------
	float CritChance = 0.f;
	float CritDamage = 0.f;
	float CritResist = 0.f;

	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().CritChanceDef, Params, CritChance);
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().CritDamageDef, Params, CritDamage);
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().CritResistDef, Params, CritResist);

	// Erwartung:
	// - CritChance als 0..1 (oder 0..100, dann musst du /100)
	// - CritDamage als "Bonus" z.B. 0.5 = +50% (also Mult = 1.5)
	const float FinalCritChance = Clamp01(CritChance - CritResist);

	if (FinalCritChance > 0.f)
	{
		// deterministisch pro Spec (optional), sonst Rand() ok für Start
		const float Roll = FMath::FRand();
		if (Roll < FinalCritChance)
		{
			const float CritMult = 1.f + FMath::Max(0.f, CritDamage);
			Damage *= CritMult;
		}
	}

	Damage = FMath::Max(0.f, Damage);

	// -------------------------
	// 4) Output -> VitalSet.Damage (Meta)
	// -------------------------
	if (Damage > 0.f)
	{
		OutExecutionOutput.AddOutputModifier(
			FGameplayModifierEvaluatedData(URpgVitalSet::GetDamageAttribute(), EGameplayModOp::Additive, Damage)
		);
	}
#endif
}
