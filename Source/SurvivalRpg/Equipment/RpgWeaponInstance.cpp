#include "RpgWeaponInstance.h"

#include "Animation/AnimInstance.h"

#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"

#if WITH_EDITOR
#include "Animation/AnimClassInterface.h"
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "RpgWeaponInstance"

FRpgWeaponAttackDefinition::FRpgWeaponAttackDefinition()
{
	TracePointSockets.Add(TEXT("blade_base"));
	TracePointSockets.Add(TEXT("blade_mid"));
	TracePointSockets.Add(TEXT("blade_tip"));
}

bool FRpgWeaponAttackDefinition::HasValidTraceData() const
{
	if (TracePointSockets.Num() < 2 || TraceSampleInterval <= 0.0f)
	{
		return false;
	}

	switch (TraceMode)
	{
	case ERpgWeaponAttackTraceMode::LineTrace:
		return !bTraceBetweenSockets || TraceInterpolationDistance > 0.0f;
	case ERpgWeaponAttackTraceMode::SphereSweep:
		return TraceRadius > 0.0f;
	case ERpgWeaponAttackTraceMode::CapsuleSweep:
		return TraceRadius > 0.0f && TraceCapsuleHalfHeight > 0.0f;
	case ERpgWeaponAttackTraceMode::BoxSweep:
		return TraceBoxExtent.GetAbsMin() > 0.0f;
	default:
		return false;
	}
}

bool FRpgConditionalAttackModifier::MatchesTargetTags(const FGameplayTagContainer& TargetTags) const
{
	if (!TargetTags.HasAll(RequiredTargetTags))
	{
		return false;
	}

	if (BlockedTargetTags.Num() > 0 && TargetTags.HasAny(BlockedTargetTags))
	{
		return false;
	}

	return true;
}

URpgWeaponInstance::URpgWeaponInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	WeaponTypeTag = RpgGameplayTags::Weapon_Type_Melee;
	WeaponFamilyTag = RpgGameplayTags::Weapon_Family_Sword;

	FRpgWeaponAttackDefinition PrimaryAttack;
	PrimaryAttack.DamageTypeTags.AddTag(RpgGameplayTags::Damage_Type_Melee);
	PrimaryAttack.StaggerDamage = 20.0f;
	PrimaryAttack.HitReactionEventTag = RpgGameplayTags::GameplayEvent_HitReaction;
	AttackDefinitions.Add(RpgGameplayTags::Weapon_Attack_Primary, PrimaryAttack);

	BlockDefinition.BlockableDamageTypeTags.AddTag(RpgGameplayTags::Damage_Type_Melee);
}

#if WITH_EDITOR
EDataValidationResult URpgWeaponInstance::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);
	const FText WeaponPath = FText::FromString(GetPathName());
	if (!FMath::IsFinite(BlockDefinition.MovementSpeedLimit) || BlockDefinition.MovementSpeedLimit < 0.f)
	{
		Context.AddError(FText::Format(LOCTEXT("InvalidBlockMovementSpeedLimit",
			"Weapon '{0}' has an invalid BlockDefinition.MovementSpeedLimit. Use a finite non-negative speed in cm/s; zero preserves the ordinary speed limit."), WeaponPath));
		Result = EDataValidationResult::Invalid;
	}

	if (UClass* LayerClass = BlockDefinition.BlockLocomotionLayer.Get())
	{
		const IAnimClassInterface* AnimClass = IAnimClassInterface::GetFromClass(LayerClass);
		// LinkAnimClassLayers binds only implemented animation functions. A main AnimGraph alone,
		// an interface stub, or a native AnimInstance cannot replace a linked layer.
		const bool bHasImplementedLayer = AnimClass && AnimClass->GetAnimBlueprintFunctions().ContainsByPredicate(
			[](const FAnimBlueprintFunction& Function)
			{
				return Function.bImplemented && !Function.Name.IsNone() && Function.Name != TEXT("AnimGraph");
			});
		if (LayerClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists | CLASS_Interface)
			|| !bHasImplementedLayer)
		{
			Context.AddError(FText::Format(LOCTEXT("InvalidBlockLocomotionLayer",
				"Weapon '{0}' uses BlockDefinition.BlockLocomotionLayer '{1}', which is not a concrete compiled Anim Blueprint with an implemented animation layer. Assign a valid linked-layer class or clear it to retain the legacy block loop."),
				WeaponPath, FText::FromString(LayerClass->GetPathName())));
			Result = EDataValidationResult::Invalid;
		}
		else
		{
			// Let the layer's existing asset-validation seam report its designer-owned selection checks.
			Result = CombineDataValidationResults(Result, LayerClass->GetDefaultObject<UAnimInstance>()->IsDataValid(Context));
		}
	}
	return Result;
}
#endif

