#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"

#include "RpgPortalEncounterComponent.generated.h"

class ARpgFeatureEncounterPoint;
class ARpgFeatureRegionActor;
class ARpgPortalActor;
class URpgPortalEncounterDefinition;

/**
 * Data-driven rule used by a portal encounter component to claim region points.
 *
 * Required tags are limited to Feature.* tags so level authors can mark generic
 * overworld placement points without hard-referencing GameFeature portal assets.
 */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgPortalEncounterSpawnRule
{
	GENERATED_BODY()

	/** Region must contain all of these Feature.* tags before this rule can spawn anything. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal|Encounter", meta = (Categories = "Feature"))
	FGameplayTagContainer RequiredRegionTags;

	/** Encounter point must contain all of these Feature.* tags before this rule can claim it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal|Encounter", meta = (Categories = "Feature"))
	FGameplayTagContainer RequiredPointTags;

	/** Portal actor class to spawn at matching points. Kept soft so GameFeature content owns the asset reference. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal|Encounter", meta = (AssetBundles = "Server"))
	TSoftClassPtr<ARpgPortalActor> PortalActorClass;

	/** Encounter definition assigned to the spawned portal before BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal|Encounter", meta = (AssetBundles = "Server"))
	TSoftObjectPtr<URpgPortalEncounterDefinition> EncounterDefinition;

	/** Local transform offset applied on top of the encounter point transform for the overworld portal. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal|Encounter")
	FTransform SpawnTransformOffset = FTransform::Identity;
};

/**
 * Server-only GameFeature component that spawns portal encounters for a feature region.
 *
 * A GameFeature action adds this component to ARpgFeatureRegionActor instances.
 * The component scans the region's explicit ManagedEncounterPoints, claims points
 * that match its Feature.* tag rules, spawns one portal per claimed point, and
 * gives each portal a deterministic technical dungeon-pocket transform.
 */
UCLASS(Blueprintable, ClassGroup = (Portals), meta = (BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgPortalEncounterComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URpgPortalEncounterComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Server-side entry point that applies SpawnRules once for this region owner. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Portal|Encounter")
	void TrySpawnPortalEncounters();

	/** Destroys portals spawned by this component, usually when the GameFeature/component unloads. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Portal|Encounter")
	void DestroySpawnedPortals();

	/** Editor helper for prototype assets that need one simple region-tag to point-tag spawn rule. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Portal|Encounter")
	void ConfigureSingleSpawnRuleByTagNames(
		FName RequiredRegionTagName,
		FName RequiredPointTagName,
		TSubclassOf<ARpgPortalActor> InPortalActorClass,
		URpgPortalEncounterDefinition* InEncounterDefinition);

	UFUNCTION(BlueprintPure, Category = "Portal|Encounter")
	int32 GetSpawnedPortalCount() const { return SpawnedPortals.Num(); }

protected:
	ARpgPortalActor* SpawnPortalForPoint(ARpgFeatureRegionActor& RegionActor, ARpgFeatureEncounterPoint& EncounterPoint, const FRpgPortalEncounterSpawnRule& SpawnRule, int32 PortalIndex);
	FTransform BuildDungeonLevelInstanceTransform(const ARpgFeatureRegionActor& RegionActor, const ARpgFeatureEncounterPoint& EncounterPoint, int32 PortalIndex) const;

	/** Ordered rules evaluated against the owning region's ManagedEncounterPoints. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal|Encounter")
	TArray<FRpgPortalEncounterSpawnRule> SpawnRules;

	/** Hidden world-space origin for technical dungeon level-instance pockets. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal|Dungeon", meta = (ForceUnits = "cm"))
	FVector DungeonPocketBaseLocation = FVector(1000000.0, 0.0, 0.0);

	/** World grid size used to bucket overworld regions into deterministic dungeon-pocket ranges. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal|Dungeon", meta = (ClampMin = "1000.0", ForceUnits = "cm"))
	double DungeonPocketRegionGridSize = 100000.0;

	/** Distance between dungeon-pocket ranges for neighboring region buckets. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal|Dungeon", meta = (ClampMin = "1000.0", ForceUnits = "cm"))
	double DungeonPocketRegionStride = 1000000.0;

	/** Distance between individual portal dungeon instances inside one region bucket. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal|Dungeon", meta = (ClampMin = "1000.0", ForceUnits = "cm"))
	double DungeonPocketPortalSpacing = 100000.0;

	/** Portals spawned and owned by this component so they can be cleaned up with the region. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<ARpgPortalActor>> SpawnedPortals;

	/** Prevents duplicate spawns from OnRegister and BeginPlay both firing during GameFeature activation. */
	bool bSpawnAttempted = false;
};
