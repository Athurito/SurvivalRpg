#pragma once

#include "AbilitySystemInterface.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "RpgEquipmentDefinition.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySet.h"
#include "SurvivalRpg/Inventory/Itemization/RpgItemizationProfile.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"

#include "RpgEquipmentAutomationTestTypes.generated.h"

class URpgAbilitySystemComponent;
class URpgCombatSet;
class URpgEquipmentManagerComponent;
class URpgHealthSet;
class URpgPrimarySet;
class URpgStaminaSet;

/** Common-only generated weapon profile used to exercise equipment-owned dynamic effects without content assets. */
UCLASS(NotBlueprintable, Transient)
class URpgEquipmentAutomationTestItemizationProfile final : public URpgItemizationProfile
{
	GENERATED_BODY()

public:
	explicit URpgEquipmentAutomationTestItemizationProfile(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool IsEditorOnly() const override { return true; }
};

/** Non-stackable generated weapon whose rolls include local damage and a global MaxStamina value. */
UCLASS(NotBlueprintable, Transient)
class URpgEquipmentAutomationTestItemizedWeaponDefinition final : public URpgInventoryItemDefinition
{
	GENERATED_BODY()

public:
	explicit URpgEquipmentAutomationTestItemizedWeaponDefinition(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool IsEditorOnly() const override { return true; }
};

/** Infinite +500 MaxHealth effect used to reproduce persistent equipment-stat regressions. */
UCLASS(NotBlueprintable, Transient)
class URpgEquipmentAutomationTestMaxHealthEffect final : public UGameplayEffect
{
	GENERATED_BODY()

public:
	URpgEquipmentAutomationTestMaxHealthEffect();
};

/** Ability set containing the persistent MaxHealth effect used by equipment automation tests. */
UCLASS(NotBlueprintable, Transient)
class URpgEquipmentAutomationTestHealthAbilitySet final : public URpgAbilitySet
{
	GENERATED_BODY()

public:
	URpgEquipmentAutomationTestHealthAbilitySet();
};

/** Head equipment whose persistent AbilitySet raises MaxHealth from 100 to 600. */
UCLASS(NotBlueprintable, Transient)
class URpgEquipmentAutomationTestHelmetDefinition final : public URpgEquipmentDefinition
{
	GENERATED_BODY()

public:
	explicit URpgEquipmentAutomationTestHelmetDefinition(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

/** Main-hand equipment with no stat grants, used to exercise an unrelated loadout change. */
UCLASS(NotBlueprintable, Transient)
class URpgEquipmentAutomationTestSwordDefinition final : public URpgEquipmentDefinition
{
	GENERATED_BODY()

public:
	explicit URpgEquipmentAutomationTestSwordDefinition(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

/** Off-hand definition used to prove rolled weapon damage is never aggregated into global BaseDamage. */
UCLASS(NotBlueprintable, Transient)
class URpgEquipmentAutomationTestOffHandDefinition final : public URpgEquipmentDefinition
{
	GENERATED_BODY()

public:
	explicit URpgEquipmentAutomationTestOffHandDefinition(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

/** Minimal authoritative GAS pawn carrying the real equipment manager and health attributes. */
UCLASS(NotBlueprintable, Transient)
class ARpgEquipmentAutomationTestPawn final : public APawn, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	explicit ARpgEquipmentAutomationTestPawn(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void PostInitializeComponents() override;

	URpgAbilitySystemComponent* GetRpgAbilitySystemComponent() const { return AbilitySystemComponent; }
	URpgEquipmentManagerComponent* GetEquipmentManagerComponent() const { return EquipmentManagerComponent; }
	URpgHealthSet* GetHealthSet() const { return HealthSet; }
	URpgPrimarySet* GetPrimarySet() const { return PrimarySet; }
	URpgCombatSet* GetCombatSet() const { return CombatSet; }
	URpgStaminaSet* GetStaminaSet() const { return StaminaSet; }

private:
	UPROPERTY()
	TObjectPtr<URpgAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<URpgHealthSet> HealthSet;

	UPROPERTY()
	TObjectPtr<URpgPrimarySet> PrimarySet;

	UPROPERTY()
	TObjectPtr<URpgCombatSet> CombatSet;

	UPROPERTY()
	TObjectPtr<URpgStaminaSet> StaminaSet;

	UPROPERTY()
	TObjectPtr<URpgEquipmentManagerComponent> EquipmentManagerComponent;
};