#if WITH_DEV_AUTOMATION_TESTS
bool URpgWeaponInstance::ConfigureBlockLocomotionForTests(TSubclassOf<UAnimInstance> LayerClass, float SpeedLimit)
{
	if (HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) || !GetPawn()
		|| !FMath::IsFinite(SpeedLimit) || SpeedLimit < 0.f) { return false; }
	BlockDefinition.BlockLocomotionLayer = LayerClass;
	BlockDefinition.MovementSpeedLimit = SpeedLimit;
	return true;
}
#endif

const FRpgWeaponAttackDefinition* URpgWeaponInstance::FindAttackDefinition(FGameplayTag AttackDefinitionTag) const
{
	return AttackDefinitionTag.IsValid() ? AttackDefinitions.Find(AttackDefinitionTag) : nullptr;
}

bool URpgWeaponInstance::GetAttackDefinitionByTag(FGameplayTag AttackDefinitionTag, FRpgWeaponAttackDefinition& OutAttackDefinition) const
{
	if (const FRpgWeaponAttackDefinition* AttackDefinition = FindAttackDefinition(AttackDefinitionTag))
	{
		OutAttackDefinition = *AttackDefinition;
		return true;
	}

	return false;
}

TArray<FGameplayTag> URpgWeaponInstance::GetAttackDefinitionTags() const
{
	TArray<FGameplayTag> AttackDefinitionTags;
	AttackDefinitions.GetKeys(AttackDefinitionTags);
	return AttackDefinitionTags;
}

TArray<FName> URpgWeaponInstance::GetAttackDefinitionTagNames() const
{
	TArray<FName> AttackDefinitionTagNames;
	AttackDefinitionTagNames.Reserve(AttackDefinitions.Num());
	for (const TPair<FGameplayTag, FRpgWeaponAttackDefinition>& Entry : AttackDefinitions)
	{
		AttackDefinitionTagNames.Add(Entry.Key.GetTagName());
	}
	return AttackDefinitionTagNames;
}

bool URpgWeaponInstance::HasAttackDefinitionByTagName(FName AttackDefinitionTagName) const
{
	const FGameplayTag AttackDefinitionTag = AttackDefinitionTagName.IsNone() ? FGameplayTag() : FGameplayTag::RequestGameplayTag(AttackDefinitionTagName);
	return FindAttackDefinition(AttackDefinitionTag) != nullptr;
}

#if WITH_DEV_AUTOMATION_TESTS
bool URpgWeaponInstance::SetAttackMontagePlayRateForTests(
	const FGameplayTag AttackDefinitionTag,
	const float NewPlayRate,
	float& OutPreviousPlayRate)
{
	if (!FMath::IsFinite(NewPlayRate) || NewPlayRate <= UE_SMALL_NUMBER)
	{
		return false;
	}

	FRpgWeaponAttackDefinition* AttackDefinition = AttackDefinitions.Find(AttackDefinitionTag);
	if (!AttackDefinition)
	{
		return false;
	}

	OutPreviousPlayRate = AttackDefinition->MontagePlayRate;
	AttackDefinition->MontagePlayRate = NewPlayRate;
	return true;
}
#endif

void URpgWeaponInstance::SetWeaponTagsByName(FName WeaponTypeTagName, FName WeaponFamilyTagName)
{
	WeaponTypeTag = WeaponTypeTagName.IsNone() ? FGameplayTag() : FGameplayTag::RequestGameplayTag(WeaponTypeTagName);
	WeaponFamilyTag = WeaponFamilyTagName.IsNone() ? FGameplayTag() : FGameplayTag::RequestGameplayTag(WeaponFamilyTagName);
}

void URpgWeaponInstance::ClearAttackDefinitions()
{
	AttackDefinitions.Reset();
}

