#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Engine/HitResult.h"
#include "Engine/TimerHandle.h"
#include "SurvivalRpg/Combat/RpgWeaponTraceInterpolation.h"
#include "SurvivalRpg/Physics/RpgCollisionChannels.h"
#include "SurvivalRpg/Runes/RpgWeaponSwingRecord.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "RpgSwingTraceWindow.generated.h"

class UAnimMontage;
class UGameplayAbility;
class URpgEquipmentInstance;
class UWorld;
struct FCollisionQueryParams;
struct FGameplayAbilityActorInfo;

/** Collision scan style of a swing trace window, shared by weapon attacks and tool swings. */
UENUM(BlueprintType)
enum class ERpgWeaponAttackTraceMode : uint8
{
	// Thin continuous line ribbon. Best default for blades because it avoids large forgiving volumes.
	LineTrace,

	// Sphere swept along each trace point. Useful for blunt weapons, claws, fists, or forgiving prototype hits.
	SphereSweep,

	// Capsule swept along each trace point. Useful for long, thick hit volumes such as monster limbs.
	CapsuleSweep,

	// Box swept along each trace point. Useful when the hit area should feel broad and flat.
	BoxSweep
};

/** Which machine traces a swing window, and what its hits mean there. */
enum class ERpgSwingTraceRole : uint8
{
	/** The server: its hits are the gameplay result. */
	Authority,

	/** A remote owning client: it traces the same window only for its own hit feedback, never for gameplay. */
	OwnerFeedback,

	/** Any other machine; it traces nothing. */
	None
};

/**
 * Trace shape and sampling of one swing window: which trace points the swung equipment carries and how the motion
 * between two samples is traced. Designer-tuned static data, built per swing from an attack or tool definition. The
 * server traces it for gameplay and a remote owner for its own hit feedback; it is never replicated.
 */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgSwingTraceParams
{
	GENERATED_BODY()

	/** Collision scan style while the window is open. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trace")
	ERpgWeaponAttackTraceMode TraceMode = ERpgWeaponAttackTraceMode::LineTrace;

	/** Also traces the ribbon between neighbouring trace points, not only each trace point's own path. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trace")
	bool bTraceBetweenSockets = true;

	/** Largest spacing in cm between generated ribbon points. Lower values are denser but more expensive. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trace", meta = (ClampMin = "1.0", EditCondition = "bTraceBetweenSockets", ForceUnits = "cm"))
	float TraceInterpolationDistance = 12.0f;

	/** Radius in cm of SphereSweep and CapsuleSweep; at least 1 cm is used. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trace", meta = (ClampMin = "1.0", ForceUnits = "cm"))
	float TraceRadius = 12.0f;

	/** Half height in cm of CapsuleSweep; the capsule is never shorter than TraceRadius. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trace", meta = (ClampMin = "1.0", ForceUnits = "cm"))
	float TraceCapsuleHalfHeight = 24.0f;

	/** Half extents in cm of BoxSweep; each axis uses at least 1 cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trace")
	FVector TraceBoxExtent = FVector(12.0f, 12.0f, 12.0f);

	/**
	 * Ordered trace points from base to tip, at least two: scene component names or sockets on the equipment's spawned
	 * actors, else sockets on the avatar's gameplay mesh.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trace")
	TArray<FName> TracePointSockets;

	/**
	 * Centimeters by which every sample moves the last trace point beyond the tip, along the line from the
	 * second-to-last trace point through it, so the swing reaches farther than the swung mesh. Zero traces the authored
	 * points. Tool swings add their tuned reach (Ability.Tuning.Harvest.SwingReach) per swing.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trace", meta = (ClampMin = "0.0", UIMax = "400.0", ForceUnits = "cm"))
	float TipExtension = 0.0f;

	/**
	 * Seconds between samples while the window is open, at least 0.001 s. Lower values are more accurate but more
	 * expensive. A blade that turned more than RpgWeaponTraceInterpolation::MaxSubstepAngleDegrees between two samples is
	 * traced along its swing arc in sub-steps.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trace", meta = (ClampMin = "0.001", ForceUnits = "s"))
	float TraceSampleInterval = 0.016f;

	/** Collision channel of every line and sweep query. Weapon attacks use the weapon trace channel. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trace")
	TEnumAsByte<ECollisionChannel> TraceChannel = Rpg_TraceChannel_Weapon;

	/** Whether the trace points, sample interval and the selected shape's extents can be traced. */
	bool HasValidTraceData() const;

	/** Moves the last of TracePointTransforms TipExtension beyond the tip; nothing without an extension or two points. */
	void ExtendTip(TArray<FTransform>& TracePointTransforms) const;
};

