#include "RpgCombatImpact.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "GameplayCueManager.h"
#include "GameplayPrediction.h"
#include "HAL/PlatformTime.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgDefenseSet.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgStaminaSet.h"
#include "SurvivalRpg/Factions/RpgFactionSubsystem.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"

namespace
{
	struct FPredictedImpact
	{
		TWeakObjectPtr<const AActor> Target;
		TWeakObjectPtr<const AActor> Instigator;
		double Time = 0.0;
	};

	/** Predictions of the attacking clients in this process (one per PIE client world); game thread only. */
	TArray<FPredictedImpact>& GetPredictedImpacts()
	{
		static TArray<FPredictedImpact> PredictedImpacts;
		return PredictedImpacts;
	}

	void PruneExpiredPredictions(const double Now)
	{
		GetPredictedImpacts().RemoveAll([Now](const FPredictedImpact& Impact)
		{
			return !Impact.Target.IsValid() || Now - Impact.Time > RpgCombatImpact::PredictedImpactLifetimeSeconds;
		});
	}

	bool bPlayingPredictedImpact = false;

#if WITH_DEV_AUTOMATION_TESTS
	TMap<TWeakObjectPtr<const UWorld>, int32>& GetAbsorbedImpactCounts()
	{
		static TMap<TWeakObjectPtr<const UWorld>, int32> AbsorbedImpactCounts;
		return AbsorbedImpactCounts;
	}
#endif
}

