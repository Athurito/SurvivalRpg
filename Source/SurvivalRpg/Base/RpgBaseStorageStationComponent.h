#pragma once

#include "Components/ActorComponent.h"
#include "RpgBaseStorageComponent.h"
#include "SurvivalRpg/Interaction/IInteractableTarget.h"

#include "RpgBaseStorageStationComponent.generated.h"

class ARpgBaseCampActor;
class URpgInventoryManagerComponent;

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

	/** Returns the linked material-count storage pool, or null when no base camp is linked. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Base Storage")
	URpgBaseStorageComponent* GetBaseStorage() const;

	/** Returns the linked armory inventory for instance items, or null when no base camp is linked. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Base Storage")
	URpgInventoryManagerComponent* GetArmoryInventory() const;

	/** Returns true when the requesting actor is allowed to interact with this station right now. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Base Storage")
	bool CanActorAccess(const AActor* RequestingActor) const;

	/** Runtime access toggle for locked, damaged, or scripted stations. Server-authoritative. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Base Storage")
	void SetStationAccessible(bool bNewAccessible);

protected:
	/** Interaction option shown by the Lyra-style interaction scan when the station is usable. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base Storage")
	FInteractionOption OpenStationOption;

	/** Base camp that receives this station's capacity bonuses and owns the shared pools. */
	UPROPERTY(EditInstanceOnly, Replicated, BlueprintReadOnly, Category = "Base Storage")
	TObjectPtr<ARpgBaseCampActor> LinkedBaseCamp;

	/** Resource capacity added while this station exists and is linked on the authority. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base Storage")
	TArray<FRpgBaseResourceCapacity> CapacityBonuses;

	/** Maximum direct interaction distance in centimeters. Zero or below allows access at any distance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base Storage", meta = (ClampMin = "0", UIMin = "0", Units = "cm"))
	float InteractionRadius = 350.0f;

	/** Server-owned access state replicated so UI can hide locked or disabled stations. */
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "Base Storage")
	bool bAccessible = true;

private:
	void ApplyCapacityBonuses(int32 Sign);

	bool bCapacityBonusesApplied = false;
};
