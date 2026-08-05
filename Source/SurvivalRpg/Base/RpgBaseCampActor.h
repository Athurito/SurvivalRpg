#pragma once

#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "RpgBaseStorageSaveTypes.h"

#include "RpgBaseCampActor.generated.h"

class URpgBaseBuildableDefinition;
class URpgBaseStorageDomainAnchorComponent;
class URpgBaseStorageComponent;
class URpgBaseStorageStationComponent;
class URpgInventoryManagerComponent;
class USceneComponent;
class APlayerController;
class ARpgPersonalStorageLockerActor;
class FDataValidationContext;

/**
 * Replicated authority actor for one player/base storage hub.
 *
 * Resource materials are stored as counts in BaseStorageComponent. Instance-based gear, weapons,
 * shields, and durability-bearing items remain item instances in ArmoryInventoryComponent.
 */
UCLASS(Blueprintable)
class SURVIVALRPG_API ARpgBaseCampActor : public AActor
{
	GENERATED_BODY()

public:
	explicit ARpgBaseCampActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	/** Stable designer-authored id used by stations, crafting stations, and future save data to refer to this base. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Base Camp")
	FName GetBaseId() const { return BaseId; }

	/** Maximum placement radius in centimeters used by V1 buildable validation. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Base Camp|Building")
	float GetBuildRadius() const { return BuildRadius; }

	/** Shared resource pool for material counts owned by this base camp. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Base Camp")
	URpgBaseStorageComponent* GetBaseStorageComponent() const { return BaseStorageComponent; }

	/** Inventory for instance-based base storage such as weapons, shields, armor, and tools. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Base Camp")
	URpgInventoryManagerComponent* GetArmoryInventoryComponent() const { return ArmoryInventoryComponent; }

	/** Inventory for concrete Rift items. It is shared, server-authoritative, and limited by containment progression. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Base Camp")
	URpgInventoryManagerComponent* GetContainmentInventoryComponent() const { return ContainmentInventoryComponent; }

	/** Authority-only stable profile key that owns upgrades, decommission, and destructive Rift extraction. Clients receive no owner identifier. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Base Camp|Permissions")
	FString GetOwnerProfileKey() const { return HasAuthority() ? OwnerProfileKey : FString(); }

	/** Claims an unowned base for the controller; already-owned bases only succeed for the same stable profile. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Base Camp|Permissions")
	bool EnsureClaimedByController(APlayerController* Controller);

	/** Server-side owner permission check for a controller, pawn, or controller-owned request facade. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Base Camp|Permissions")
	bool IsBaseOwner(const AActor* RequestingActor) const;

	/** Gets or authority-spawns the owner-only personal locker for this stable player profile. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Base Camp|Personal")
	ARpgPersonalStorageLockerActor* GetOrCreatePersonalLocker(APlayerController* Controller);

	/** Finds the owner-relevant personal locker already visible to this controller without mutating gameplay state. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Base Camp|Personal")
	ARpgPersonalStorageLockerActor* FindPersonalLocker(APlayerController* Controller) const;

	/** Returns whether the exact concrete item retains stabilized runtime state in this base's Vault, Armory, or lockers. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Base Camp|Rift")
	bool IsContainmentItemStabilized(FRpgInventoryItemId ItemId) const;

	/** Changes persistent stabilization state on the exact item while it is owned by this base's containment graph. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Base Camp|Rift")
	bool SetContainmentItemStabilized(FRpgInventoryItemId ItemId, bool bStabilized);

	/** Removes only legacy Vault shadow metadata; persistent stabilization remains on the concrete item instance. */
	bool ForgetContainmentItemState(FRpgInventoryItemId ItemId);

	/** Captures the complete pointer-free base network state for WorldSave V2. */
	bool ExportBaseStorageSaveData(FRpgBaseStorageSaveData& OutSaveData, FString& OutError) const;

	/** Atomically restores internal storage, Armory, Containment, and pending owner-private locker graphs. */
	bool RestoreBaseStorageSaveData(const FRpgBaseStorageSaveData& SaveData, FString& OutError);

	/** True only after an attempted restore could not re-establish the exact pre-candidate state. */
	bool IsStorageRestoreTainted() const { return bStorageRestoreTainted; }

