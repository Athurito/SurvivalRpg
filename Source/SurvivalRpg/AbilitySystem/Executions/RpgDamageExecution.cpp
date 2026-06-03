// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgDamageExecution.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySourceInterface.h"
#include "SurvivalRpg/AbilitySystem/RpgGameplayEffectContext.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgCombatSet.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgDefenseSet.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgHealthSet.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgStaminaSet.h"
#include "SurvivalRpg/Factions/RpgFactionSubsystem.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"


namespace
{
	static TAutoConsoleVariable<int32> CVarRpgCombatBlockDebug(
		TEXT("rpg.Combat.Block.Debug"),
		0,
		TEXT("Logs server-side block, perfect block, and guard/posture decisions."));

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
			ArmorDef       = FGameplayEffectAttributeCaptureDefinition(URpgDefenseSet::GetArmorAttribute(), EGameplayEffectAttributeCaptureSource::Target, true);
			ArmorPenDef    = FGameplayEffectAttributeCaptureDefinition(URpgCombatSet::GetArmorPenetrationAttribute(), EGameplayEffectAttributeCaptureSource::Source, true);

			CritResistDef  = FGameplayEffectAttributeCaptureDefinition(URpgDefenseSet::GetCriticalHitResistanceAttribute(), EGameplayEffectAttributeCaptureSource::Target, true);
		}
	};

	static const FRpgDamageStatics& DamageStatics()
	{
		static FRpgDamageStatics Statics;
		return Statics;
	}

	static float Clamp01(float V) { return FMath::Clamp(V, 0.f, 1.f); }

	static bool CanSourceDamageTarget(const FGameplayEffectSpec& Spec, UAbilitySystemComponent* SourceASC, UAbilitySystemComponent* TargetASC)
	{
		const AActor* SourceActor = SourceASC ? SourceASC->GetAvatarActor() : nullptr;
		const AActor* TargetActor = TargetASC ? TargetASC->GetAvatarActor() : nullptr;
		if (!TargetActor)
		{
			return false;
		}

		const AActor* EffectCauser = Spec.GetEffectContext().GetEffectCauser();
		const AActor* OriginalInstigator = Spec.GetEffectContext().GetOriginalInstigator();

		UWorld* World = TargetActor->GetWorld();
		if (!World && SourceActor)
		{
			World = SourceActor->GetWorld();
		}

		const URpgFactionSubsystem* FactionSubsystem = World ? World->GetSubsystem<URpgFactionSubsystem>() : nullptr;
		if (!FactionSubsystem)
		{
			return false;
		}

		const UObject* CandidateInstigators[] = { OriginalInstigator, SourceActor, EffectCauser };
		for (const UObject* CandidateInstigator : CandidateInstigators)
		{
			if (!CandidateInstigator)
			{
				continue;
			}

			if (FactionSubsystem->CanCauseDamage(CandidateInstigator, TargetActor, true))
			{
				return true;
			}

			int32 CandidateFactionId = INDEX_NONE;
			int32 TargetFactionId = INDEX_NONE;
			const ERpgFactionComparison Comparison = FactionSubsystem->CompareFactions(
				CandidateInstigator,
				TargetActor,
				CandidateFactionId,
				TargetFactionId);

			if (Comparison == ERpgFactionComparison::SameFaction || CandidateFactionId != INDEX_NONE)
			{
				return false;
			}
		}

		return false;
	}

	static bool HasDamageType(const FGameplayEffectSpec& Spec, const FGameplayTagContainer* SourceTags, FGameplayTag DamageTypeTag)
	{
		FGameplayTagContainer AssetTags;
		Spec.GetAllAssetTags(AssetTags);
		return AssetTags.HasTag(DamageTypeTag) || (SourceTags && SourceTags->HasTag(DamageTypeTag));
	}

	static bool IsHitInsideBlockCone(const AActor* TargetActor, const AActor* SourceActor, float BlockAngleDegrees)
	{
		if (!TargetActor || !SourceActor)
		{
			return false;
		}

		const float ClampedAngle = FMath::Clamp(BlockAngleDegrees, 0.0f, 360.0f);
		if (ClampedAngle >= 359.0f)
		{
			return true;
		}

		const FVector TargetForward = TargetActor->GetActorForwardVector().GetSafeNormal2D();
		const FVector ToSource = (SourceActor->GetActorLocation() - TargetActor->GetActorLocation()).GetSafeNormal2D();
		if (TargetForward.IsNearlyZero() || ToSource.IsNearlyZero())
		{
			return false;
		}

		const float RequiredDot = FMath::Cos(FMath::DegreesToRadians(ClampedAngle * 0.5f));
		return FVector::DotProduct(TargetForward, ToSource) >= RequiredDot;
	}

	static void SendCombatEvent(AActor* TargetActor, AActor* InstigatorActor, FGameplayTag EventTag, const FGameplayEffectSpec& Spec, float EventMagnitude)
	{
		if (!TargetActor || !EventTag.IsValid())
		{
			return;
		}

		FGameplayEventData Payload;
		Payload.EventTag = EventTag;
		Payload.Instigator = InstigatorActor;
		Payload.Target = TargetActor;
		Payload.ContextHandle = Spec.GetContext();
		Payload.EventMagnitude = EventMagnitude;

		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(TargetActor, EventTag, Payload);
	}

	static bool CanReceiveStagger(const UAbilitySystemComponent* ASC)
	{
		return ASC &&
			ASC->HasMatchingGameplayTag(RpgGameplayTags::Trait_Staggerable) &&
			!ASC->HasMatchingGameplayTag(RpgGameplayTags::State_Staggered) &&
			!ASC->HasMatchingGameplayTag(RpgGameplayTags::State_GuardBroken) &&
			!ASC->HasMatchingGameplayTag(RpgGameplayTags::State_StaggerImmune);
	}

	static float GetScaledIncomingStaggerDamage(const UAbilitySystemComponent* ASC, float StaggerDamage)
	{
		if (!CanReceiveStagger(ASC) || StaggerDamage <= 0.0f)
		{
			return 0.0f;
		}

		const float IncomingStaggerMultiplier = FMath::Max(0.0f, ASC->GetNumericAttribute(URpgDefenseSet::GetIncomingStaggerDamageMultiplierAttribute()));
		return FMath::Max(0.0f, StaggerDamage * IncomingStaggerMultiplier);
	}

	static bool ShouldDebugBlock()
	{
		return CVarRpgCombatBlockDebug.GetValueOnGameThread() != 0;
	}

	static void ApplyStaggerToSource(UAbilitySystemComponent* SourceASC, AActor* SourceActor, AActor* InstigatorActor, const FGameplayEffectSpec& Spec, float StaggerDamage)
	{
		StaggerDamage = GetScaledIncomingStaggerDamage(SourceASC, StaggerDamage);
		if (!SourceASC || !SourceActor || StaggerDamage <= 0.0f)
		{
			if (ShouldDebugBlock())
			{
				UE_LOG(LogTemp, Warning, TEXT("BlockDebug PerfectBlockSource[%s]: no attacker posture applied. RawOrScaled=%.2f Staggerable=%d Immune=%d"),
					*GetNameSafe(SourceActor),
					StaggerDamage,
					SourceASC && SourceASC->HasMatchingGameplayTag(RpgGameplayTags::Trait_Staggerable) ? 1 : 0,
					SourceASC && SourceASC->HasMatchingGameplayTag(RpgGameplayTags::State_StaggerImmune) ? 1 : 0);
			}
			return;
		}

		const float OldStagger = SourceASC->GetNumericAttribute(URpgDefenseSet::GetStaggerAttribute());
		const float MaxStagger = FMath::Max(1.0f, SourceASC->GetNumericAttribute(URpgDefenseSet::GetMaxStaggerAttribute()));
		const bool bWillGuardBreak = OldStagger < MaxStagger && (OldStagger + StaggerDamage) >= MaxStagger && CanReceiveStagger(SourceASC);

		if (ShouldDebugBlock())
		{
			UE_LOG(LogTemp, Warning, TEXT("BlockDebug PerfectBlockSource[%s]: attacker posture +%.2f %.2f/%.2f -> %.2f/%.2f WillGuardBreak=%d"),
				*GetNameSafe(SourceActor),
				StaggerDamage,
				OldStagger,
				MaxStagger,
				FMath::Min(MaxStagger, OldStagger + StaggerDamage),
				MaxStagger,
				bWillGuardBreak ? 1 : 0);
		}

		SourceASC->ApplyModToAttribute(URpgDefenseSet::GetStaggerAttribute(), EGameplayModOp::Additive, StaggerDamage);

		const float NewStagger = SourceASC->GetNumericAttribute(URpgDefenseSet::GetStaggerAttribute());
		if (OldStagger < MaxStagger && NewStagger >= MaxStagger && CanReceiveStagger(SourceASC))
		{
			SendCombatEvent(SourceActor, InstigatorActor, RpgGameplayTags::GameplayEvent_Stagger, Spec, NewStagger);
			SourceASC->SetNumericAttributeBase(URpgDefenseSet::GetStaggerAttribute(), 0.0f);
		}
	}
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
	const float InStaggerDamage = Spec.GetSetByCallerMagnitude(RpgGameplayTags::SetByCaller_StaggerDamage, /*WarnIfNotFound*/ false, 0.f);

	// Optional: BaseDamage aus CombatSet (Source) dazu
	float BaseDamage = 0.f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().BaseDamageDef, Params, BaseDamage);

	float Damage = FMath::Max(0.f, InDamage + BaseDamage);
	float StaggerDamage = FMath::Max(0.f, InStaggerDamage);

	if (Damage <= 0.f && StaggerDamage <= 0.f)
	{
		return;
	}

	UAbilitySystemComponent* SourceASC = ExecutionParams.GetSourceAbilitySystemComponent();
	UAbilitySystemComponent* TargetASC = ExecutionParams.GetTargetAbilitySystemComponent();
	AActor* SourceActor = SourceASC ? SourceASC->GetAvatarActor() : nullptr;
	AActor* TargetActor = TargetASC ? TargetASC->GetAvatarActor() : nullptr;

	if (!CanSourceDamageTarget(Spec, SourceASC, TargetASC))
	{
		return;
	}

	if (const FRpgGameplayEffectContext* RpgContext = FRpgGameplayEffectContext::ExtractEffectContext(Spec.GetEffectContext()))
	{
		if (const IRpgAbilitySourceInterface* AbilitySource = RpgContext->GetAbilitySource())
		{
			float Distance = 0.0f;
			if (const FHitResult* HitResult = RpgContext->GetHitResult())
			{
				if (const AActor* ContextSourceActor = RpgContext->GetEffectCauser())
				{
					Distance = FVector::Dist(ContextSourceActor->GetActorLocation(), HitResult->ImpactPoint);
				}
			}

			Damage *= AbilitySource->GetDistanceAttenuation(Distance, SourceTags, TargetTags);
			Damage *= AbilitySource->GetPhysicalMaterialAttenuation(RpgContext->GetPhysicalMaterial(), SourceTags, TargetTags);
		}
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
	// 4) Block / Perfect Block
	// -------------------------
	const bool bIsMeleeDamage = HasDamageType(Spec, SourceTags, RpgGameplayTags::Damage_Type_Melee);
	const bool bIsBlocking = TargetASC && TargetASC->HasMatchingGameplayTag(RpgGameplayTags::State_Blocking);
	const bool bIsPerfectBlock = TargetASC && TargetASC->HasMatchingGameplayTag(RpgGameplayTags::State_PerfectBlockWindow);

	if (bIsMeleeDamage && bIsBlocking && TargetActor && SourceActor)
	{
		const float BlockAngleDegrees = TargetASC->GetNumericAttribute(URpgDefenseSet::GetBlockAngleDegreesAttribute());
		const bool bInBlockCone = IsHitInsideBlockCone(TargetActor, SourceActor, BlockAngleDegrees);
		if (bInBlockCone)
		{
			const float DamageBeforeBlock = Damage;
			const float StaggerDamageBeforeBlock = StaggerDamage;
			const float StaminaCost = FMath::Max(0.0f, TargetASC->GetNumericAttribute(URpgDefenseSet::GetBlockStaminaCostAttribute()));
			const float CurrentStamina = TargetASC->GetNumericAttribute(URpgStaminaSet::GetStaminaAttribute());
			const bool bCanPayBlock = StaminaCost <= 0.0f || CurrentStamina >= StaminaCost;

			if (bIsPerfectBlock)
			{
				const float StaminaRestore = FMath::Max(0.0f, TargetASC->GetNumericAttribute(URpgDefenseSet::GetPerfectBlockStaminaRestoreAttribute()));
				const float SourceStaggerDamage = FMath::Max(0.0f, TargetASC->GetNumericAttribute(URpgDefenseSet::GetPerfectBlockStaggerDamageAttribute()));

				Damage = 0.0f;
				StaggerDamage = 0.0f;

				if (ShouldDebugBlock())
				{
					const float CurrentTargetStagger = TargetASC->GetNumericAttribute(URpgDefenseSet::GetStaggerAttribute());
					const float TargetMaxStagger = FMath::Max(1.0f, TargetASC->GetNumericAttribute(URpgDefenseSet::GetMaxStaggerAttribute()));
					UE_LOG(LogTemp, Warning, TEXT("BlockDebug PerfectBlock[%s <- %s]: Damage %.2f->%.2f DefenderStagger %.2f->%.2f Current %.2f/%.2f StaminaRestore=%.2f AttackerPosture=%.2f DefenderStaggerable=%d"),
						*GetNameSafe(TargetActor),
						*GetNameSafe(SourceActor),
						DamageBeforeBlock,
						Damage,
						StaggerDamageBeforeBlock,
						StaggerDamage,
						CurrentTargetStagger,
						TargetMaxStagger,
						StaminaRestore,
						SourceStaggerDamage,
						TargetASC->HasMatchingGameplayTag(RpgGameplayTags::Trait_Staggerable) ? 1 : 0);
				}

				if (StaminaRestore > 0.0f)
				{
					OutExecutionOutput.AddOutputModifier(
						FGameplayModifierEvaluatedData(URpgStaminaSet::GetStaminaAttribute(), EGameplayModOp::Additive, StaminaRestore)
					);
				}

				ApplyStaggerToSource(SourceASC, SourceActor, TargetActor, Spec, SourceStaggerDamage);
				SendCombatEvent(TargetActor, SourceActor, RpgGameplayTags::GameplayEvent_PerfectBlock, Spec, SourceStaggerDamage);
			}
			else if (bCanPayBlock)
			{
				const float DamageReduction = FMath::Clamp(TargetASC->GetNumericAttribute(URpgDefenseSet::GetBlockDamageReductionAttribute()), 0.0f, 1.0f);
				const float BlockStaggerMultiplier = FMath::Max(0.0f, TargetASC->GetNumericAttribute(URpgDefenseSet::GetBlockStaggerDamageMultiplierAttribute()));

				Damage *= (1.0f - DamageReduction);
				StaggerDamage *= BlockStaggerMultiplier;

				if (ShouldDebugBlock())
				{
					const float CurrentTargetStagger = TargetASC->GetNumericAttribute(URpgDefenseSet::GetStaggerAttribute());
					const float TargetMaxStagger = FMath::Max(1.0f, TargetASC->GetNumericAttribute(URpgDefenseSet::GetMaxStaggerAttribute()));
					const float ScaledStaggerDamage = GetScaledIncomingStaggerDamage(TargetASC, StaggerDamage);
					const bool bWillGuardBreak = CurrentTargetStagger < TargetMaxStagger &&
						(CurrentTargetStagger + ScaledStaggerDamage) >= TargetMaxStagger &&
						CanReceiveStagger(TargetASC);

					UE_LOG(LogTemp, Warning, TEXT("BlockDebug NormalBlock[%s <- %s]: Damage %.2f->%.2f StaminaCost=%.2f Stamina=%.2f Stagger %.2f*%.2f=%.2f Scaled=%.2f Current %.2f/%.2f DefenderStaggerable=%d Immune=%d WillGuardBreak=%d"),
						*GetNameSafe(TargetActor),
						*GetNameSafe(SourceActor),
						DamageBeforeBlock,
						Damage,
						StaminaCost,
						CurrentStamina,
						StaggerDamageBeforeBlock,
						BlockStaggerMultiplier,
						StaggerDamage,
						ScaledStaggerDamage,
						CurrentTargetStagger,
						TargetMaxStagger,
						TargetASC->HasMatchingGameplayTag(RpgGameplayTags::Trait_Staggerable) ? 1 : 0,
						TargetASC->HasMatchingGameplayTag(RpgGameplayTags::State_StaggerImmune) ? 1 : 0,
						bWillGuardBreak ? 1 : 0);
				}

				if (StaminaCost > 0.0f)
				{
					OutExecutionOutput.AddOutputModifier(
						FGameplayModifierEvaluatedData(URpgStaminaSet::GetStaminaAttribute(), EGameplayModOp::Additive, -StaminaCost)
					);
				}

				SendCombatEvent(TargetActor, SourceActor, RpgGameplayTags::GameplayEvent_Block, Spec, StaminaCost);
			}
			else if (ShouldDebugBlock())
			{
				UE_LOG(LogTemp, Warning, TEXT("BlockDebug FailedBlock[%s <- %s]: not enough stamina. Damage=%.2f Stagger=%.2f StaminaCost=%.2f Stamina=%.2f"),
					*GetNameSafe(TargetActor),
					*GetNameSafe(SourceActor),
					Damage,
					StaggerDamage,
					StaminaCost,
					CurrentStamina);
			}
		}
		else if (ShouldDebugBlock())
		{
			UE_LOG(LogTemp, Warning, TEXT("BlockDebug FailedBlock[%s <- %s]: outside block cone. Cone=%.2f Damage=%.2f Stagger=%.2f"),
				*GetNameSafe(TargetActor),
				*GetNameSafe(SourceActor),
				BlockAngleDegrees,
				Damage,
				StaggerDamage);
		}
	}

	if (TargetASC && TargetASC->HasMatchingGameplayTag(RpgGameplayTags::State_Staggered))
	{
		const float StaggeredDamageMultiplier = FMath::Max(
			0.0f,
			TargetASC->GetNumericAttribute(URpgDefenseSet::GetStaggeredDamageTakenMultiplierAttribute()));
		Damage *= StaggeredDamageMultiplier;
	}

	StaggerDamage = GetScaledIncomingStaggerDamage(TargetASC, StaggerDamage);

	// -------------------------
	// 5) Output -> Health and Stagger
	// -------------------------
	if (Damage > 0.f)
	{
		OutExecutionOutput.AddOutputModifier(
			FGameplayModifierEvaluatedData(URpgHealthSet::GetDamageAttribute(), EGameplayModOp::Additive, Damage)
		);
	}

	if (StaggerDamage > 0.f)
	{
		OutExecutionOutput.AddOutputModifier(
			FGameplayModifierEvaluatedData(URpgDefenseSet::GetStaggerAttribute(), EGameplayModOp::Additive, StaggerDamage)
		);
	}
#endif
}
