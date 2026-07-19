#pragma once

#include "RpgGameFeatureAction_WorldActionBase.h"

#include "GameplayTagContainer.h"
#include "UIExtensionSystem.h"
#include "UObject/ObjectKey.h"
#include "UObject/SoftObjectPtr.h"

#include "RpgGameFeatureAction_AddWidgets.generated.h"

class UCommonActivatableWidget;
class ULocalPlayer;
class UPrimaryGameLayout;
class UUserWidget;
struct FAssetBundleData;
struct FComponentRequestHandle;

/**
 * CommonGame layout widget to push while an RPG experience or GameFeature is active.
 */
USTRUCT(BlueprintType)
struct FRpgGameFeatureWidgetLayoutEntry
{
	GENERATED_BODY()

	/** CommonActivatableWidget layout to push. Designer assets should use the CUI_ prefix. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI", meta = (AssetBundles = "Client"))
	TSoftClassPtr<UCommonActivatableWidget> LayoutClass;

	/** CommonGame UI layer that receives the layout, usually UI.Layer.Game for the persistent HUD. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI", meta = (Categories = "UI.Layer"))
	FGameplayTag LayerTag;
};

/**
 * Widget contribution registered into a UIExtensionPointWidget inside a HUD layout.
 */
USTRUCT(BlueprintType)
struct FRpgGameFeatureWidgetEntry
{
	GENERATED_BODY()

	/** Widget class spawned by the matching UIExtensionPointWidget. Designer assets should use the CUI_ prefix. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI", meta = (AssetBundles = "Client"))
	TSoftClassPtr<UUserWidget> WidgetClass;

	/** Slot tag matched by a UIExtensionPointWidget in the HUD layout, for example UI.HUD.Slot.Health. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI", meta = (Categories = "UI.HUD.Slot"))
	FGameplayTag SlotTag;

	/** Higher-priority widgets are preferred by extension points that sort entries. Use -1 for default ordering. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	int32 Priority = -1;
};

/**
 * Adds CommonUI layouts to the PrimaryGameLayout for the lifetime of an active experience.
 *
 * This mirrors Lyra's Add Widgets action: layouts are pushed to CommonGame layers and widget contributions
 * are registered into UIExtension slots on those layouts.
 */
UCLASS(meta = (DisplayName = "Add Rpg Widgets"))
class SURVIVALRPG_API URpgGameFeatureAction_AddWidgets final : public URpgGameFeatureAction_WorldActionBase
{
	GENERATED_BODY()

public:
	//~ UGameFeatureAction interface
	virtual void OnGameFeatureActivating(FGameFeatureActivatingContext& Context) override;
	virtual void OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context) override;
#if WITH_EDITORONLY_DATA
	virtual void AddAdditionalAssetBundleData(FAssetBundleData& AssetBundleData) override;
#endif
	//~ End UGameFeatureAction interface

	//~ UObject interface
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
	//~ End UObject interface

	/** Layouts pushed for each local player's HUD actor while this experience or feature is active. */
	UPROPERTY(EditAnywhere, Category = "UI", meta = (TitleProperty = "{LayerTag} -> {LayoutClass}"))
	TArray<FRpgGameFeatureWidgetLayoutEntry> Layouts;

	/** Widgets registered into UIExtensionPointWidget slots while this experience or feature is active. */
	UPROPERTY(EditAnywhere, Category = "UI", meta = (TitleProperty = "{SlotTag} -> {WidgetClass}"))
	TArray<FRpgGameFeatureWidgetEntry> Widgets;

private:
	struct FPerActorData
	{
		TArray<TWeakObjectPtr<UCommonActivatableWidget>> LayoutsAdded;
		TArray<FUIExtensionHandle> ExtensionHandles;
	};

	struct FPerContextData
	{
		TArray<TSharedPtr<FComponentRequestHandle>> ComponentRequests;
		TMap<FObjectKey, FPerActorData> ActorData;
	};

	TMap<FGameFeatureStateChangeContext, FPerContextData> ContextData;

	//~ URpgGameFeatureAction_WorldActionBase interface
	virtual void AddToWorld(const FWorldContext& WorldContext, const FGameFeatureStateChangeContext& ChangeContext) override;
	//~ End URpgGameFeatureAction_WorldActionBase interface

	UPrimaryGameLayout* GetPrimaryGameLayout(ULocalPlayer* LocalPlayer) const;
	void Reset(FPerContextData& ActiveData);
	void HandleActorExtension(AActor* Actor, FName EventName, FGameFeatureStateChangeContext ChangeContext);
	void AddWidgets(AActor* Actor, FPerContextData& ActiveData);
	void RemoveWidgets(AActor* Actor, FPerContextData& ActiveData);
};
