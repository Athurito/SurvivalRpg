#pragma once

#include "Components/ActorComponent.h"
#include "GameplayAbilitySpecHandle.h"
#include "Harvesting/RpgHarvestTargeting.h"
#include "SurvivalRpg/UI/IndicatorSystem/IndicatorDescriptor.h"
#include "TimerManager.h"

#include "RpgHarvestTargetingComponent.generated.h"

class AController;
class UAbilitySystemComponent;
class UIndicatorDescriptor;
class USceneComponent;
class UUserWidget;
struct FGameplayAbilitySpec;

/** Cosmetic notification fired on the owning client when the previewed harvest targets change. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRpgHarvestPreviewChangedEvent, const FRpgHarvestPreview&, Preview);

/** Presentation state of the primary previewed target, derived from its evaluation. UI-read-only. */
UENUM(BlueprintType)
enum class ERpgHarvestTargetStatus : uint8
{
	/** No harvest ability or no target is previewed. */
	None,

	/** A commit would extract stock from the target. */
	Harvestable,

	/** The target lies beyond the ability's reach. */
	OutOfReach,

	/** The target is inactive or has no stock left. */
	Depleted,

	/** The target requires a tool the ability does not provide. */
	WrongTool,

	/** The harvester's trade skill is too low for the target. */
	SkillLocked,

	/** The target rejected the request for another reason. */
	Unavailable
};

/** One projected target indicator and the target it marks. Local presentation state. */
USTRUCT()
struct FRpgHarvestTargetIndicator
{
	GENERATED_BODY()

	/** Indicator registered with the controller's indicator manager. */
	UPROPERTY()
	TObjectPtr<UIndicatorDescriptor> Indicator = nullptr;

	/** Marked instance of an instanced target component, or INDEX_NONE for other target components. */
	int32 InstanceIndex = INDEX_NONE;
};

/**
 * Local-only read model of what the player's harvest abilities would hit, for target indicators.
 *
 * Added to player controllers by the harvesting GameFeature. On the locally controlled player it re-evaluates at
 * UpdateRateHz with the same query the server uses at commit time:
 * - While a hold-to-aim harvest ability is held, that ability's area and targets are previewed.
 * - Otherwise the harvest ability bound to the active main-hand tool's primary input is previewed.
 * OnPreviewChanged fires only when the presentation-relevant content changes. The preview never grants or
 * mutates anything; presentation Blueprints and widgets read it.
 *
 * When IndicatorWidgetClass is set, the component anchors projected indicators through the controller's indicator
 * manager: over the primary target, and over every target while a hold-to-aim ability is held. Instanced targets get
 * one indicator per instance, placed over that instance. The Widget Blueprint owns all presentation and reads its
 * target through URpgHarvestTargetIndicatorWidget. While an area ability is held, an optional
 * AreaMarkerClass actor marks the area on the ground.
 */
UCLASS(Blueprintable, BlueprintType, ClassGroup = (Rpg), meta = (BlueprintSpawnableComponent, DisplayName = "RPG Harvest Targeting"))
class GF_HARVESTING_MAGIC_API URpgHarvestTargetingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	explicit URpgHarvestTargetingComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Returns the targeting component of Controller, or null when the harvesting feature did not add one. */
	static URpgHarvestTargetingComponent* FindForController(const AController* Controller);

	/** Returns the latest local preview; empty when no harvest ability is available. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Harvesting|Targeting")
	FRpgHarvestPreview GetCurrentPreview() const { return CurrentPreview; }

	/** True while the current preview belongs to a held aim ability. */
	bool IsPreviewAiming() const { return CurrentPreview.bIsAiming; }

	/** Returns the previewed evaluation whose hit component is TargetComponent, or null when it is not previewed. */
	const FRpgHarvestTargetEvaluation* FindTargetEvaluation(const USceneComponent* TargetComponent) const;

	/**
	 * Returns the previewed evaluation that Indicator marks, matching its instance for instanced targets. Null when
	 * Indicator was not created by this component or its target left the preview.
	 */
	const FRpgHarvestTargetEvaluation* FindIndicatedTarget(const UIndicatorDescriptor* Indicator) const;

	/** Summarizes the target Indicator marks, as GetPrimaryTargetStatus does; false when it is not previewed. */
	bool GetIndicatedTargetStatus(
		const UIndicatorDescriptor* Indicator,
		ERpgHarvestTargetStatus& OutStatus,
		int32& OutRemainingSections,
		int32& OutSectionCount,
		int32& OutSectionsToTake) const;

	/**
	 * Summarizes the primary previewed target for indicators. Returns false when nothing is previewed.
	 * OutRemainingSections is the target's current stock; OutSectionsToTake is what the ability would extract.
	 */
	UFUNCTION(BlueprintPure, Category = "Rpg|Harvesting|Targeting")
	bool GetPrimaryTargetStatus(
		ERpgHarvestTargetStatus& OutStatus,
		int32& OutRemainingSections,
		int32& OutSectionCount,
		int32& OutSectionsToTake) const;

	/**
	 * Summarizes the previewed target whose hit component is TargetComponent, as GetPrimaryTargetStatus does.
	 * Returns false when that component is not part of the current preview.
	 */
	UFUNCTION(BlueprintPure, Category = "Rpg|Harvesting|Targeting")
	bool GetTargetStatus(
		const USceneComponent* TargetComponent,
		ERpgHarvestTargetStatus& OutStatus,
		int32& OutRemainingSections,
		int32& OutSectionCount,
		int32& OutSectionsToTake) const;

	/** Re-evaluates immediately and notifies listeners when the preview changed. Does nothing on non-local controllers. */
	UFUNCTION(BlueprintCallable, Category = "Rpg|Harvesting|Targeting")
	void RefreshPreview();

	/** Called by hold-to-aim harvest abilities on the owning client while their input is held. */
	void SetAimingAbility(FGameplayAbilitySpecHandle Handle, bool bAiming);

	/** Cosmetic event for target highlights and indicators. Never grant loot from it. */
	UPROPERTY(BlueprintAssignable, Category = "Rpg|Harvesting|Targeting")
	FRpgHarvestPreviewChangedEvent OnPreviewChanged;

