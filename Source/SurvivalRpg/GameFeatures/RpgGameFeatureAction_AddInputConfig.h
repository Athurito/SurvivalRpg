#pragma once

#include "CoreMinimal.h"
#include "RpgGameFeatureAction_WorldActionBase.h"
#include "RpgGameFeatureAction_AddInputConfig.generated.h"

class APlayerController;
class URpgInputConfig;
class URpgPawnGameplayComponent;
struct FComponentRequestHandle;

/**
 * GameFeatureAction that adds ability input bindings from URpgInputConfig assets.
 *
 * Mapping contexts and input bindings are intentionally separate, matching Lyra's pattern:
 * one action exposes hardware mappings, this one binds ability input tags while the feature is active.
 */
UCLASS(meta = (DisplayName = "Add Rpg Input Config"))
class SURVIVALRPG_API URpgGameFeatureAction_AddInputConfig final : public URpgGameFeatureAction_WorldActionBase
{
	GENERATED_BODY()

public:
	virtual void OnGameFeatureActivating(FGameFeatureActivatingContext& Context) override;
	virtual void OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context) override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	UPROPERTY(EditAnywhere, Category = "Input", meta = (AssetBundles = "Client,Server"))
	TArray<TSoftObjectPtr<const URpgInputConfig>> InputConfigs;

private:
	struct FComponentInputConfigEntry
	{
		TWeakObjectPtr<URpgPawnGameplayComponent> PawnGameplayComponent;
		TArray<const URpgInputConfig*> AddedInputConfigs;
	};

	struct FPerContextData
	{
		TArray<TSharedPtr<FComponentRequestHandle>> ExtensionRequestHandles;
		TArray<FComponentInputConfigEntry> AddedInputConfigEntries;
	};

	TMap<FGameFeatureStateChangeContext, FPerContextData> ContextData;

	virtual void AddToWorld(const FWorldContext& WorldContext, const FGameFeatureStateChangeContext& ChangeContext) override;

	void Reset(FPerContextData& ActiveData);
	void HandleControllerExtension(AActor* Actor, FName EventName, FGameFeatureStateChangeContext ChangeContext);
	void AddInputConfigForController(APlayerController* PlayerController, FPerContextData& ActiveData);
	void RemoveInputConfigForController(APlayerController* PlayerController, FPerContextData& ActiveData);
	void RemoveEntry(int32 EntryIndex, FPerContextData& ActiveData);
	int32 FindEntryIndexForComponent(const URpgPawnGameplayComponent* PawnGameplayComponent, const FPerContextData& ActiveData) const;
};
