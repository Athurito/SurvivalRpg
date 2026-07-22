#pragma once

#include "RpgDroppedInventoryActor.h"
#include "RpgInventoryItemDefinition.h"
#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/Core/RpgWorldCollectable.h"
#include "SurvivalRpg/Equipment/RpgEquipmentDefinition.h"
#include "SurvivalRpg/Equipment/RpgEquipmentInstance.h"

#include "RpgInventoryAutomationTestTypes.generated.h"

class URpgPawnData;
class URpgPlayerInventoryLayoutDefinition;

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

/** Editor-only stackable material with no fragment-owned runtime state. */
UCLASS(NotBlueprintable, Transient)
class URpgInventoryAutomationTestMaterialDefinition final
	: public URpgInventoryItemDefinition
{
	GENERATED_BODY()

public:
	explicit URpgInventoryAutomationTestMaterialDefinition(
		const FObjectInitializer& ObjectInitializer =
			FObjectInitializer::Get());

	virtual bool IsEditorOnly() const override { return true; }
};

/** Editor-only non-stackable material that owns a nested container, used to prevent BaseStorage subtree loss. */
UCLASS(NotBlueprintable, Transient)
class URpgInventoryAutomationTestMaterialContainerDefinition final
	: public URpgInventoryItemDefinition
{
	GENERATED_BODY()

public:
	explicit URpgInventoryAutomationTestMaterialContainerDefinition(
		const FObjectInitializer& ObjectInitializer =
			FObjectInitializer::Get());

	virtual bool IsEditorOnly() const override { return true; }
};

/**
 * Editor-only fragment that owns one byte of per-instance state outside StatTags.
 * It proves that canonical stack keys and split copying include fragment payloads rather than only core state.
 */
UCLASS(NotBlueprintable, Transient)
class URpgInventoryAutomationTestStatefulFragment final
	: public URpgInventoryItemFragment
{
	GENERATED_BODY()

public:
	virtual bool IsEditorOnly() const override { return true; }
	virtual void OnInstanceCreated(
		URpgInventoryItemInstance* Instance) const override;
	virtual FName GetRuntimeStateIdentifier() const override;
	virtual int32 GetRuntimeStateVersion() const override;
	virtual bool ExportRuntimeState(
		const URpgInventoryItemInstance* Instance,
		FRpgInventoryFragmentStatePayload& OutPayload) const override;
	virtual bool ValidateRuntimeState(
		const URpgInventoryItemInstance* Instance,
		const FRpgInventoryFragmentStatePayload& Payload) const override;
	virtual bool ImportRuntimeState(
		URpgInventoryItemInstance* Instance,
		const FRpgInventoryFragmentStatePayload& Payload) const override;
	virtual void CopyRuntimeState(
		const URpgInventoryItemInstance* Source,
		URpgInventoryItemInstance* Target) const override;

	/** Changes the opaque fragment state used only by automation tests. */
	void SetTestValue(
		const URpgInventoryItemInstance* Instance,
		uint8 Value) const;

	/** Returns the opaque fragment state used only by automation tests. */
	uint8 GetTestValue(const URpgInventoryItemInstance* Instance) const;

private:
	mutable TMap<TWeakObjectPtr<URpgInventoryItemInstance>, uint8>
		TestValues;
};