protected:
	//~ UActorComponent interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent interface

	/** Preview evaluations per second on the local client; each runs one view query. Cosmetic tuning. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Targeting", meta = (ClampMin = "1.0", ClampMax = "60.0", UIMin = "5.0", UIMax = "30.0"))
	float UpdateRateHz = 15.0f;

	/** Designer-owned widget shown over the primary target; empty disables the projected indicator. Cosmetic. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Indicator")
	TSoftClassPtr<UUserWidget> IndicatorWidgetClass;

	/**
	 * How the indicator is projected onto the target's hit component. Instanced targets always use a point at
	 * BoundingBoxAnchor of the targeted instance's bounds, because the component's bounds span every instance. Cosmetic.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Indicator")
	EActorCanvasProjectionMode ProjectionMode = EActorCanvasProjectionMode::ComponentBoundingBox;

	/** Normalized anchor inside the projected bounding box or instance bounds; (0.5, 0.5, 1) is the top center. Cosmetic. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Indicator")
	FVector BoundingBoxAnchor = FVector(0.5, 0.5, 1.0);

	/** Screen-space offset of the indicator in pixels after projection. Cosmetic. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Indicator")
	FVector2D ScreenSpaceOffset = FVector2D(0.0, -24.0);

	/** Indicator layer priority; higher values draw above lower ones. Cosmetic. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Indicator")
	int32 IndicatorPriority = 10;

	/**
	 * Designer-owned actor shown at the aim point while a hold-to-aim area ability is held; empty disables it.
	 * Author it for a 100 cm radius: the component scales it uniformly to the previewed area radius.
	 * Spawned locally, never replicated, cosmetic.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Indicator")
	TSoftClassPtr<AActor> AreaMarkerClass;

	/** Local area marker instance; hidden while no area is previewed. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Rpg|Harvesting|Indicator")
	TObjectPtr<AActor> AreaMarker;

private:
	bool IsLocallyControlled() const;
	UAbilitySystemComponent* FindAbilitySystem() const;
	const FGameplayAbilitySpec* FindPreviewSpec(UAbilitySystemComponent& AbilitySystem, bool& bOutAiming) const;
	void UpdateTargetIndicators();
	void PlaceTargetIndicator(UIndicatorDescriptor& Indicator, const FRpgHarvestTargetEvaluation& Target) const;
	void RemoveTargetIndicators();
	void UpdateAreaMarker();
	void DestroyAreaMarker();

	/** Latest local preview, compared on refresh to suppress redundant notifications. */
	FRpgHarvestPreview CurrentPreview;

	/** Hold-to-aim ability currently held on this client, if any. */
	FGameplayAbilitySpecHandle AimingSpecHandle;

	/** Projected indicators, one per indicated target; kept while their target stays indicated. */
	UPROPERTY(Transient)
	TArray<FRpgHarvestTargetIndicator> TargetIndicators;

	FTimerHandle RefreshTimerHandle;
};
