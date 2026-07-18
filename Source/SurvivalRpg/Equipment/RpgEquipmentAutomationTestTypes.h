#pragma once

#include "AbilitySystemInterface.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "RpgEquipmentDefinition.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySet.h"

#include "RpgEquipmentAutomationTestTypes.generated.h"

class URpgAbilitySystemComponent;
class URpgEquipmentManagerComponent;
class URpgHealthSet;

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

private:
	UPROPERTY()
	TObjectPtr<URpgAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<URpgHealthSet> HealthSet;

	UPROPERTY()
	TObjectPtr<URpgEquipmentManagerComponent> EquipmentManagerComponent;
};
