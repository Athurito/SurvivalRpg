#pragma once

#include "AbilitySystem/Abilities/RpgGameplayAbility_Harvest.h"
#include "GameplayEffect.h"
#include "Harvesting/RpgHarvestableInstancesComponent.h"
#include "Harvesting/RpgHarvestChainComponent.h"
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
class URpgSkillTreeDefinition;

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

/** Skinning tool whose SkillTree fragment points at a transient tree supplied by the running test. */
UCLASS(NotBlueprintable, Transient)
class URpgHarvestAutomationTestTreeToolDefinition final : public URpgInventoryItemDefinition
{
	GENERATED_BODY()

public:
	explicit URpgHarvestAutomationTestTreeToolDefinition(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual bool IsEditorOnly() const override { return true; }

	/** Points the class default fragment at Tree; tests restore null when they finish. */
	static void SetTestSkillTree(const URpgSkillTreeDefinition* Tree);
};

/** Cooldown effect of four seconds whose duration skill tree tests tune. */
UCLASS(NotBlueprintable, Transient)
class URpgHarvestAutomationTestCooldownEffect final : public UGameplayEffect
{
	GENERATED_BODY()

public:
	explicit URpgHarvestAutomationTestCooldownEffect(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Makes the class default grant CooldownTag, as cooldown effects must; tests restore an empty tag when they finish. */
	static void SetCooldownTag(FGameplayTag CooldownTag);

	static constexpr float BaseDurationSeconds = 4.0f;
};

/** Instant focus gain like GE_HarvestFocus_Gain: adds its SetByCaller.Harvest.FocusGain magnitude to Focus. */
UCLASS(NotBlueprintable, Transient)
class URpgHarvestAutomationTestFocusGainEffect final : public UGameplayEffect
{
	GENERATED_BODY()

public:
	explicit URpgHarvestAutomationTestFocusGainEffect(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * Makes the class default refuse to apply while its target owns IgnoredTag, as the asset ignores Harvest.Spell.Active.
	 * An empty tag ignores nothing; tests restore it when they finish.
	 */
	static void SetIgnoredTargetTag(FGameplayTag IgnoredTag);
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

/** Records a harvest node's server-side commit telemetry so tests can inspect committed results. */
UCLASS(Transient)
class URpgHarvestAutomationHarvestedListener final : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION()
	void HandleHarvested(const FRpgHarvestRequest& Request, const FRpgHarvestResult& Result);

	TArray<FRpgHarvestResult> Results;
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

/** Actor whose root is a harvest chain box, like a grove placed in a portal realm. */
UCLASS(NotBlueprintable, Transient)
class ARpgHarvestAutomationChainActor final : public AActor
{
	GENERATED_BODY()

public:
	explicit ARpgHarvestAutomationChainActor(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<URpgHarvestChainComponent> Chain;
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
	void ConfigureSwarm(const TSubclassOf<ARpgHarvestSwarm> InSwarmClass, const FRpgHarvestSwarmParams& InSwarm)
	{
		SwarmClass = InSwarmClass;
		Swarm = InSwarm;
	}
	void ConfigureCooldown(const TSubclassOf<UGameplayEffect> InCooldownEffect) { CooldownGameplayEffectClass = InCooldownEffect; }
	void ConfigureYieldConversions(const TArray<FRpgHarvestYieldConversion>& InConversions) { YieldConversions = InConversions; }
	void ConfigureSuccessCues(const FGameplayTag InSuccessCue, const TArray<FRpgHarvestFormGameplayCue>& InFormCues)
	{
		SuccessGameplayCue = InSuccessCue;
		FormSuccessGameplayCues = InFormCues;
	}
	void ConfigureTargetCue(const FGameplayTag InTargetCue) { TargetGameplayCue = InTargetCue; }
	void ConfigureStride(const FRpgHarvestStrideParams& InStride, const FGameplayTag InCueTag)
	{
		Stride = InStride;
		StrideGameplayCue = InCueTag;
	}
	void ConfigureRuneFlow(
		const TSubclassOf<ARpgHarvestRuneStrike> InEchoClass,
		const TSubclassOf<ARpgHarvestRuneStrike> InTransferClass,
		const float InRadius)
	{
		EchoStrikeClass = InEchoClass;
		TransferStrikeClass = InTransferClass;
		RuneFlowRadius = InRadius;
	}
	void ConfigureBurn(const TSubclassOf<ARpgHarvestBurn> InBurnClass) { BurnClass = InBurnClass; }
	void ConfigureSpread(
		const TSubclassOf<ARpgHarvestRuneStrike> InLeapClass,
		const TSubclassOf<ARpgHarvestRuneStrike> InPierceClass,
		const float InLeapRadius,
		const float InPierceDepth)
	{
		LeapStrikeClass = InLeapClass;
		PierceStrikeClass = InPierceClass;
		LeapRadius = InLeapRadius;
		PierceDepth = InPierceDepth;
	}
	void ConfigureWorkers(const TSubclassOf<ARpgHarvestSwarm> InSwarmClass, const FRpgHarvestSwarmParams& InWorker, const float InRadius)
	{
		WorkerSwarmClass = InSwarmClass;
		Worker = InWorker;
		WorkerRadius = InRadius;
	}
	void ConfigureToolSwing(const FRpgSwingTraceParams& InSwingTrace, const FGameplayTag InNoYieldCue)
	{
		StrikeSource = ERpgHarvestStrikeSource::ToolSwing;
		SwingTrace = InSwingTrace;
		NoYieldGameplayCue = InNoYieldCue;
	}

	/** Montage whose cycle the server times the swing by; the test swing itself never plays it. */
	void ConfigureMontage(UAnimMontage* InMontage) { HarvestMontage = InMontage; }

	/** Milliseconds the swinger's own montage freezes at a swing's first resource contact. */
	void ConfigureHitstop(const float InMilliseconds) { ToolSwingHitstopMilliseconds = InMilliseconds; }

	/** Effect and authored amount of the focus each landed execution adds. */
	void ConfigureFocusGain(const TSubclassOf<UGameplayEffect> InEffect, const float InPerSwing)
	{
		FocusGainEffect = InEffect;
		FocusGainPerSwing = InPerSwing;
	}

	/** World times at which the hitstop froze and resumed the swing's montage, in order; the test swing plays none. */
	const TArray<double>& GetMontagePauseTimes() const { return MontagePauseTimes; }
	const TArray<double>& GetMontageResumeTimes() const { return MontageResumeTimes; }

	/**
	 * Makes this ability repeat while its input is held and yield to other exclusive abilities, as the tool swing assets
	 * do, or restores the native defaults. Input processing reads the class default, so tests call it on the class
	 * default too and restore that when they finish. New instances of a native class do not copy a class default
	 * changed at runtime, so the granted instance needs its own call.
	 */
	void ConfigureHoldToRepeat(const bool bHold)
	{
		ActivationPolicy = bHold ? ERpgAbilityActivationPolicy::WhileInputActive : ERpgAbilityActivationPolicy::OnInputTriggered;
		ActivationGroup = bHold ? ERpgAbilityActivationGroup::Exclusive_Replaceable : ERpgAbilityActivationGroup::Exclusive_Blocking;
	}

	/** Trace the last swing started with, after its tuned reach; tests trace it themselves. */
	const FRpgSwingTraceParams& GetPlayedSwingTrace() const { return PlayedSwingTrace; }

	/** Swings started since the ability was granted. */
	int32 GetPlayedSwingCount() const { return PlayedSwingCount; }

protected:
	/** Starts the swing without a montage or animated mesh; tests feed its hits through HandleToolSwingHitsForTests. */
	virtual bool PlayToolSwing(const FRpgSwingTraceParams& Params) override
	{
		PlayedSwingTrace = Params;
		++PlayedSwingCount;
		return true;
	}

	/** Records the hitstop's freeze and resume instead of pausing a montage. */
	virtual bool SetToolSwingMontagePaused(const bool bPaused) override
	{
		const UWorld* World = GetWorld();
		(bPaused ? MontagePauseTimes : MontageResumeTimes).Add(World ? World->GetTimeSeconds() : 0.0);
		return true;
	}

private:
	FRpgSwingTraceParams PlayedSwingTrace;
	int32 PlayedSwingCount = 0;
	TArray<double> MontagePauseTimes;
	TArray<double> MontageResumeTimes;
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
	void ConfigureStruckTargetLinger(const float InSeconds) { StruckTargetLingerSeconds = InSeconds; }
	AActor* GetAreaMarkerForTest() const { return AreaMarker; }
};