/**
 * Receivers of a swing window's events, bound by the ability that runs it. The window keeps its sink until the next
 * Begin, so a callback may end the ability; Begin itself must not run inside a callback.
 */
struct FRpgSwingTraceSink
{
	/** Hits of one trace query in query order, neither filtered nor de-duplicated; never called with an empty array. */
	TDelegate<void(const TArray<FHitResult>& HitResults, ERpgSwingTraceRole Role)> OnHits;

	/** Every trace query in order, before its hits; for example to record the swing for an Echo replay. */
	TDelegate<void(const FVector& TraceStart, const FVector& TraceEnd, ERpgRecordedTraceShape Shape, const FVector& ShapeExtent)> OnTraceQuery;

	/** The window opened, before its first sample. Role None: this machine does not trace it. */
	TDelegate<void(ERpgSwingTraceRole Role)> OnWindowOpened;

	/** The window closed, after its final sample. */
	TDelegate<void(ERpgSwingTraceRole Role)> OnWindowClosed;

	/** Lifecycle record of the window for the ability's debug log; Detail builds its text only when called. */
	TDelegate<void(const TCHAR* Stage, TFunctionRef<FString()> Detail)> OnLifecycle;
};

/**
 * The attack window and blade trace of one montage-driven swing, shared by melee attacks and tool swings.
 *
 * The owning ability calls Begin once its montage plays. A remote owner opens and closes the window from the montage's
 * AttackWindowStart/AttackWindowEnd notifies. Authority opens on whichever comes first of the notify (accepted only once
 * the running montage instance reached the authored time), a once-per-frame poll of the montage position, and a clock
 * schedule; it closes the same way. While open, the window samples the trace points of the equipment's spawned actors
 * (or the avatar's mesh) about once per frame and traces the motion since the last sample, sub-stepped along the swing
 * arc (RpgWeaponTraceInterpolation). Closing traces a final sample.
 *
 * Hits go to the sink without de-duplication or damage; the ability owns both. Plain runtime state of one ability
 * instance, never replicated. Timers are bound weakly to the ability, which must own this object.
 */
class SURVIVALRPG_API FRpgSwingTraceWindow
{
public:
	FRpgSwingTraceWindow() = default;
	UE_NONCOPYABLE(FRpgSwingTraceWindow);

	/**
	 * Whether a window ending at WindowEndTime (montage seconds) leaves more real time than the auto blend-out trigger at
	 * EffectivePlayRate. Also reports the remaining real time and the trigger in seconds, or -1 for invalid input.
	 */
	static bool IsWindowEndBeforeAutoBlendOut(
		float MontageLength,
		float WindowEndTime,
		float EffectivePlayRate,
		float AuthoredBlendOutTriggerTime,
		float& OutRemainingPlayTime,
		float& OutBlendOutTriggerSeconds);

	/**
	 * Validates the montage's swing contract at EffectivePlayRate (montage play rate times rate scale): one section at
	 * zero without links, no time-stretch curve, exactly one queued, unfiltered, always-triggering AttackWindowStart/End
	 * notify pair that also fires on dedicated servers, ordered inside the montage and ending before auto blend-out.
	 * Outputs the window in montage seconds, or the failure reason.
	 */
	static bool ResolveWindowTiming(
		const UAnimMontage* Montage,
		float EffectivePlayRate,
		float& OutStartTime,
		float& OutEndTime,
		FString& OutFailureReason);

	/**
	 * Prepares the window of a swing whose montage InAbility just started. On authority it also schedules the window from
	 * the running montage; false means the montage did not start or failed its contract, and the swing must abort. Call
	 * once per swing, after StopTracing or Reset.
	 */
	bool Begin(
		UGameplayAbility& InAbility,
		URpgEquipmentInstance* InEquipment,
		UAnimMontage* InMontage,
		const FRpgSwingTraceParams& InParams,
		FRpgSwingTraceSink InSink);

