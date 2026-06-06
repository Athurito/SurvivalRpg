#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"

#include "RpgPortalEncounterComponent.generated.h"

class ARpgFeatureEncounterPoint;
class ARpgFeatureRegionActor;
class ARpgPortalActor;
class URpgPortalEncounterDefinition;

USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgPortalEncounterSpawnRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal|Encounter", meta = (Categories = "Feature"))
	FGameplayTagContainer RequiredRegionTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal|Encounter", meta = (Categories = "Feature"))
	FGameplayTagContainer RequiredPointTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal|Encounter", meta = (AssetBundles = "Server"))
	TSoftClassPtr<ARpgPortalActor> PortalActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal|Encounter", meta = (AssetBundles = "Server"))
	TSoftObjectPtr<URpgPortalEncounterDefinition> EncounterDefinition;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal|Encounter")
	FTransform SpawnTransformOffset = FTransform::Identity;
};

/**
 * Server-only GameFeature component that spawns portal encounters for a feature region.
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

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Portal|Encounter")
	void TrySpawnPortalEncounters();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Portal|Encounter")
	void DestroySpawnedPortals();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Portal|Encounter")
	void ConfigureSingleSpawnRuleByTagNames(
		FName RequiredRegionTagName,
		FName RequiredPointTagName,
		TSubclassOf<ARpgPortalActor> InPortalActorClass,
		URpgPortalEncounterDefinition* InEncounterDefinition);

	UFUNCTION(BlueprintPure, Category = "Portal|Encounter")
	int32 GetSpawnedPortalCount() const { return SpawnedPortals.Num(); }

protected:
	ARpgPortalActor* SpawnPortalForPoint(ARpgFeatureRegionActor& RegionActor, ARpgFeatureEncounterPoint& EncounterPoint, const FRpgPortalEncounterSpawnRule& SpawnRule);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal|Encounter")
	TArray<FRpgPortalEncounterSpawnRule> SpawnRules;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ARpgPortalActor>> SpawnedPortals;

	bool bSpawnAttempted = false;
};
