#include "RpgWeaponInstance.h"

#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"

URpgWeaponInstance::URpgWeaponInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	WeaponTypeTag = RpgGameplayTags::Weapon_Type_Melee;
	WeaponFamilyTag = RpgGameplayTags::Weapon_Family_Sword;

	FRpgWeaponAttackDefinition PrimaryAttack;
	PrimaryAttack.HitReactionEventTag = RpgGameplayTags::GameplayEvent_HitReaction;
	AttackDefinitions.Add(RpgGameplayTags::Weapon_Attack_Primary, PrimaryAttack);
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

float URpgWeaponInstance::GetDistanceAttenuation(float Distance, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags) const
{
	return 1.0f;
}

float URpgWeaponInstance::GetPhysicalMaterialAttenuation(const UPhysicalMaterial* PhysicalMaterial, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags) const
{
	return 1.0f;
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