namespace RpgCombatImpact
{
	bool CanDamageTarget(const FGameplayEffectContextHandle& Context, const AActor* SourceActor, const AActor* TargetActor)
	{
		if (!TargetActor)
		{
			return false;
		}

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

		const UObject* CandidateInstigators[] = { Context.GetOriginalInstigator(), SourceActor, Context.GetEffectCauser() };
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

	bool HasDamageType(const FGameplayEffectSpec& Spec, FGameplayTag DamageTypeTag)
	{
		FGameplayTagContainer AssetTags;
		Spec.GetAllAssetTags(AssetTags);
		const FGameplayTagContainer* SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
		return AssetTags.HasTag(DamageTypeTag) || (SourceTags && SourceTags->HasTag(DamageTypeTag));
	}

	bool IsInsideBlockCone(const AActor* TargetActor, const AActor* SourceActor, float BlockAngleDegrees)
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

	ERpgCombatImpactOutcome ResolveMeleeOutcome(const UAbilitySystemComponent* TargetASC, const AActor* TargetActor, const AActor* SourceActor)
	{
		if (!TargetASC || !TargetActor || !SourceActor || !TargetASC->HasMatchingGameplayTag(RpgGameplayTags::State_Blocking))
		{
			return ERpgCombatImpactOutcome::Hit;
		}

		const float BlockAngleDegrees = TargetASC->GetNumericAttribute(URpgDefenseSet::GetBlockAngleDegreesAttribute());
		if (!IsInsideBlockCone(TargetActor, SourceActor, BlockAngleDegrees))
		{
			return ERpgCombatImpactOutcome::Hit;
		}

		if (TargetASC->HasMatchingGameplayTag(RpgGameplayTags::State_PerfectBlockWindow))
		{
			return ERpgCombatImpactOutcome::PerfectBlock;
		}

		const float StaminaCost = FMath::Max(0.0f, TargetASC->GetNumericAttribute(URpgDefenseSet::GetBlockStaminaCostAttribute()));
		const float CurrentStamina = TargetASC->GetNumericAttribute(URpgStaminaSet::GetStaminaAttribute());
		return StaminaCost <= 0.0f || CurrentStamina >= StaminaCost
			? ERpgCombatImpactOutcome::Block
			: ERpgCombatImpactOutcome::Hit;
	}

	FGameplayTag FindHitCueTag(const FGameplayEffectSpec& Spec)
	{
		for (const FGameplayTag& Tag : Spec.GetDynamicAssetTags())
		{
			if (Tag.MatchesTag(RpgGameplayTags::GameplayCue_Combat_Hit))
			{
				return Tag;
			}
		}

		return FGameplayTag();
	}

	FGameplayTag GetOutcomeCueTag(ERpgCombatImpactOutcome Outcome, FGameplayTag HitCueTag)
	{
		switch (Outcome)
		{
		case ERpgCombatImpactOutcome::Block:
			return RpgGameplayTags::GameplayCue_Combat_Block;
		case ERpgCombatImpactOutcome::PerfectBlock:
			return RpgGameplayTags::GameplayCue_Combat_PerfectBlock;
		case ERpgCombatImpactOutcome::Hit:
		default:
			return HitCueTag;
		}
	}

	bool IsPredictedByOwner(ERpgCombatImpactOutcome Outcome)
	{
		// A perfect block is rare and decisive, so it reaches the attacker from the server even after a predicted block.
		return Outcome != ERpgCombatImpactOutcome::PerfectBlock;
	}

	FGameplayCueParameters MakeCueParameters(
		const FGameplayEffectContextHandle& Context,
		AActor* SourceActor,
		const UAbilitySystemComponent* TargetASC,
		const FHitResult* HitResult,
		float Damage,
		float StaggerDamage)
	{
		FGameplayCueParameters Parameters;
		Parameters.Instigator = SourceActor;
		Parameters.EffectCauser = SourceActor;
		Parameters.SourceObject = Context.GetSourceObject();
		Parameters.RawMagnitude = FMath::Max(0.0f, Damage);

		const AActor* TargetActor = TargetASC ? TargetASC->GetAvatarActor() : nullptr;
		if (HitResult && (HitResult->bBlockingHit || !HitResult->ImpactPoint.IsZero()))
		{
			Parameters.Location = HitResult->ImpactPoint;
			Parameters.Normal = HitResult->ImpactNormal;
			Parameters.PhysicalMaterial = HitResult->PhysMaterial;
		}
		else if (TargetActor)
		{
			Parameters.Location = TargetActor->GetActorLocation();
		}

		if (TargetASC)
		{
			const float MaxStagger = FMath::Max(1.0f, TargetASC->GetNumericAttribute(URpgDefenseSet::GetMaxStaggerAttribute()));
			Parameters.NormalizedMagnitude = FMath::Clamp(StaggerDamage / MaxStagger, 0.0f, 1.0f);
		}

		return Parameters;
	}

	void ExecuteServerCue(UAbilitySystemComponent* TargetASC, FGameplayTag CueTag, const FGameplayCueParameters& Parameters)
	{
		if (!TargetASC || !CueTag.IsValid())
		{
			return;
		}

		// A key scoped on the target would make its owning client skip the cue as its own prediction.
		FScopedPredictionWindow ClearedKey(TargetASC, FPredictionKey(), false);
		TargetASC->ExecuteGameplayCue(CueTag, Parameters);
	}

	void PlayPredictedImpact(AActor* Target, FGameplayTag CueTag, const FGameplayCueParameters& Parameters)
	{
		if (!Target || !CueTag.IsValid())
		{
			return;
		}

		{
			TGuardValue<bool> PlayingPrediction(bPlayingPredictedImpact, true);
			UGameplayCueManager::ExecuteGameplayCue_NonReplicated(Target, CueTag, Parameters);
		}
		RecordPredictedImpact(Target, Parameters.GetInstigator());
	}

	bool IsPlayingPredictedImpact()
	{
		return bPlayingPredictedImpact;
	}

	void RecordPredictedImpact(const AActor* Target, const AActor* Instigator)
	{
		const double Now = FPlatformTime::Seconds();
		PruneExpiredPredictions(Now);
		GetPredictedImpacts().Add({Target, Instigator, Now});
	}

	bool ConsumePredictedImpact(const AActor* Target, const AActor* Instigator)
	{
		PruneExpiredPredictions(FPlatformTime::Seconds());
		TArray<FPredictedImpact>& PredictedImpacts = GetPredictedImpacts();
		const int32 Index = PredictedImpacts.IndexOfByPredicate([Target, Instigator](const FPredictedImpact& Impact)
		{
			return Impact.Target.Get() == Target && Impact.Instigator.Get() == Instigator;
		});
		if (Index == INDEX_NONE)
		{
			return false;
		}

		PredictedImpacts.RemoveAt(Index);
		return true;
	}

	void NoteAbsorbedImpact(const AActor* Target)
	{
#if WITH_DEV_AUTOMATION_TESTS
		if (const UWorld* World = Target ? Target->GetWorld() : nullptr)
		{
			++GetAbsorbedImpactCounts().FindOrAdd(World);
		}
#endif
	}

#if WITH_DEV_AUTOMATION_TESTS
	int32 GetAbsorbedImpactCountForTests(const UWorld* World)
	{
		const int32* Count = GetAbsorbedImpactCounts().Find(World);
		return Count ? *Count : 0;
	}
#endif
}
