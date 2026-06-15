#pragma once

#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"

#include "RpgBaseCampActor.generated.h"

class URpgBaseBuildableDefinition;
class URpgBaseStorageComponent;
class URpgBaseStorageStationComponent;
class URpgInventoryManagerComponent;
class USceneComponent;

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

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

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

private:
	TArray<TWeakObjectPtr<URpgBaseStorageStationComponent>> RegisteredStorageStations;
};