	/** Handles the montage's AttackWindowStart notify on any machine. */
	void HandleStartNotify();

	/** Handles the montage's AttackWindowEnd notify on any machine. */
	void HandleEndNotify();

	/**
	 * Authority: the running montage holds still for Seconds, for example during a listen-server host's own local
	 * hitstop, so the clock schedule's pending open and close move back by the same time. The montage poll follows the
	 * montage anyway. Nothing happens without a schedule or on another machine.
	 */
	void DelayAuthoritySchedule(float Seconds);

	/**
	 * Closes an open window after its final sample and clears the authority schedule. bLogMissingEndNotify warns when the
	 * window was still open without its end notify, for example at the montage's end.
	 */
	void StopTracing(bool bLogMissingEndNotify = false);

	/** Clears the timers and every state of the swing and its activation; the debug counters for tests keep counting. */
	void Reset();

	bool IsWindowOpen() const { return bWindowOpen; }
	bool HasReceivedStartNotify() const { return bReceivedStartNotify; }
	bool HasReceivedEndNotify() const { return bReceivedEndNotify; }
	/** Authority: whether the clock schedule, not the notify or the montage poll, opened this swing's window. */
	bool WasOpenedBySchedule() const { return bOpenedBySchedule; }
	/** Authority: whether the clock schedule closed this swing's window. */
	bool WasClosedBySchedule() const { return bClosedBySchedule; }
	/** Authority trace samples since the last Reset. */
	int32 GetAuthorityTraceSampleCount() const { return AuthorityTraceSamples; }
	/** Authority samples since the last Reset that could not resolve every trace point. */
	int32 GetAuthorityTracePointFailureCount() const { return AuthorityTracePointFailures; }
	/** Most sub-steps one authority sample split its motion into since the last Reset; above 1 the pose jumped. */
	int32 GetAuthorityMaxTraceSubstepCount() const { return AuthorityMaxTraceSubsteps; }
	bool HasPendingTimers() const;
	/** Whether the window is open, scheduled or holds a sample. */
	bool HasResidualState() const;

#if WITH_DEV_AUTOMATION_TESTS
	uint32 GetAuthorityWindowOpenCountForTests() const { return AuthorityWindowOpenCountForTests; }
	uint32 GetAuthorityWindowCloseCountForTests() const { return AuthorityWindowCloseCountForTests; }
	uint32 GetAuthorityTraceSampleCountForTests() const { return AuthorityTraceSampleCountForTests; }

	/**
	 * Traces the motion from Previous to Current in World as an authority sample would, without an ability or equipment;
	 * the samples hold the authored trace points, and InParams' tip extension applies. Nothing is ignored. Returns the
	 * sub-step count.
	 */
	int32 TraceSampleMotionForTests(
		UWorld& World,
		const FRpgSwingTraceParams& InParams,
		const RpgWeaponTraceInterpolation::FBladeSample& Previous,
		const RpgWeaponTraceInterpolation::FBladeSample& Current,
		FRpgSwingTraceSink InSink);
#endif

private:
	UWorld* GetWorld() const;
	const FGameplayAbilityActorInfo* GetActorInfo() const;
	bool IsAbilityActive() const;
	bool IsAuthority() const;
	ERpgSwingTraceRole GetRole() const;
	/** Sets a timer on the ability's world that calls Callback while the ability lives. */
	void SetWindowTimer(
		FTimerHandle& Handle,
		void (FRpgSwingTraceWindow::*Callback)(),
		float Rate,
		bool bLoop = false,
		float FirstDelay = -1.0f,
		bool bMaxOncePerFrame = false);
	void LogLifecycle(const TCHAR* Stage, const FString& Detail = FString()) const;
	void LogLifecycleLazy(const TCHAR* Stage, TFunctionRef<FString()> DetailBuilder) const;

