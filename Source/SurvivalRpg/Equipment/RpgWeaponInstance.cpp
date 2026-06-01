#include "RpgWeaponInstance.h"

#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"

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
	float DamageTraceDelay,
	float TraceDistance,
	float TraceRadius,
	FName TraceStartSocket,
	FName TraceEndSocket,
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
	AttackDefinition.DamageTraceDelay = DamageTraceDelay;
	AttackDefinition.TraceDistance = TraceDistance;
	AttackDefinition.TraceRadius = TraceRadius;
	AttackDefinition.TraceStartSocket = TraceStartSocket;
	AttackDefinition.TraceEndSocket = TraceEndSocket;
	AttackDefinition.CameraMode = CameraMode;
	AttackDefinition.HitReactionEventTag = HitReactionEventTagName.IsNone() ? FGameplayTag() : FGameplayTag::RequestGameplayTag(HitReactionEventTagName);
}

void URpgWeaponInstance::ConfigureMeleeAttackByTagName(
	FName AttackDefinitionTagName,
	UAnimMontage* Montage,
	TSubclassOf<UGameplayEffect> DamageEffect,
	float Damage,
	float StaggerDamage,
	float DamageTraceDelay,
	float TraceDistance,
	float TraceRadius,
	FName TraceStartSocket,
	FName TraceEndSocket,
	TSubclassOf<URpgCameraMode> CameraMode,
	FName HitReactionEventTagName)
{
	ConfigureAttackByTagName(
		AttackDefinitionTagName,
		Montage,
		DamageEffect,
		Damage,
		DamageTraceDelay,
		TraceDistance,
		TraceRadius,
		TraceStartSocket,
		TraceEndSocket,
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