/** Editor-only stackable material whose fragment payload exercises canonical keys and BaseStorage rejection. */
UCLASS(NotBlueprintable, Transient)
class URpgInventoryAutomationTestStatefulMaterialDefinition final
	: public URpgInventoryItemDefinition
{
	GENERATED_BODY()

public:
	explicit URpgInventoryAutomationTestStatefulMaterialDefinition(
		const FObjectInitializer& ObjectInitializer =
			FObjectInitializer::Get());

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

/** Editor-only fixed-orientation 2x1 item definition used by persistence validation tests. */
UCLASS(NotBlueprintable, Transient)
class URpgInventoryAutomationTestFixedWideItemDefinition final : public URpgInventoryItemDefinition
{
	GENERATED_BODY()

public:
	explicit URpgInventoryAutomationTestFixedWideItemDefinition(
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

/** Runtime fixture that counts authoritative equip lifecycle callbacks without changing production behavior. */
UCLASS(NotBlueprintable, Transient)
class URpgInventoryAutomationTestCountingEquipmentInstance final : public URpgEquipmentInstance
{
	GENERATED_BODY()

public:
	virtual void OnEquipped() override;
	virtual void OnUnequipped() override;

	int32 GetEquippedCount() const { return EquippedCount; }
	int32 GetUnequippedCount() const { return UnequippedCount; }

private:
	int32 EquippedCount = 0;
	int32 UnequippedCount = 0;
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

/** Editor-only OffHand equipment data with a visible four-kilogram load contribution. */
UCLASS(NotBlueprintable, Transient)
class URpgInventoryAutomationTestOffHandEquipmentDefinition final : public URpgEquipmentDefinition
{
	GENERATED_BODY()

public:
	explicit URpgInventoryAutomationTestOffHandEquipmentDefinition(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

/** Editor-only stackable shield used to regress OffHand activation and derived equipment-load sync. */
UCLASS(NotBlueprintable, Transient)
class URpgInventoryAutomationTestStackableOffHandItemDefinition final : public URpgInventoryItemDefinition
{
	GENERATED_BODY()

public:
	explicit URpgInventoryAutomationTestStackableOffHandItemDefinition(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool IsEditorOnly() const override { return true; }
};

/** Editor-only two-handed MainHand equipment data used to exercise runtime conflict reconciliation. */
UCLASS(NotBlueprintable, Transient)
class URpgInventoryAutomationTestTwoHandEquipmentDefinition final : public URpgEquipmentDefinition
{
	GENERATED_BODY()

public:
	explicit URpgInventoryAutomationTestTwoHandEquipmentDefinition(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

/** Editor-only 1x1 two-handed weapon used to regress MainHand/OffHand runtime replacement. */
UCLASS(NotBlueprintable, Transient)
class URpgInventoryAutomationTestTwoHandItemDefinition final : public URpgInventoryItemDefinition
{
	GENERATED_BODY()

public:
	explicit URpgInventoryAutomationTestTwoHandItemDefinition(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool IsEditorOnly() const override { return true; }
};

/** Editor-only armor equipment that can move between Chest and Head while granting persistent MaxHealth. */
UCLASS(NotBlueprintable, Transient)
class URpgInventoryAutomationTestMovableGrantEquipmentDefinition final : public URpgEquipmentDefinition
{
	GENERATED_BODY()

public:
	explicit URpgInventoryAutomationTestMovableGrantEquipmentDefinition(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

/** Editor-only armor item used to detect transient duplicate grants during a physical slot-role change. */
UCLASS(NotBlueprintable, Transient)
class URpgInventoryAutomationTestMovableGrantItemDefinition final : public URpgInventoryItemDefinition
{
	GENERATED_BODY()

public:
	explicit URpgInventoryAutomationTestMovableGrantItemDefinition(
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

/**
 * Editor-only 1x1 bag whose 4x4 item-owned container deliberately reuses a built-in Gear root id.
 * Full container handles must keep this grid distinct from the player's single-cell Gear slot.
 */
UCLASS(NotBlueprintable, Transient)
class URpgInventoryAutomationTestGearNameCollisionBagItemDefinition final
	: public URpgInventoryItemDefinition
{
	GENERATED_BODY()

public:
	explicit URpgInventoryAutomationTestGearNameCollisionBagItemDefinition(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool IsEditorOnly() const override { return true; }
};

/**
 * Editor-only malformed legacy provider whose raw traits advertise a stack of ten.
 * Runtime inventory rules must still treat every concrete provider as a single entry.
 */
UCLASS(NotBlueprintable, Transient)
class URpgInventoryAutomationTestLegacyStackableBagItemDefinition final
	: public URpgInventoryItemDefinition
{
	GENERATED_BODY()

public:
	explicit URpgInventoryAutomationTestLegacyStackableBagItemDefinition(
		const FObjectInitializer& ObjectInitializer =
			FObjectInitializer::Get());

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
	virtual void PostActorCreated() override;
	virtual void PostInitializeComponents() override;

	/** Returns the fixture-owned mutable layout definition used to author test-only groups. */
	URpgPlayerInventoryLayoutDefinition* GetMutableTestInventoryLayoutDefinition() const
	{
		return TestInventoryLayoutDefinition.Get();
	}

private:
	void InitializeTestPawnData();

	/** Transient PawnData that supplies the fixture layout without loading production assets. */
	UPROPERTY(Transient)
	TObjectPtr<URpgPawnData> TestPawnData;

	/** Per-fixture mutable layout definition; never shared with production content or other tests. */
	UPROPERTY(Transient)
	TObjectPtr<URpgPlayerInventoryLayoutDefinition> TestInventoryLayoutDefinition;
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

/**
 * Deferred-spawn drop fixture used to model a non-canonical runtime graph while
 * retaining the authoritative static pickup fallback.
 */
UCLASS(NotBlueprintable, Transient)
class ARpgInventoryAutomationTestDroppedInventoryActor final
	: public ARpgDroppedInventoryActor
{
	GENERATED_BODY()

public:
	void SetTestStaticPickupFallback(
		const FInventoryPickup& InPickupInventory)
	{
		StaticInventory = InPickupInventory;
	}
};
