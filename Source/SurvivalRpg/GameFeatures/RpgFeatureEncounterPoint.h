#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"

#include "RpgFeatureEncounterPoint.generated.h"

class USceneComponent;

/**
 * Lightweight placed marker that feature plugins can use as an encounter spawn transform.
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

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Feature Spawning")
	void ClearFeatureTags();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Feature Spawning")
	void AddFeatureTagByName(FName TagName);

	UFUNCTION(BlueprintPure, Category = "Feature Spawning")
	FTransform GetEncounterTransform() const { return GetActorTransform(); }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Feature Spawning")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feature Spawning", meta = (Categories = "Feature"))
	FGameplayTagContainer FeatureTags;
};