	/** Recomputes replicated cosmetic anchor states from authoritative storage and concrete inventory state. */
	void RefreshStorageAnchorVisuals();

	/** Returns whether the requested buildable can be placed at the transform for the requesting actor. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Base Camp|Building")
	bool CanPlaceBuildableAtTransform(const URpgBaseBuildableDefinition* BuildableDefinition, const FTransform& BuildTransform, const AActor* RequestingActor) const;

	/** Registers a runtime or placed storage station as part of this base. Called by station components. */
	void RegisterStorageStation(URpgBaseStorageStationComponent* Station);

	/** Removes a storage station from the base registry. Called by station components. */
	void UnregisterStorageStation(URpgBaseStorageStationComponent* Station);

	/** Returns storage stations currently linked on this machine. Useful for terminal UI lists. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Base Camp|Stations")
	TArray<URpgBaseStorageStationComponent*> GetStorageStations() const;

	/** Returns all upgrade tags granted by currently linked storage stations. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Base Camp|Upgrades", meta = (Categories = "Base.Storage.Upgrade"))
	FGameplayTagContainer GetGrantedStorageUpgradeTags() const;

	/** Returns true if any linked station grants the supplied upgrade tag. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Base Camp|Upgrades", meta = (Categories = "Base.Storage.Upgrade"))
	bool HasStorageUpgradeTag(FGameplayTag UpgradeTag) const;

protected:
	/** Stable id for this base camp. Replicated so UI and linked actors can display/debug their base ownership. */
	UPROPERTY(EditInstanceOnly, Replicated, BlueprintReadOnly, Category = "Base Camp")
	FName BaseId;

	/** Server-validated radius in centimeters for placing base buildables around this camp. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base Camp|Building", meta = (ClampMin = "0", UIMin = "0", Units = "cm"))
	float BuildRadius = 2500.0f;

	/** Simple replicated actor root so Blueprint children can attach base visuals. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Base Camp")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Server-authoritative material pool shared by linked stations and crafting stations. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Base Camp")
	TObjectPtr<URpgBaseStorageComponent> BaseStorageComponent;

	/** Server-authoritative instance inventory for stored equipment and non-material items. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Base Camp")
	TObjectPtr<URpgInventoryManagerComponent> ArmoryInventoryComponent;

	/** Server-authoritative concrete item graph for unstable or stabilized Rift objects. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Base Camp")
	TObjectPtr<URpgInventoryManagerComponent> ContainmentInventoryComponent;

	/** Fixed Materials-domain anchor; capacity remains owned by BaseStorageComponent. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Base Camp|Anchors")
	TObjectPtr<URpgBaseStorageDomainAnchorComponent> MaterialDepotAnchor;

	/** Fixed shared Armory-domain anchor; item instances remain in ArmoryInventoryComponent. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Base Camp|Anchors")
	TObjectPtr<URpgBaseStorageDomainAnchorComponent> ArmoryAnchor;

	/** Fixed Rift-domain anchor; it remains visually offline until containment capability and slots are installed. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Base Camp|Anchors")
	TObjectPtr<URpgBaseStorageDomainAnchorComponent> RiftVaultAnchor;

	/** Authority-only profile permission owner. Manually persisted by the base save envelope and never replicated to clients. */
	UPROPERTY(Transient)
	FString OwnerProfileKey;

	/** Legacy V2 restore shadow used only while migrating old saves; live state is replicated by each item instance. */
	UPROPERTY(Transient)
	TArray<FRpgBaseContainmentItemStateSaveData> ContainmentStates;

private:
	FString ResolveProfileKey(const APlayerController* Controller) const;
	APlayerController* ResolvePlayerController(const AActor* RequestingActor) const;
	void DestroyPersonalLockers();

	TArray<TWeakObjectPtr<URpgBaseStorageStationComponent>> RegisteredStorageStations;
	TMap<FString, TWeakObjectPtr<ARpgPersonalStorageLockerActor>> PersonalLockersByProfile;
	TMap<FString, FRpgInventoryGraphSaveData> PendingPersonalLockerGraphs;
	bool bStorageRestoreTainted = false;
};