void URpgWeaponInstance::ConfigureAttackByTagName(
	FName AttackDefinitionTagName,
	UAnimMontage* Montage,
	TSubclassOf<UGameplayEffect> DamageEffect,
	float Damage,
	float TraceRadius,
	const TArray<FName>& TracePointSockets,
	float TraceSampleInterval,
	TSubclassOf<URpgCameraMode> CameraMode,
	FName HitReactionEventTagName)
{
	const FGameplayTag AttackDefinitionTag = AttackDefinitionTagName.IsNone() ? FGameplayTag() : FGameplayTag::RequestGameplayTag(AttackDefinitionTagName);
	if (!AttackDefinitionTag.IsValid())
	{
		return;
	}

	FRpgWeaponAttackDefinition& AttackDefinition = AttackDefinitions.FindOrAdd(AttackDefinitionTag);
	AttackDefinition.Montage = Montage;
	AttackDefinition.DamageEffect = DamageEffect;
	AttackDefinition.Damage = Damage;
	AttackDefinition.TraceRadius = TraceRadius;
	AttackDefinition.TracePointSockets = TracePointSockets;
	AttackDefinition.TraceSampleInterval = TraceSampleInterval;
	AttackDefinition.CameraMode = CameraMode;
	AttackDefinition.HitReactionEventTag = HitReactionEventTagName.IsNone() ? FGameplayTag() : FGameplayTag::RequestGameplayTag(HitReactionEventTagName);
}

void URpgWeaponInstance::ConfigureMeleeAttackByTagName(
	FName AttackDefinitionTagName,
	UAnimMontage* Montage,
	TSubclassOf<UGameplayEffect> DamageEffect,
	float Damage,
	float StaggerDamage,
	float TraceRadius,
	const TArray<FName>& TracePointSockets,
	float TraceSampleInterval,
	TSubclassOf<URpgCameraMode> CameraMode,
	FName HitReactionEventTagName)
{
	ConfigureAttackByTagName(
		AttackDefinitionTagName,
		Montage,
		DamageEffect,
		Damage,
		TraceRadius,
		TracePointSockets,
		TraceSampleInterval,
		CameraMode,
		HitReactionEventTagName);

	const FGameplayTag AttackDefinitionTag = AttackDefinitionTagName.IsNone() ? FGameplayTag() : FGameplayTag::RequestGameplayTag(AttackDefinitionTagName);
	if (FRpgWeaponAttackDefinition* AttackDefinition = AttackDefinitions.Find(AttackDefinitionTag))
	{
		AttackDefinition->DamageTypeTags.Reset();
		AttackDefinition->DamageTypeTags.AddTag(RpgGameplayTags::Damage_Type_Melee);
		AttackDefinition->StaggerDamage = FMath::Max(0.0f, StaggerDamage);
	}
}

void URpgWeaponInstance::ConfigureMeleeBlock(
	bool bCanBlock,
	bool bAllowPerfectBlock,
	float BlockAngleDegrees,
	float PerfectBlockWindow,
	float StaminaCost,
	float DamageReduction,
	float BlockStaggerDamageMultiplier,
	float PerfectBlockStaminaRestore,
	float PerfectBlockStaggerDamage,
	UAnimMontage* BlockLoopMontage)
{
	BlockDefinition.bCanBlock = bCanBlock;
	BlockDefinition.bAllowPerfectBlock = bAllowPerfectBlock;
	BlockDefinition.BlockableDamageTypeTags.Reset();
	BlockDefinition.BlockableDamageTypeTags.AddTag(RpgGameplayTags::Damage_Type_Melee);
	BlockDefinition.BlockAngleDegrees = FMath::Clamp(BlockAngleDegrees, 0.0f, 360.0f);
	BlockDefinition.PerfectBlockWindow = FMath::Max(0.0f, PerfectBlockWindow);
	BlockDefinition.StaminaCost = FMath::Max(0.0f, StaminaCost);
	BlockDefinition.DamageReduction = FMath::Clamp(DamageReduction, 0.0f, 1.0f);
	BlockDefinition.BlockStaggerDamageMultiplier = FMath::Max(0.0f, BlockStaggerDamageMultiplier);
	BlockDefinition.PerfectBlockStaminaRestore = FMath::Max(0.0f, PerfectBlockStaminaRestore);
	BlockDefinition.PerfectBlockStaggerDamage = FMath::Max(0.0f, PerfectBlockStaggerDamage);
	BlockDefinition.BlockLoopMontage = BlockLoopMontage;
}

#undef LOCTEXT_NAMESPACE