	bool ScheduleAuthorityWindow();
	void ClearAuthoritySchedule();
	void OnAuthorityScheduleStarted();
	void OnAuthorityScheduleEnded();
	/**
	 * Authority, once per frame after the animation update while the window is scheduled: opens and closes the window
	 * when the running montage reaches the authored times. A remote client's montage advances with its moves, several
	 * per server frame, and its queued notifies may then never arrive; the clock schedule lags the montage.
	 */
	void OnAuthorityMontagePoll();
	/**
	 * Whether the running swing's own montage instance has played to MontageTime. False for a notify of a replaced swing
	 * or of an earlier instance of the same montage, which never reaches this swing's window.
	 */
	bool HasActiveMontageReached(float MontageTime) const;

	void OpenWindow();
	/** Samples the body and every trace point's world transform; false while a trace point cannot be resolved. */
	bool GatherSample(RpgWeaponTraceInterpolation::FBladeSample& OutSample);
	/**
	 * Traces the motion since the previous sample. A pose that turned more than a sub-step since the last sample is
	 * traced along its swing arc.
	 */
	void PerformSample();
	void BuildTraceQueryParams(FCollisionQueryParams& QueryParams) const;
	/** Traces each sub-step from one row of trace point locations to the next while the window stays open. */
	void TraceSubsteps(
		UWorld& World,
		const TArray<TArray<FVector>>& SubstepLocations,
		const FCollisionQueryParams& QueryParams,
		ERpgSwingTraceRole Role);
	void BuildInterpolatedTracePointPairs(
		const TArray<FVector>& PreviousSocketLocations,
		const TArray<FVector>& CurrentSocketLocations,
		TArray<FVector>& OutPreviousTraceLocations,
		TArray<FVector>& OutCurrentTraceLocations) const;
	void PerformLineTraceRibbon(
		UWorld& World,
		const TArray<FVector>& PreviousSocketLocations,
		const TArray<FVector>& CurrentSocketLocations,
		const FCollisionQueryParams& QueryParams,
		ERpgSwingTraceRole Role);
	void PerformSweepTraceRibbon(
		UWorld& World,
		const TArray<FVector>& PreviousSocketLocations,
		const TArray<FVector>& CurrentSocketLocations,
		const FCollisionQueryParams& QueryParams,
		ERpgSwingTraceRole Role);
	void TraceLine(UWorld& World, const FVector& TraceStart, const FVector& TraceEnd, const FCollisionQueryParams& QueryParams, ERpgSwingTraceRole Role);
	void TraceSweep(UWorld& World, const FVector& TraceStart, const FVector& TraceEnd, const FCollisionQueryParams& QueryParams, ERpgSwingTraceRole Role);
	void ReportHits(const TArray<FHitResult>& HitResults, ERpgSwingTraceRole Role) const;

	/** The ability that owns this window; it outlives every timer, which is bound weakly to it. */
	UGameplayAbility* Ability = nullptr;
	TWeakObjectPtr<URpgEquipmentInstance> Equipment;
	TWeakObjectPtr<UAnimMontage> Montage;
	FRpgSwingTraceParams Params;
	FRpgSwingTraceSink Sink;

	/** Authored window of the running swing in montage seconds, resolved by the authority schedule. */
	float WindowStartTime = 0.0f;
	float WindowEndTime = 0.0f;
	bool bWindowOpen = false;
	bool bReceivedStartNotify = false;
	bool bReceivedEndNotify = false;
	bool bAuthorityScheduled = false;
	bool bOpenedBySchedule = false;
	bool bClosedBySchedule = false;
	int32 AuthorityTraceSamples = 0;
	int32 AuthorityTracePointFailures = 0;
	int32 AuthorityMaxTraceSubsteps = 0;
	bool bLoggedTracePointFailure = false;

	FTimerHandle TraceSampleTimerHandle;
	FTimerHandle AuthorityStartTimerHandle;
	FTimerHandle AuthorityEndTimerHandle;
	FTimerHandle AuthorityMontagePollTimerHandle;
	/** Blade pose of the open window's last trace sample; the next sample traces the motion from it. */
	RpgWeaponTraceInterpolation::FBladeSample PreviousSample;

#if WITH_DEV_AUTOMATION_TESTS
	uint32 AuthorityWindowOpenCountForTests = 0;
	uint32 AuthorityWindowCloseCountForTests = 0;
	uint32 AuthorityTraceSampleCountForTests = 0;
#endif
};
