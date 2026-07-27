#pragma once

#include "Components/ControllerComponent.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameplayTagContainer.h"
#include "SurvivalRpg/Inventory/RpgInventoryGraphTypes.h"
#include "SurvivalRpg/Inventory/RpgPlayerInventoryLayoutTypes.h"

#include "RpgActionBarComponent.generated.h"

class ARpgPlayerController;
class URpgAbilitySystemComponent;
class URpgInventoryItemDefinition;
class URpgInventoryItemInstance;
class URpgPlayerInventoryLayoutComponent;
struct FRpgInventoryChangeMessage;
struct FRpgPlayerInventoryLayoutChangedMessage;

/** Payload type shared by keyboard 1..8 and the controller gameplay radial. */
UENUM(BlueprintType)
enum class ERpgActionBarSlotType : uint8
{
	/** The slot has no assigned action. */
	Empty = 0,

	/** Uses a compatible stack from designer-marked quick-access grids, preferring one persistent item id. */
	Consumable = 1,

	/** Activates or holsters the current item in a semantic Carry role such as Primary, Secondary, or OffHand. */
	CarrySlot = 2,

	/** Presses the one granted GAS spec identified by a unique semantic AbilityId. */
	Ability = 3,

	/** Deprecated serialized enum name migrated to Consumable during server revalidation. */
	InventorySlotBinding = 4 UMETA(Hidden),

	/** Deprecated serialized enum name migrated to CarrySlot during server revalidation. */
	CarrySlotBinding = 5 UMETA(Hidden)
};

/** Stable reason exposed to HUD/radial presentation when a non-empty binding cannot currently activate. */
UENUM(BlueprintType)
enum class ERpgQuickAccessBlockedReason : uint8
{
	/** The binding is valid and currently activatable. */
	None,

	/** Empty bindings intentionally have no action. */
	Empty,

	/** The configured Carry role is not present in the active data-driven player layout. */
	InvalidCarryRole,

	/** No compatible item currently exists in the bound Carry or quick-access grids. */
	MissingItem,

	/** The selected item definition is not a server-validated usable consumable. */
	NotConsumable,

	/** No currently granted GAS spec owns the selected AbilityId. */
	MissingAbility,

	/** Several granted specs own the same AbilityId; activation is blocked as a content configuration error. */
	AmbiguousAbility
};

/** Owner-only replicated and save-friendly state for one shared quick-access binding. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgQuickAccessBinding
{
	GENERATED_BODY()

	/** Assignment type consumed by both keyboard 1..8 and controller radial activation. */
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Quick Access")
	ERpgActionBarSlotType SlotType = ERpgActionBarSlotType::Empty;

	/**
	 * Compatibility/presentation address last used to create this binding.
	 * Consumable resolution no longer treats this mutable cell as authoritative.
	 */
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Quick Access")
	FRpgInventorySlotAddress SlotAddress;

	/** Stable data-driven Carry role whose current item is activated; the physical container id may change independently. */
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Quick Access", meta = (Categories = "Rpg.Inventory.Layout.Role"))
	FGameplayTag CarrySemanticRole;

	/** Consumable definition resolved only from designer-marked quick-access grids. */
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Quick Access")
	TSubclassOf<URpgInventoryItemDefinition> ConsumableDefinition;

	/** Preferred concrete stack. If absent, another compatible quick-access stack is selected deterministically. */
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Quick Access")
	FRpgInventoryItemId PreferredItemId;

	/** Semantic id that must resolve to exactly one currently granted GAS ability spec. */
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Quick Access")
	FGameplayTag AbilityId;

	/** Server-derived availability seam used by the HUD and radial; false never authorizes gameplay by itself. */
	UPROPERTY(BlueprintReadOnly, Category = "Quick Access")
	bool bAvailable = false;

	/** Server-derived reason shown when a configured binding cannot currently activate. */
	UPROPERTY(BlueprintReadOnly, Category = "Quick Access")
	ERpgQuickAccessBlockedReason BlockedReason = ERpgQuickAccessBlockedReason::Empty;

	bool IsEmpty() const { return SlotType == ERpgActionBarSlotType::Empty; }
	void Reset() { *this = FRpgQuickAccessBinding(); }

private:
	friend class URpgActionBarComponent;

	/** Historical Carry root id retained only so authoritative restore can promote version-one Quick Access saves. */
	UPROPERTY(SaveGame, NotReplicated, meta = (DeprecatedProperty, DeprecationMessage = "Use CarrySemanticRole. This field is retained only for legacy save migration."))
	FName CarryRole_DEPRECATED = NAME_None;
};

/** Backward-compatible actionbar slot name; every slot now carries the canonical quick-access binding payload. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgActionBarSlot : public FRpgQuickAccessBinding
{
	GENERATED_BODY()
};

/** Gameplay message sent to the owning client when the general actionbar assignment changes. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgActionBarSlotsChangedMessage
{
	GENERATED_BODY()

	/** Controller that owns the actionbar. */
	UPROPERTY(BlueprintReadOnly, Category = "Action Bar")
	TObjectPtr<APlayerController> Owner = nullptr;

	/** Actionbar component that changed. */
	UPROPERTY(BlueprintReadOnly, Category = "Action Bar")
	TObjectPtr<UActorComponent> ActionBarComponent = nullptr;
};

/**
 * Controller-owned general actionbar for player-configured slot bindings on keys 1..8.
 *
 * The component stores owner-only selection state. Item use, carry activation, and inventory ownership remain
 * validated by the existing server paths; the actionbar never owns item instances directly.
 */
