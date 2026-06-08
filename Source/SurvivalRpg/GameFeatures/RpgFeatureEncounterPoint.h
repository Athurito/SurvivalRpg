#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"

#include "RpgFeatureEncounterPoint.generated.h"

class USceneComponent;

/**
 * Lightweight placed marker that feature plugins can use as an encounter spawn transform.
 *
 * This is an overworld/GameFeature placement point, not a dungeon gameplay
 * marker. Keep its tags in the Feature.* namespace so content plugins can claim
 * it without the map hard-referencing feature-owned actors.
 */
UCLASS(Blueprintable)
class SURVIVALRPG_API ARpgFeatureEncounterPoint : public AActor
{
	GENERATED_BODY()

public:
	ARpgFeatureEncounterPoint(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintPure, Category = "Feature Spawning")
	const FGameplayTagContainer& GetFeatureTags() const { return FeatureTags; }

	UFUNCTION(BlueprintPure, Category = "Feature Spawning")
	bool HasAllFeatureTags(const FGameplayTagContainer& RequiredTags) const;

	/** Removes all Feature.* tags from this point. Editor helper for test placement. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Feature Spawning")
	void ClearFeatureTags();

	/** Adds a tag only if it exists and belongs to the Feature.* namespace. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Feature Spawning")
	void AddFeatureTagByName(FName TagName);

	/** Transform used by feature spawners when placing optional content at this point. */
	UFUNCTION(BlueprintPure, Category = "Feature Spawning")
	FTransform GetEncounterTransform() const { return GetActorTransform(); }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Feature Spawning")
	TObjectPtr<USceneComponent> SceneRoot;

	/** Feature-only tags consumed by GameFeature spawn rules. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feature Spawning", meta = (Categories = "Feature"))
	FGameplayTagContainer FeatureTags;
};
