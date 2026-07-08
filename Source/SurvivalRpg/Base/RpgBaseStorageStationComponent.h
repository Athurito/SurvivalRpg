#pragma once

#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "RpgBaseStorageComponent.h"
#include "RpgBaseStorageUpgradeDefinition.h"
#include "SurvivalRpg/Interaction/IInteractableTarget.h"

#include "RpgBaseStorageStationComponent.generated.h"

class ARpgBaseCampActor;
class URpgInventoryItemDefinition;
class URpgInventoryManagerComponent;

/** UI and access behavior for a physical base storage station. */
UENUM(BlueprintType)
enum class ERpgBaseStorageStationMode : uint8
{
	/** Full terminal access to resources, armory, stations, and upgrades. */
	Terminal,

	/** Filtered access intended for one physical storage unit such as a wood pile or ore rack. */
	ResourceUnit
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRpgBaseStorageStationUpgradesChanged, URpgBaseStorageStationComponent*, Station);

/**
 * Physical interaction point for a linked base camp.
 *
 * Stations do not own items. They grant resource capacity to the linked base and provide validated
 * access to the base resource pool and armory inventory.
 */
UCLASS(Blueprintable, ClassGroup = (Base), meta = (BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgBaseStorageStationComponent : public UActorComponent, public IInteractableTarget
{
	GENERATED_BODY()

public:
	explicit URpgBaseStorageStationComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void GatherInteractionOptions(const FInteractionQuery& InteractQuery, FInteractionOptionBuilder& InteractionBuilder) override;

	/** Returns the base camp that owns the shared storage pools exposed by this station. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Base Storage")
	ARpgBaseCampActor* GetBaseCamp() const { return LinkedBaseCamp; }

	/** Runtime-links this placed or spawned station to a base camp. Server-authoritative. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Base Storage")
	void SetLinkedBaseCamp(ARpgBaseCampActor* NewBaseCamp);

	/** Returns the linked material-count storage pool, or null when no base camp is linked. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Base Storage")
	URpgBaseStorageComponent* GetBaseStorage() const;

	/** Returns the linked armory inventory for instance items, or null when no base camp is linked. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Base Storage")
	URpgInventoryManagerComponent* GetArmoryInventory() const;

	/** Returns whether this station opens the full base terminal or a filtered resource-unit view. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Base Storage")
	ERpgBaseStorageStationMode GetStationMode() const { return StationMode; }

	/** Returns semantic station tags used by upgrade definitions and UI filters. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Base Storage", meta = (Categories = "Base.Storage.Station"))
	FGameplayTagContainer GetStationTags() const { return StationTags; }

	/** Returns the material definitions visible through this station. Empty means full terminal access. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Base Storage")
	TArray<TSubclassOf<URpgInventoryItemDefinition>> GetAllowedResourceDefinitions() const;

	/** Returns true when this station should show or allow a specific material resource. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Base Storage")
	bool AllowsResourceDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition) const;

	/** Returns true when the requesting actor is allowed to interact with this station right now. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Base Storage")
	bool CanActorAccess(const AActor* RequestingActor) const;

	/** Runtime access toggle for locked, damaged, or scripted stations. Server-authoritative. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Base Storage")
	void SetStationAccessible(bool bNewAccessible);

	/** Returns installed storage-upgrade assets in replicated server order. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Base Storage|Upgrades")
	TArray<URpgBaseStorageUpgradeDefinition*> GetInstalledUpgrades() const;

	/** Returns true when this station already has the supplied upgrade asset installed. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Base Storage|Upgrades")
	bool HasInstalledUpgrade(const URpgBaseStorageUpgradeDefinition* UpgradeDefinition) const;

	/** Returns true when any installed upgrade grants the supplied tag. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Base Storage|Upgrades", meta = (Categories = "Base.Storage.Upgrade"))
	bool HasUpgradeTag(FGameplayTag UpgradeTag) const;

	/** Returns all gameplay tags granted by installed upgrades. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Base Storage|Upgrades", meta = (Categories = "Base.Storage.Upgrade"))
	FGameplayTagContainer GetGrantedUpgradeTags() const;

	/** Returns true when the upgrade can be installed on this station before checking material costs. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Base Storage|Upgrades")
	bool CanInstallUpgrade(const URpgBaseStorageUpgradeDefinition* UpgradeDefinition) const;

	/** Installs an upgrade and applies its capacity bonuses. Server-authoritative; costs are paid by caller. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Base Storage|Upgrades")
	bool InstallUpgrade(URpgBaseStorageUpgradeDefinition* UpgradeDefinition);

	/** Returns the source order used when paying material costs for station upgrades. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Base Storage|Upgrades")
	ERpgBaseStorageUpgradeCostConsumeOrder GetUpgradeCostConsumeOrder() const { return UpgradeCostConsumeOrder; }

	/** Broadcast when installed upgrades replicate or change on the server. */
	UPROPERTY(BlueprintAssignable, Category = "Base Storage|Upgrades")
	FRpgBaseStorageStationUpgradesChanged OnInstalledUpgradesChanged;

protected:
	UFUNCTION()
	void OnRep_LinkedBaseCamp();

	UFUNCTION()
	void OnRep_InstalledUpgrades();

	/** Interaction option shown by the Lyra-style interaction scan when the station is usable. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base Storage")
	FInteractionOption OpenStationOption;

	/** Determines whether this interaction point opens a full terminal or a filtered storage-unit view. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base Storage")
	ERpgBaseStorageStationMode StationMode = ERpgBaseStorageStationMode::ResourceUnit;

	/** Semantic station tags used by upgrade definitions and UI grouping. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base Storage", meta = (Categories = "Base.Storage.Station"))
	FGameplayTagContainer StationTags;

	/** Base camp that receives this station's capacity bonuses and owns the shared pools. */
	UPROPERTY(EditInstanceOnly, ReplicatedUsing = OnRep_LinkedBaseCamp, BlueprintReadOnly, Category = "Base Storage")
	TObjectPtr<ARpgBaseCampActor> LinkedBaseCamp;

	/** Resource capacity added while this station exists and is linked on the authority. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base Storage")
	TArray<FRpgBaseResourceCapacity> CapacityBonuses;

	/** Explicit material definitions visible through ResourceUnit mode. Empty falls back to capacity definitions. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base Storage")
	TArray<TSubclassOf<URpgInventoryItemDefinition>> AllowedResourceDefinitions;

	/** Installed upgrade assets replicated to clients for terminal UI and crafting-output unlock checks. */
	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_InstalledUpgrades, BlueprintReadOnly, Category = "Base Storage|Upgrades")
	TArray<TObjectPtr<URpgBaseStorageUpgradeDefinition>> InstalledUpgrades;

	/** Source order used when paying upgrade costs from player inventory and linked base storage. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base Storage|Upgrades")
	ERpgBaseStorageUpgradeCostConsumeOrder UpgradeCostConsumeOrder = ERpgBaseStorageUpgradeCostConsumeOrder::BaseThenPlayer;

	/** Maximum direct interaction distance in centimeters. Zero or below allows access at any distance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base Storage", meta = (ClampMin = "0", UIMin = "0", Units = "cm"))
	float InteractionRadius = 350.0f;

	/** Server-owned access state replicated so UI can hide locked or disabled stations. */
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "Base Storage")
	bool bAccessible = true;

private:
	void ApplyCapacityBonuses(int32 Sign);
	void ApplyCapacityList(const TArray<FRpgBaseResourceCapacity>& Bonuses, int32 Sign);
	void RegisterWithLinkedBaseCamp();
	void UnregisterFromLinkedBaseCamp();

	bool bCapacityBonusesApplied = false;
	TWeakObjectPtr<ARpgBaseCampActor> RegisteredBaseCamp;
};
