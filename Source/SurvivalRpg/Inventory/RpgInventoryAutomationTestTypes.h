#pragma once

#include "RpgInventoryItemDefinition.h"
#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/Core/RpgWorldCollectable.h"
#include "SurvivalRpg/Equipment/RpgEquipmentDefinition.h"

#include "RpgInventoryAutomationTestTypes.generated.h"

/** Editor-only 1x1 non-stackable item definition used by inventory automation tests. */
UCLASS(NotBlueprintable, Transient)
class URpgInventoryAutomationTestUnitItemDefinition final : public URpgInventoryItemDefinition
{
	GENERATED_BODY()

public:
	explicit URpgInventoryAutomationTestUnitItemDefinition(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool IsEditorOnly() const override { return true; }
};

/** Editor-only 1x1 stackable item definition with a maximum stack size of ten. */
UCLASS(NotBlueprintable, Transient)
class URpgInventoryAutomationTestStackItemDefinition final : public URpgInventoryItemDefinition
{
	GENERATED_BODY()

public:
	explicit URpgInventoryAutomationTestStackItemDefinition(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool IsEditorOnly() const override { return true; }
};

/** Editor-only no-op ability class used to make usable-item context policy deterministic. */
UCLASS(NotBlueprintable, Transient)
class URpgInventoryAutomationTestUseAbility final : public URpgGameplayAbility
{
	GENERATED_BODY()

public:
	virtual bool IsEditorOnly() const override { return true; }
};

/** Editor-only usable item restricted to the owning player's inventory. */
UCLASS(NotBlueprintable, Transient)
class URpgInventoryAutomationTestUsableItemDefinition final : public URpgInventoryItemDefinition
{
	GENERATED_BODY()

public:
	explicit URpgInventoryAutomationTestUsableItemDefinition(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool IsEditorOnly() const override { return true; }
};

/** Editor-only item whose manual-drop policy is explicitly disabled. */
UCLASS(NotBlueprintable, Transient)
class URpgInventoryAutomationTestNoDropItemDefinition final : public URpgInventoryItemDefinition
{
	GENERATED_BODY()

public:
	explicit URpgInventoryAutomationTestNoDropItemDefinition(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool IsEditorOnly() const override { return true; }
};

/** Editor-only rotatable 2x1 item definition used by spatial and sort tests. */
UCLASS(NotBlueprintable, Transient)
class URpgInventoryAutomationTestWideItemDefinition final : public URpgInventoryItemDefinition
{
	GENERATED_BODY()

public:
	explicit URpgInventoryAutomationTestWideItemDefinition(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool IsEditorOnly() const override { return true; }
};

/** Editor-only 3x2 item definition used to regress size-asymmetric displacement. */
UCLASS(NotBlueprintable, Transient)
class URpgInventoryAutomationTestLargeItemDefinition final : public URpgInventoryItemDefinition
{
	GENERATED_BODY()

public:
	explicit URpgInventoryAutomationTestLargeItemDefinition(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool IsEditorOnly() const override { return true; }
};

/** Editor-only equipment data that permits the spatial test weapon only in the MainHand Carry role. */
UCLASS(NotBlueprintable, Transient)
class URpgInventoryAutomationTestWeaponEquipmentDefinition final : public URpgEquipmentDefinition
{
	GENERATED_BODY()

public:
	explicit URpgInventoryAutomationTestWeaponEquipmentDefinition(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

/** Editor-only 1x1 MainHand weapon used to exercise real Carry and Quick Access authority paths. */
UCLASS(NotBlueprintable, Transient)
class URpgInventoryAutomationTestWeaponItemDefinition final : public URpgInventoryItemDefinition
{
	GENERATED_BODY()

public:
	explicit URpgInventoryAutomationTestWeaponItemDefinition(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool IsEditorOnly() const override { return true; }
};

/** Editor-only stackable MainHand item used to regress partial transfers of assigned player items. */
UCLASS(NotBlueprintable, Transient)
class URpgInventoryAutomationTestStackableWeaponItemDefinition final : public URpgInventoryItemDefinition
{
	GENERATED_BODY()

public:
	explicit URpgInventoryAutomationTestStackableWeaponItemDefinition(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool IsEditorOnly() const override { return true; }
};

/**
 * Editor-only Shield-category item that is deliberately MainHand-only.
 * It isolates Carry-role validation from the category filter in placement-policy tests.
 */
UCLASS(NotBlueprintable, Transient)
class URpgInventoryAutomationTestMainHandShieldItemDefinition final : public URpgInventoryItemDefinition
{
	GENERATED_BODY()

public:
	explicit URpgInventoryAutomationTestMainHandShieldItemDefinition(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool IsEditorOnly() const override { return true; }
};

/** Editor-only 1x1 bag whose 4x4 Main grid permits nested containers through depth four. */
UCLASS(NotBlueprintable, Transient)
class URpgInventoryAutomationTestBagItemDefinition final : public URpgInventoryItemDefinition
{
	GENERATED_BODY()

public:
	explicit URpgInventoryAutomationTestBagItemDefinition(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool IsEditorOnly() const override { return true; }
};

/** Editor-only equipment data that gives the spatial test backpack 7.5 kg of load. */
UCLASS(NotBlueprintable, Transient)
class URpgInventoryAutomationTestBagEquipmentDefinition final : public URpgEquipmentDefinition
{
	GENERATED_BODY()

public:
	explicit URpgInventoryAutomationTestBagEquipmentDefinition(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

/** Editor-only equipment data for a 30 kg armor item used to prove that nested contents are weightless. */
UCLASS(NotBlueprintable, Transient)
class URpgInventoryAutomationTestHeavyEquipmentDefinition final : public URpgEquipmentDefinition
{
	GENERATED_BODY()

public:
	explicit URpgInventoryAutomationTestHeavyEquipmentDefinition(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

/** Editor-only 1x1 armor item carrying a deliberately large equipment-load value. */
UCLASS(NotBlueprintable, Transient)
class URpgInventoryAutomationTestHeavyItemDefinition final : public URpgInventoryItemDefinition
{
	GENERATED_BODY()

public:
	explicit URpgInventoryAutomationTestHeavyItemDefinition(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool IsEditorOnly() const override { return true; }
};

/** Concrete controller fixture exposing the real controller-owned layout and equipment-load components. */
UCLASS(NotBlueprintable, Transient)
class ARpgInventoryAutomationTestPlayerController final : public ARpgPlayerController
{
	GENERATED_BODY()
};

/** Player-state fixture that keeps the real inventory component without requiring an Experience/GameState. */
UCLASS(NotBlueprintable, Transient)
class ARpgInventoryAutomationTestPlayerState final : public ARpgPlayerState
{
	GENERATED_BODY()

public:
	virtual void PostInitializeComponents() override;
};

/** Concrete pickup fixture that exposes deterministic instance batches to inventory automation tests. */
UCLASS(NotBlueprintable, Transient)
class ARpgInventoryAutomationTestPickupActor final : public ARpgWorldCollectable
{
	GENERATED_BODY()

public:
	void SetTestPickupInventory(const FInventoryPickup& InPickupInventory)
	{
		StaticInventory = InPickupInventory;
	}
};
