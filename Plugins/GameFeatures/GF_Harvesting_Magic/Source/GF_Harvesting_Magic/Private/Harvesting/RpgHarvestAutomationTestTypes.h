#pragma once

#include "AbilitySystem/Abilities/RpgGameplayAbility_Harvest.h"
#include "Harvesting/RpgHarvestableInstancesComponent.h"
#include "Harvesting/RpgHarvestProtectionComponent.h"
#include "Harvesting/RpgHarvestTargetingComponent.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/Inventory/RpgDroppedInventoryActor.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"

#include "RpgHarvestAutomationTestTypes.generated.h"

class URpgCorpseLifecycleComponent;
class URpgHarvestableComponent;
class URpgHarvestableCorpseComponent;
class UBoxComponent;
class USceneComponent;

/** Drop fixture that deliberately materializes only part of a payload before reporting failure. */
UCLASS(NotBlueprintable, Transient)
class ARpgHarvestAutomationPartialFailureDropActor final : public ARpgDroppedInventoryActor
{
	GENERATED_BODY()

public:
	virtual bool TrySetPickupInventory(
		const FInventoryPickup& NewPickupInventory) override;
};

/** Player-state fixture that skips Experience wiring while retaining real inventory and trade-skill components. */
UCLASS(NotBlueprintable, Transient)
class ARpgHarvestAutomationTestPlayerState final : public ARpgPlayerState
{
	GENERATED_BODY()

public:
	virtual void PostInitializeComponents() override;
};

/** Stackable 1x1 material used by asset-free harvest reward tests. */
UCLASS(NotBlueprintable, Transient)
class URpgHarvestAutomationTestStackItemDefinition final : public URpgInventoryItemDefinition
{
	GENERATED_BODY()

public:
	explicit URpgHarvestAutomationTestStackItemDefinition(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool IsEditorOnly() const override { return true; }
};
/** Distinct stackable material used to prove multi-row overflow batches remain complete. */
UCLASS(NotBlueprintable, Transient)
class URpgHarvestAutomationTestSecondMaterialDefinition final : public URpgInventoryItemDefinition
{
	GENERATED_BODY()

public:
	explicit URpgHarvestAutomationTestSecondMaterialDefinition(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool IsEditorOnly() const override { return true; }
};

/** Low-power skinning tool used to verify deterministic best-tool selection. */
UCLASS(NotBlueprintable, Transient)
class URpgHarvestAutomationTestLowToolDefinition final : public URpgInventoryItemDefinition
{
	GENERATED_BODY()

public:
	explicit URpgHarvestAutomationTestLowToolDefinition(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual bool IsEditorOnly() const override { return true; }
};

/** High-power skinning tool used to verify power wins before item identity. */
UCLASS(NotBlueprintable, Transient)
class URpgHarvestAutomationTestHighToolDefinition final : public URpgInventoryItemDefinition
{
	GENERATED_BODY()

public:
	explicit URpgHarvestAutomationTestHighToolDefinition(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual bool IsEditorOnly() const override { return true; }
};

/** Equal-power skinning tool used to verify stable item-id tie-breaking. */
UCLASS(NotBlueprintable, Transient)
class URpgHarvestAutomationTestTieToolDefinition final : public URpgInventoryItemDefinition
{
	GENERATED_BODY()

public:
	explicit URpgHarvestAutomationTestTieToolDefinition(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual bool IsEditorOnly() const override { return true; }
};

/** Asset-free authoritative corpse fixture with the real lifecycle and harvest components. */
UCLASS(NotBlueprintable, Transient)
class ARpgHarvestAutomationCorpseActor final : public AActor
{
	GENERATED_BODY()

public:
	explicit ARpgHarvestAutomationCorpseActor(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<URpgCorpseLifecycleComponent> CorpseLifecycle;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<URpgHarvestableCorpseComponent> HarvestableCorpse;
};

/** Asset-free replicated resource fixture with the real actor-backed harvestable node component. */
UCLASS(NotBlueprintable, Transient)
class ARpgHarvestAutomationNodeActor final : public AActor
{
	GENERATED_BODY()

public:
	explicit ARpgHarvestAutomationNodeActor(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<URpgHarvestableComponent> HarvestableNode;
};

/** Records harvest-node presentation events so tests can assert initial and live notifications. */
UCLASS(Transient)
class URpgHarvestAutomationNodeStateListener final : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION()
	void HandleStateChanged(
		URpgHarvestableComponent* Component,
		int32 RemainingSections,
		int32 SectionCount,
		bool bActive,
		bool bInitialState);

