// Fill out your copyright notice in the Description page of Project Settings.

#include "RpgDefenseSet.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"

URpgDefenseSet::URpgDefenseSet()
	: Armor(0.0f)
	, BlockChance(0.0f)
	, CriticalHitResistance(0.0f)
	, Stagger(0.0f)
	, MaxStagger(100.0f)
	, BlockAngleDegrees(120.0f)
	, BlockStaminaCost(20.0f)
	, BlockDamageReduction(1.0f)
	, BlockStaggerDamageMultiplier(0.3f)
	, PerfectBlockStaminaRestore(15.0f)
	, PerfectBlockStaggerDamage(35.0f)
{
}

void URpgDefenseSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);
	ClampDefenseAttribute(Attribute, NewValue);
}

void URpgDefenseSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	ClampDefenseAttribute(Attribute, NewValue);
}

void URpgDefenseSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetStaggerAttribute())
	{
		const float NewStagger = GetStagger();
		const float OldStagger = FMath::Max(0.0f, NewStagger - Data.EvaluatedData.Magnitude);
		HandleStaggerThreshold(Data, OldStagger, NewStagger);
	}
}

void URpgDefenseSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(URpgDefenseSet, Armor, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URpgDefenseSet, BlockChance, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URpgDefenseSet, CriticalHitResistance, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URpgDefenseSet, Stagger, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URpgDefenseSet, MaxStagger, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URpgDefenseSet, BlockAngleDegrees, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URpgDefenseSet, BlockStaminaCost, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URpgDefenseSet, BlockDamageReduction, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URpgDefenseSet, BlockStaggerDamageMultiplier, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URpgDefenseSet, PerfectBlockStaminaRestore, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URpgDefenseSet, PerfectBlockStaggerDamage, COND_None, REPNOTIFY_Always);
}

void URpgDefenseSet::OnRep_Armor(const FGameplayAttributeData& OldValue) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URpgDefenseSet, Armor, OldValue);
}

void URpgDefenseSet::OnRep_BlockChance(const FGameplayAttributeData& OldValue) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URpgDefenseSet, BlockChance, OldValue);
}

void URpgDefenseSet::OnRep_CriticalHitResistance(const FGameplayAttributeData& OldValue) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URpgDefenseSet, CriticalHitResistance, OldValue);
}

void URpgDefenseSet::OnRep_Stagger(const FGameplayAttributeData& OldValue) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URpgDefenseSet, Stagger, OldValue);
}

void URpgDefenseSet::OnRep_MaxStagger(const FGameplayAttributeData& OldValue) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URpgDefenseSet, MaxStagger, OldValue);
}

void URpgDefenseSet::OnRep_BlockAngleDegrees(const FGameplayAttributeData& OldValue) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URpgDefenseSet, BlockAngleDegrees, OldValue);
}

void URpgDefenseSet::OnRep_BlockStaminaCost(const FGameplayAttributeData& OldValue) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URpgDefenseSet, BlockStaminaCost, OldValue);
}

void URpgDefenseSet::OnRep_BlockDamageReduction(const FGameplayAttributeData& OldValue) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URpgDefenseSet, BlockDamageReduction, OldValue);
}

void URpgDefenseSet::OnRep_BlockStaggerDamageMultiplier(const FGameplayAttributeData& OldValue) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URpgDefenseSet, BlockStaggerDamageMultiplier, OldValue);
}

void URpgDefenseSet::OnRep_PerfectBlockStaminaRestore(const FGameplayAttributeData& OldValue) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URpgDefenseSet, PerfectBlockStaminaRestore, OldValue);
}

void URpgDefenseSet::OnRep_PerfectBlockStaggerDamage(const FGameplayAttributeData& OldValue) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URpgDefenseSet, PerfectBlockStaggerDamage, OldValue);
}

void URpgDefenseSet::ClampDefenseAttribute(const FGameplayAttribute& Attribute, float& NewValue) const
{
	if (Attribute == GetStaggerAttribute())
	{
		NewValue = ClampAttribute(NewValue, 0.0f, GetMaxStagger());
	}
	else if (Attribute == GetMaxStaggerAttribute())
	{
		NewValue = FMath::Max(1.0f, NewValue);
	}
	else if (Attribute == GetBlockAngleDegreesAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, 360.0f);
	}
	else if (Attribute == GetBlockDamageReductionAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, 1.0f);
	}
	else if (Attribute == GetArmorAttribute() ||
		Attribute == GetBlockChanceAttribute() ||
		Attribute == GetCriticalHitResistanceAttribute() ||
		Attribute == GetBlockStaminaCostAttribute() ||
		Attribute == GetBlockStaggerDamageMultiplierAttribute() ||
		Attribute == GetPerfectBlockStaminaRestoreAttribute() ||
		Attribute == GetPerfectBlockStaggerDamageAttribute())
	{
		NewValue = FMath::Max(0.0f, NewValue);
	}
}

void URpgDefenseSet::HandleStaggerThreshold(const FGameplayEffectModCallbackData& Data, float OldValue, float NewValue)
{
	const float Threshold = FMath::Max(1.0f, GetMaxStagger());
	if (OldValue >= Threshold || NewValue < Threshold)
	{
		return;
	}

	URpgAbilitySystemComponent* TargetASC = Cast<URpgAbilitySystemComponent>(&Data.Target);
	AActor* TargetActor = TargetASC ? TargetASC->GetAvatarActor() : nullptr;
	if (!TargetASC || !TargetActor || !TargetActor->HasAuthority())
	{
		return;
	}

	if (!TargetASC->HasMatchingGameplayTag(RpgGameplayTags::State_Staggered))
	{
		FGameplayEventData Payload;
		Payload.EventTag = RpgGameplayTags::GameplayEvent_Stagger;
		Payload.Instigator = Data.EffectSpec.GetContext().GetOriginalInstigator();
		Payload.Target = TargetActor;
		Payload.ContextHandle = Data.EffectSpec.GetContext();
		Payload.EventMagnitude = NewValue;

		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(TargetActor, Payload.EventTag, Payload);
	}

	TargetASC->SetNumericAttributeBase(GetStaggerAttribute(), 0.0f);
}
