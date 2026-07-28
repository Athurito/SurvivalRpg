#pragma once

#include "RpgGameFeatureAction_WorldActionBase.h"

#include "RpgGameFeatureAction_AddComponents.generated.h"

class UActorComponent;
struct FComponentRequestHandle;

USTRUCT(BlueprintType)
struct FRpgGameFeatureComponentEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Components")
	TSoftClassPtr<AActor> ActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Components", meta = (AssetBundles = "Client,Server"))
	TSoftClassPtr<UActorComponent> ComponentClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Components")
	bool bClientComponent = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Components")
	bool bServerComponent = true;
};

/**
 * Adds presentation or gameplay components to modular actors for the lifetime of a feature action.
 */
UCLASS(meta = (DisplayName = "Add Rpg Components"))
class SURVIVALRPG_API URpgGameFeatureAction_AddComponents final : public URpgGameFeatureAction_WorldActionBase
{
	GENERATED_BODY()

public:
	virtual void OnGameFeatureActivating(FGameFeatureActivatingContext& Context) override;
	virtual void OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context) override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	/** Components granted to matching modular actors while this Game Feature is active. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Components", meta = (TitleProperty = "ActorClass", ShowOnlyInnerProperties))
	TArray<FRpgGameFeatureComponentEntry> ComponentList;

private:
	virtual void AddToWorld(const FWorldContext& WorldContext, const FGameFeatureStateChangeContext& ChangeContext) override;

	TMap<FGameFeatureStateChangeContext, TArray<TSharedPtr<FComponentRequestHandle>>> ComponentRequests;
};
