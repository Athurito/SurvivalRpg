#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"

#include "RpgFeatureRegionActor.generated.h"

class ARpgFeatureEncounterPoint;
class USceneComponent;

/**
 * Chunk-local feature receiver that owns placed encounter points for modular GameFeature content.
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

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Feature Spawning")
	void ClearFeatureTags();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Feature Spawning")
	void AddFeatureTagByName(FName TagName);

	const TArray<TObjectPtr<ARpgFeatureEncounterPoint>>& GetManagedEncounterPoints() const { return ManagedEncounterPoints; }

	UFUNCTION(BlueprintCallable, Category = "Feature Spawning")
	void GetManagedEncounterPointsMatchingTags(const FGameplayTagContainer& RequiredTags, TArray<ARpgFeatureEncounterPoint*>& OutEncounterPoints) const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Feature Spawning")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feature Spawning", meta = (Categories = "Feature"))
	FGameplayTagContainer FeatureTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feature Spawning")
	TArray<TObjectPtr<ARpgFeatureEncounterPoint>> ManagedEncounterPoints;
};