UCLASS(Blueprintable, meta = (BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgActionBarComponent : public UControllerComponent
{
	GENERATED_BODY()

public:
	explicit URpgActionBarComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Number of shared keyboard/radial bindings. This contract is always exactly eight. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Action Bar")
	int32 GetNumSlots() const { return 8; }

	/** Returns a copy of all owner-only actionbar slots for UI display. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Action Bar")
	const TArray<FRpgActionBarSlot>& GetSlots() const { return Slots; }

	/** Returns exactly eight canonical bindings for disk-save and the shared keyboard/controller radial UI. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Quick Access")
	TArray<FRpgQuickAccessBinding> GetQuickAccessBindings() const;

	/** Returns one actionbar slot by internal zero-based index 0..7, or an empty slot for invalid indices. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Action Bar")
	FRpgActionBarSlot GetSlot(int32 SlotIndex) const;

	/**
	 * Immediately applies a validated consumable-address binding on server authority.
	 * Server-side transaction components use this instead of nesting a second Server RPC and reading stale state.
	 */
	bool TryBindInventorySlotToSlotAuthority(int32 SlotIndex, const FRpgInventorySlotAddress& SlotAddress);

	/**
	 * Immediately applies a semantic Carry binding on server authority.
	 * The binding follows the Carry role, not the concrete item currently occupying that address.
	 */
	bool TryBindCarrySlotToSlotAuthority(int32 SlotIndex, const FRpgInventorySlotAddress& SlotAddress);

	/** Immediately clears one Quick Access binding on server authority and reports whether the slot is empty. */
	bool TryClearSlotAuthority(int32 SlotIndex);

	/** Compatibility adapter that creates a definition+item-id consumable binding from a quick-access grid cell. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Rpg|Action Bar")
	void RequestBindInventorySlotToSlot(int32 SlotIndex, FRpgInventorySlotAddress SlotAddress);

	/** Compatibility adapter that stores the address's data-driven Carry role instead of a concrete item. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Rpg|Action Bar")
	void RequestBindCarrySlotToSlot(int32 SlotIndex, FRpgInventorySlotAddress SlotAddress);

	/** Binds a consumable definition and preferred persistent stack id to one shared quick-access slot. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Rpg|Quick Access")
	void RequestBindConsumableToSlot(
		int32 SlotIndex,
		TSubclassOf<URpgInventoryItemDefinition> ConsumableDefinition,
		FRpgInventoryItemId PreferredItemId);

	/** Binds a semantic AbilityId. Missing grants stay saved but blocked; ambiguous ids are never activated. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Rpg|Quick Access")
	void RequestBindAbilityToSlot(int32 SlotIndex, FGameplayTag AbilityId);

	/** Clears one general actionbar slot. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Rpg|Action Bar")
	void RequestClearSlot(int32 SlotIndex);

	/**
	 * Restores up to eight pointer-free bindings after the inventory graph and layout are ready.
	 * Legacy Carry roots may be promoted only for player-save schema v1; current schemas fail closed on legacy data.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rpg|Quick Access")
	void RestoreQuickAccessBindings(
		const TArray<FRpgQuickAccessBinding>& SavedBindings,
		bool bAllowLegacyCarryRootMigration);

	/** Revalidates all eight bindings after inventory, equipment, progression, or GAS grants change. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rpg|Quick Access")
	void RefreshBindings();

	/** Handles local key/button press for one actionbar slot. */
	void ActivateSlot(int32 SlotIndex);

	/** Handles local key/button release for one actionbar slot. */
	void ReleaseSlot(int32 SlotIndex);

	/** One-shot activation used when the controller radial commits on stick release. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Quick Access")
	void TriggerSlot(int32 SlotIndex);

	/** Input tag used by the given actionbar slot index, or invalid for out-of-range indices. */
	static FGameplayTag GetInputTagForSlotIndex(int32 SlotIndex);

protected:
	UFUNCTION()
	void OnRep_Slots();

private:
	void EnsureSlotCount();
	void BroadcastSlotsChanged() const;
	bool IsValidSlotIndex(int32 SlotIndex) const;
	ARpgPlayerController* GetRpgPlayerController() const;
	URpgInventoryItemInstance* ResolveConsumableItem(const FRpgActionBarSlot& Slot, FRpgInventorySlotAddress* OutAddress = nullptr) const;
	bool TryResolveCarrySemanticRole(FGameplayTag CarrySemanticRole, FRpgInventorySlotAddress& OutAddress) const;
	bool TryResolveLegacyCarryRoot(FName LegacyCarryRoot, FGameplayTag& OutCarrySemanticRole) const;
	bool TryPromoteLegacyCarrySemanticRole(FRpgQuickAccessBinding& Slot) const;
	void ClearDuplicateBinding(int32 TargetSlotIndex, const FRpgActionBarSlot& Binding);
	void RefreshBindingsInternal(bool bForceBroadcast);
	void RefreshBindingAvailability(int32 SlotIndex, FRpgActionBarSlot& Slot, URpgAbilitySystemComponent* AbilitySystemComponent);
	static bool AreBindingsEquivalent(const FRpgQuickAccessBinding& A, const FRpgQuickAccessBinding& B);
	void RegisterStateListeners();
	void UnregisterStateListeners();
	void HandleInventoryChanged(FGameplayTag Channel, const FRpgInventoryChangeMessage& Message);
	void HandleInventoryLayoutChanged(FGameplayTag Channel, const FRpgPlayerInventoryLayoutChangedMessage& Message);

	/** Owner-only replicated actionbar state. */
	UPROPERTY(ReplicatedUsing = OnRep_Slots, SaveGame)
	TArray<FRpgActionBarSlot> Slots;

	FGameplayMessageListenerHandle InventoryChangedHandle;
	FGameplayMessageListenerHandle InventoryLayoutChangedHandle;
};