	int32 EventCount = 0;
	int32 LastRemainingSections = INDEX_NONE;
	int32 LastSectionCount = INDEX_NONE;
	bool bLastActive = false;
	bool bLastInitialState = false;
};

/** Resource fixture with a blocking collision box so harvest targeting queries can select it. */
UCLASS(NotBlueprintable, Transient)
class ARpgHarvestAutomationCollidableNodeActor final : public AActor
{
	GENERATED_BODY()

public:
	explicit ARpgHarvestAutomationCollidableNodeActor(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UBoxComponent> Collision;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<URpgHarvestableComponent> HarvestableNode;
};

/** Harvestable instances that record their presentation events and accept a profile from tests. */
UCLASS(NotBlueprintable, Transient)
class URpgHarvestAutomationInstancesComponent final : public URpgHarvestableInstancesComponent
{
	GENERATED_BODY()

public:
	void ConfigureProfile(URpgHarvestProfile* InProfile) { HarvestProfile = InProfile; }
	void ConfigureLinkedPresentation(const FName InTag) { LinkedPresentationTag = InTag; }

	int32 EventCount = 0;
	int32 LastInstanceIndex = INDEX_NONE;
	int32 LastRemainingSections = INDEX_NONE;
	bool bLastActive = false;
	bool bLastInitialState = false;

protected:
	virtual void OnInstanceStockChanged_Implementation(
		int32 InstanceIndex,
		int32 RemainingSections,
		int32 SectionCount,
		bool bActive,
		bool bInitialState) override;
};

/** Non-replicated owner of harvestable instances, like a PCG partition actor loaded on every machine. */
UCLASS(NotBlueprintable, Transient)
class ARpgHarvestAutomationInstancesActor final : public AActor
{
	GENERATED_BODY()

public:
	explicit ARpgHarvestAutomationInstancesActor(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<URpgHarvestAutomationInstancesComponent> Instances;
};

/** Actor whose root is a harvest protection box, like a camp or a protected zone placed in a map. */
UCLASS(NotBlueprintable, Transient)
class ARpgHarvestAutomationProtectionActor final : public AActor
{
	GENERATED_BODY()

public:
	explicit ARpgHarvestAutomationProtectionActor(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<URpgHarvestProtectionComponent> Protection;
};

/** Concrete test-only harvest ability whose tuning tests set on the granted instance. */
UCLASS(NotBlueprintable)
class URpgHarvestAutomationTestAbility final : public URpgGameplayAbility_Harvest
{
	GENERATED_BODY()

public:
	explicit URpgHarvestAutomationTestAbility(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	void ConfigureTargeting(const FRpgHarvestTargetingParams& InTargeting) { Targeting = InTargeting; }
	void ConfigureSections(const int32 InSectionsPerTarget) { SectionsPerTarget = InSectionsPerTarget; }
	void ConfigureCommitDelay(const float InCommitDelaySeconds) { CommitDelaySeconds = InCommitDelaySeconds; }
	void ConfigureAimWhileInputHeld(const bool bInAim) { bAimWhileInputHeld = bInAim; }
	void ConfigureSkillUnlock(const FGameplayTag InSkillTag, const int32 InMinimumLevel)
	{
		RequiredSkillTag = InSkillTag;
		MinimumSkillLevel = InMinimumLevel;
	}
	void ConfigureWeakPointHits(const bool bInCanHitWeakPoints) { bCanHitWeakPoints = bInCanHitWeakPoints; }
	void ConfigurePresentationWave(const float InSpeed) { PresentationWaveSpeed = InSpeed; }
};

/** Records preview notifications so tests can assert change-only broadcasting. */
UCLASS(Transient)
class URpgHarvestAutomationPreviewListener final : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION()
	void HandlePreviewChanged(const FRpgHarvestPreview& Preview);

	int32 EventCount = 0;
	FRpgHarvestPreview LastPreview;
};

/** Targeting component fixture that can enable the projected indicator without a designer asset. */
UCLASS(NotBlueprintable, Transient)
class URpgHarvestAutomationTargetingComponent final : public URpgHarvestTargetingComponent
{
	GENERATED_BODY()

public:
	void ConfigureIndicator(const TSoftClassPtr<UUserWidget>& InWidgetClass) { IndicatorWidgetClass = InWidgetClass; }
	void ConfigureAreaMarker(const TSoftClassPtr<AActor>& InMarkerClass) { AreaMarkerClass = InMarkerClass; }
	AActor* GetAreaMarkerForTest() const { return AreaMarker; }
};
