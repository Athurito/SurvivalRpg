#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"

#include "RpgFeatureRegionActor.generated.h"

class ARpgFeatureEncounterPoint;
class USceneComponent;

/**
 * Chunk-local feature receiver that owns placed encounter points for modular GameFeature content.
 *
 * Region actors are safe to place in normal maps or World Partition chunks. They
 * expose only Feature.* tags and explicit ManagedEncounterPoints; feature plugins
 * add components to this actor type when they want to spawn optional content for
 * a loaded region.
 */
UCLASS(Blueprintable)
class SURVIVALRPG_API ARpgFeatureRegionActor : public AActor
{
	GENERATED_BODY()

public:
	ARpgFeatureRegionActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void PreInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintPure, Category = "Feature Spawning")
	const FGameplayTagContainer& GetFeatureTags() const { return FeatureTags; }

	UFUNCTION(BlueprintPure, Category = "Feature Spawning")
	bool HasAllFeatureTags(const FGameplayTagContainer& RequiredTags) const;

	/** Removes all Feature.* tags from this region. Editor helper for test placement. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Feature Spawning")
	void ClearFeatureTags();

	/** Adds a tag only if it exists and belongs to the Feature.* namespace. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Feature Spawning")
	void AddFeatureTagByName(FName TagName);

	/** Explicit points owned by this region/chunk. Feature components should use this instead of global actor searches. */
	const TArray<TObjectPtr<ARpgFeatureEncounterPoint>>& GetManagedEncounterPoints() const { return ManagedEncounterPoints; }

	/** Returns the region's managed points that contain all RequiredTags. */
	UFUNCTION(BlueprintCallable, Category = "Feature Spawning")
	void GetManagedEncounterPointsMatchingTags(const FGameplayTagContainer& RequiredTags, TArray<ARpgFeatureEncounterPoint*>& OutEncounterPoints) const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Feature Spawning")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Feature-only tags that let GameFeature components decide whether this region is relevant. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feature Spawning", meta = (Categories = "Feature"))
	FGameplayTagContainer FeatureTags;

	/** Explicit encounter/spawn points kept with the same streaming context as this region. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feature Spawning")
	TArray<TObjectPtr<ARpgFeatureEncounterPoint>> ManagedEncounterPoints;
};
