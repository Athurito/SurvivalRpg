#pragma once

#include "GameFramework/Actor.h"
#include "RpgBaseBuildableDefinition.h"

#include "RpgBaseConstructionSiteActor.generated.h"

class ARpgBaseCampActor;
class URpgBaseStorageComponent;
class URpgInventoryItemDefinition;
class URpgInventoryManagerComponent;
class USceneComponent;

/** Resource source order used when contributing to a construction site. */
UENUM(BlueprintType)
enum class ERpgBaseConstructionResourceConsumeOrder : uint8
{
	/** Consume from the player inventory first, then linked base storage. */
	PlayerThenBase,

	/** Consume from linked base storage first, then player inventory. */
	BaseThenPlayer,

	/** Only consume carried resources. */
	PlayerOnly,

	/** Only consume resources already stored in the linked base. */
	BaseOnly
};

/** Replicated progress row for one construction material. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgBaseConstructionResourceState
{
	GENERATED_BODY()

	/** Material definition required by the construction site. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Construction")
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition;

	/** Total units required to complete this material row. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Construction")
	int32 RequiredCount = 0;

	/** Units already paid into the construction site. */
	UPROPERTY(BlueprintReadOnly, Category = "Base Construction")
	int32 ContributedCount = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRpgBaseConstructionSiteChanged, ARpgBaseConstructionSiteActor*, ConstructionSite);

/**
 * Replicated runtime construction site for one buildable base actor.
 *
 * The site owns construction progress only. When all costs are paid on the server, it spawns the
 * final actor and links supported storage/crafting components back to the owning base camp.
 */
UCLASS(Blueprintable)
class SURVIVALRPG_API ARpgBaseConstructionSiteActor : public AActor
{
	GENERATED_BODY()

public:
	explicit ARpgBaseConstructionSiteActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Initializes this site after server spawn. Call before players contribute resources. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Base Construction")
	void InitializeConstructionSite(ARpgBaseCampActor* InBaseCamp, URpgBaseBuildableDefinition* InBuildableDefinition);

	/** Base camp that owns this construction site and receives the final buildable link. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Base Construction")
	ARpgBaseCampActor* GetBaseCamp() const { return BaseCamp; }

	/** Static buildable definition used for costs and final actor class. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Base Construction")
	URpgBaseBuildableDefinition* GetBuildableDefinition() const { return BuildableDefinition; }

	/** Returns replicated material progress rows. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Base Construction")
	TArray<FRpgBaseConstructionResourceState> GetConstructionCosts() const { return ConstructionCosts; }

	/** Returns how many units are still missing for one material definition. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Base Construction")
	int32 GetRemainingCostForDefinition(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition) const;

	/** Returns the sum of all missing material units. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Base Construction")
	int32 GetTotalRemainingCost() const;

	/** Returns true once all construction costs are fully paid. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Base Construction")
	bool IsConstructionComplete() const;

	/** Returns true when the actor is a player-controlled pawn/controller close enough to contribute. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Base Construction")
	bool CanActorContribute(const AActor* RequestingActor) const;

	/** Contributes one material from the player inventory and optionally linked base storage. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Base Construction")
	bool ContributeMaterial(AActor* RequestingActor, TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 Count, bool bAllowBaseStorage);

	/** Attempts to fill all remaining costs from the configured resource sources. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Base Construction")
	bool ContributeAllResources(AActor* RequestingActor, bool bAllowBaseStorage);

	/** Spawns the final buildable if construction is complete. Returns the spawned actor or null. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Base Construction")
	AActor* FinishConstruction();

	/** Broadcast when replicated progress or completion changes. */
	UPROPERTY(BlueprintAssignable, Category = "Base Construction")
	FRpgBaseConstructionSiteChanged OnConstructionSiteChanged;

protected:
	/** Simple root so Blueprint children can attach construction meshes and previews. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Base Construction")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Default resource consume order used by contribute calls. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base Construction")
	ERpgBaseConstructionResourceConsumeOrder ContributionConsumeOrder = ERpgBaseConstructionResourceConsumeOrder::PlayerThenBase;

	/** Maximum direct contribution distance in centimeters. Zero or below allows any distance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base Construction", meta = (ClampMin = "0", UIMin = "0", Units = "cm"))
	float ContributionRadius = 450.0f;

	/** If true, the site immediately spawns the final actor once the last material is contributed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base Construction")
	bool bAutoFinishWhenComplete = true;

	/** If true, the construction site destroys itself after spawning the final buildable. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base Construction")
	bool bDestroyWhenFinished = true;

	/** Base camp that owns this construction site. Replicated for UI and late joiners. */
	UPROPERTY(ReplicatedUsing = OnRep_ConstructionState, BlueprintReadOnly, Category = "Base Construction")
	TObjectPtr<ARpgBaseCampActor> BaseCamp;

	/** Buildable definition being constructed. Replicated for UI and late joiners. */
	UPROPERTY(ReplicatedUsing = OnRep_ConstructionState, BlueprintReadOnly, Category = "Base Construction")
	TObjectPtr<URpgBaseBuildableDefinition> BuildableDefinition;

	/** Replicated material progress. */
	UPROPERTY(ReplicatedUsing = OnRep_ConstructionState, BlueprintReadOnly, Category = "Base Construction")
	TArray<FRpgBaseConstructionResourceState> ConstructionCosts;

	/** True after the final actor has been spawned. */
	UPROPERTY(ReplicatedUsing = OnRep_ConstructionState, BlueprintReadOnly, Category = "Base Construction")
	bool bFinished = false;

private:
	UFUNCTION()
	void OnRep_ConstructionState();

	URpgInventoryManagerComponent* FindPlayerInventory(const AActor* RequestingActor) const;
	URpgBaseStorageComponent* GetBaseStorage() const;
	bool ConsumeFromPlayer(URpgInventoryManagerComponent* PlayerInventory, TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 Count) const;
	bool ConsumeFromBase(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 Count) const;
	bool ConsumeContribution(AActor* RequestingActor, TSubclassOf<URpgInventoryItemDefinition> ItemDefinition, int32 Count, bool bAllowBaseStorage);
	FRpgBaseConstructionResourceState* FindCostState(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition);
	const FRpgBaseConstructionResourceState* FindCostState(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition) const;
	void HandleProgressChanged();
	void LinkSpawnedActorToBase(AActor* SpawnedActor) const;
};
