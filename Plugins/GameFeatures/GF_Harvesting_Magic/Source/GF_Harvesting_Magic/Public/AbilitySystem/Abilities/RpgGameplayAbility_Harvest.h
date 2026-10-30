#pragma once

#include "Harvesting/RpgHarvestRuneStrike.h"
#include "Harvesting/RpgHarvestSwarm.h"
#include "Harvesting/RpgHarvestTargeting.h"
#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility_FromEquipment.h"
#include "TimerManager.h"

#include "RpgGameplayAbility_Harvest.generated.h"

class ARpgHarvestBurn;
class FRpgHarvestRewardBatch;
class UAnimMontage;
class URpgCameraMode;

/**
 * Designer-tuned repetition of an area harvest around the harvester over time, for powers that keep harvesting while
 * the player walks. Static data on the ability.
 */
USTRUCT(BlueprintType)
struct GF_HARVESTING_MAGIC_API FRpgHarvestStrideParams
{
	GENERATED_BODY()

	/**
	 * Seconds the stride keeps harvesting after the commit. Zero harvests once. Used only with the AreaAroundHarvester
	 * shape; the ability stays active for the whole stride.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stride", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "15.0", Units = "s"))
	float DurationSeconds = 0.0f;

	/**
	 * Seconds between two pulses. The first pulse harvests at the commit; each pulse harvests every target in the area
	 * around the harvester once, up to the targeting's MaxTargets.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stride", meta = (ClampMin = "0.1", UIMin = "0.1", UIMax = "2.0", Units = "s"))
	float PulseIntervalSeconds = 0.5f;
};

/**
 * Success cue of one skill tree form, for example the burning ground of Ash Wave, so a form can look different from
 * the plain power. Designer data on the ability.
 */
USTRUCT(BlueprintType)
struct GF_HARVESTING_MAGIC_API FRpgHarvestFormGameplayCue
{
	GENERATED_BODY()

	/** Owner tag that selects the cue, for example a Harvest.Form.* tag a learned node grants. Entries without a tag are ignored. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feedback")
	FGameplayTag RequiredOwnerTag;

	/** Cosmetic cue executed instead of the ability's SuccessGameplayCue while the owner has RequiredOwnerTag. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feedback", meta = (Categories = "GameplayCue"))
	FGameplayTag GameplayCue;
};

/**
 * Values of one harvest after the learned skill tree tunings of the source weapon were applied to the ability's
 * authored values. The preview resolves them each time; the server captures them when execution starts.
 */
struct GF_HARVESTING_MAGIC_API FRpgHarvestTunedValues
{
	/** Targeting with tuned area radius, target count and reach; the aim ray grows with the reach. */
	FRpgHarvestTargetingParams Targeting;

	/** Stock sections per target, or per creature strike of a swarm. */
	int32 SectionsPerTarget = 1;

	/** Work multiplier of every hit (Ability.Tuning.Harvest.Work), above 0 and at most RpgHarvestWork::MaxWorkScale. */
	float WorkScale = 1.0f;

	/** Multiplier on a weak-point hit's bonus work (Ability.Tuning.Harvest.WeakPointWork), 0 to MaxWorkScale. */
	float WeakPointWorkScale = 1.0f;

	/**
	 * Work multiplier of the next hit on a target after a weak-point hit on it (Ability.Tuning.Harvest.FollowUpWork),
	 * and the seconds it waits for that hit (Ability.Tuning.Harvest.FollowUpWindow); zero seconds turn it off.
	 */
	float FollowUpWorkScale = 1.0f;
	float FollowUpSeconds = 0.0f;

	/**
	 * Burn a direct hit lights on the resource it works, from the tunings of runes such as Brand: work per second as a
	 * fraction of the hit's own work (Ability.Tuning.Harvest.BurnWork) and burn time in seconds
	 * (Ability.Tuning.Harvest.BurnDuration). Zero in either lights none.
	 */
	float BurnWork = 0.0f;
	float BurnSeconds = 0.0f;

	/**
	 * What a further hit does to the player's running burn on the resource: restart its time
	 * (Ability.Tuning.Harvest.BurnRefresh above 0), or release its remaining work at once times BurnDischarge
	 * (Ability.Tuning.Harvest.BurnDischarge above 0), which wins over a refresh.
	 */
	bool bBurnRefresh = false;
	float BurnDischarge = 0.0f;

	/**
	 * Echo of runes such as Echo: every EchoHits accepted own hits, the struck resource takes EchoWork times the hit's
	 * own work once more (Ability.Tuning.Harvest.EchoWork, .EchoHits). Zero EchoWork echoes nothing.
	 */
	float EchoWork = 0.0f;
	int32 EchoHits = 3;

	/**
	 * Share of a depleting hit's actual overkill that runes such as Kraftuebertrag carry to the nearest other resource
	 * (Ability.Tuning.Harvest.TransferWork). Zero carries nothing.
	 */
	float TransferWork = 0.0f;

	/**
	 * Above zero, echoes and transfers also strike a second resource nearby, and each strike does this fraction of the
	 * work (Ability.Tuning.Harvest.RuneFlowSplit).
	 */
	float RuneFlowSplit = 0.0f;

	/**
	 * Leap of runes such as Erntesprung: every own hit leaps on from resource to resource of the tool, and each leap
	 * does LeapWork times the hit's own work (Ability.Tuning.Harvest.LeapWork). Zero leaps nowhere.
	 */
	float LeapWork = 0.0f;

	/**
	 * Pierce of runes such as Durchschlag: every own hit strikes the next resource behind the struck one with PierceWork
	 * times the hit's own work (Ability.Tuning.Harvest.PierceWork). Zero pierces nothing.
	 */
	float PierceWork = 0.0f;

	/** Multiplier on the reach of leaps, pierces and wandering burns (Ability.Tuning.Harvest.SpreadReach), above 0. */
	float SpreadReach = 1.0f;

	/**
	 * Above zero, every leap step and every pierce strikes two resources, and each strike does this fraction of the work
	 * (Ability.Tuning.Harvest.SpreadSplit).
	 */
	float SpreadSplit = 0.0f;

	/**
	 * Whether a burn whose resource runs empty moves on to the nearest other resource of the tool within the leap's
	 * reach (Ability.Tuning.Harvest.BurnWander above 0).
	 */
	bool bBurnWander = false;

	/** Swarm with tuned creature count, rest and strike radius. */
	FRpgHarvestSwarmParams Swarm;

	/** Yield conversions whose owner tag the ability's owner has. */
	TArray<FRpgHarvestYieldConversion> YieldConversions;

	/** Success cue of the first form whose owner tag the owner has, otherwise the ability's SuccessGameplayCue. */
	FGameplayTag SuccessGameplayCue;
};

/**
 * Abstract mechanism for tool swings and harvesting powers. Concrete abilities are GA_* Blueprint assets that only
 * configure identity, targeting, sections, montage, cues, cooldown, and unlock requirements.
 *
 * The native part owns what must not depend on Blueprint flow:
 * - Server-side target selection at the moment of extraction, with the same query the owning client previews.
 * - Exactly one commit per activation, at the authored montage notify time resolved from montage data,
 *   because server-side animation notifies are not authoritative.
 * - Cancel safety: ending or cancelling the ability before the commit, for example by switching tools, never yields loot.
 * - Optional hold-to-aim: the ability previews while its input is held and executes on release.
 *
 * - Optional stride: with the AreaAroundHarvester shape and a stride duration, the commit starts a stride that harvests
 *   around the walking harvester in pulses and delivers once when it ends.
 * - Chains: a target the commit depletes inside a URpgHarvestChainComponent box takes the box's other resources
 *   along in the same delivery, and the preview marks them.
 *
 * Tool category and harvest power come from the source equipment's item (URpgInventoryFragment_HarvestingTool).
 * Learned skill tree nodes of that weapon tune the authored values through Ability.Tuning.Harvest.* tags (area radius,
 * reach, target count, sections, swarm creatures, rest and strike radius, cooldown); preview and commit use the same
 * tuned values.
 * Blueprint children must not implement the ActivateAbility event; use On Harvest Resolved and cues for feedback.
 */
UCLASS(Abstract, Blueprintable)
class GF_HARVESTING_MAGIC_API URpgGameplayAbility_Harvest : public URpgGameplayAbility_FromEquipment
{
	GENERATED_BODY()

public:
	explicit URpgGameplayAbility_Harvest(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual ~URpgGameplayAbility_Harvest() override;

	/**
	 * Selects and evaluates this ability's current targets for Spec without mutating anything, followed by the
	 * resources chain boxes would add (bChained). Valid on the server and on the owning client; the targeting preview
	 * uses it. The server commit runs the same query from the aim captured when execution started (press, or release
	 * of a held aim), so turning the camera during the swing does not change what is hit.
	 */
	void EvaluateTargets(
		const FGameplayAbilitySpec& Spec,
		const FGameplayAbilityActorInfo& ActorInfo,
		FRpgHarvestPreview& OutPreview) const;

	/**
	 * Resolves this ability's targeting, sections and swarm for Spec with the learned skill tree tunings of its source
	 * weapon, and the yield conversions its owner's tags activate. Valid on the server and on the owning client, which
	 * see the same tree state and tags.
	 */
	void ResolveTunedValues(
		const FGameplayAbilitySpec& Spec,
		const FGameplayAbilityActorInfo& ActorInfo,
		FRpgHarvestTunedValues& OutValues) const;

	/** Returns whether this ability previews while its input is held and executes on release. */
	bool IsAimWhileInputHeld() const { return bAimWhileInputHeld; }

	/** Returns whether this ability's swings can strike a resource's active weak point for bonus sections. */
	bool CanHitWeakPoints() const { return bCanHitWeakPoints && Targeting.Shape == ERpgHarvestTargetShape::SingleTarget; }

	/** Returns the burn this ability's direct hits light while the source tool's runes give burn work, or null. */
	TSubclassOf<ARpgHarvestBurn> GetBurnClass() const { return BurnClass; }

	/** Strike an Echo sends onto the struck resource, or null when this ability echoes nothing. */
	TSubclassOf<ARpgHarvestRuneStrike> GetEchoStrikeClass() const { return EchoStrikeClass; }

	/** Strike a Kraftuebertrag sends with a depleting hit's overkill, or null when this ability carries nothing. */
	TSubclassOf<ARpgHarvestRuneStrike> GetTransferStrikeClass() const { return TransferStrikeClass; }

	/** Strike an Erntesprung sends from resource to resource, or null when this ability leaps nowhere. */
	TSubclassOf<ARpgHarvestRuneStrike> GetLeapStrikeClass() const { return LeapStrikeClass; }

	/** Strike a Durchschlag drives into the resource behind the struck one, or null when this ability pierces nothing. */
	TSubclassOf<ARpgHarvestRuneStrike> GetPierceStrikeClass() const { return PierceStrikeClass; }

	/** Returns the speed in cm/s at which this ability's presentation travels from target to target; 0 is instant. */
	float GetPresentationWaveSpeed() const { return PresentationWaveSpeed; }

	/** Returns whether this ability harvests every target in an area rather than one target it singled out. */
	bool HarvestsArea() const { return Targeting.Shape != ERpgHarvestTargetShape::SingleTarget; }

	/** Returns whether this ability's commit summons a swarm that harvests its area instead of harvesting directly. */
	bool SummonsSwarm() const { return SwarmClass && Targeting.Shape == ERpgHarvestTargetShape::AreaAtAimPoint; }

	/** Returns whether this ability's commit starts a stride that harvests around the walking harvester for a while. */
	bool HasStride() const
	{
		return Stride.DurationSeconds > 0.0f && Targeting.Shape == ERpgHarvestTargetShape::AreaAroundHarvester;
	}

	/** Server: returns whether a stride of the current activation is running. */
	bool IsStriding() const { return bStriding; }

	/** Returns the stable harvest ability id carried by every request of this ability. */
	FGameplayTag GetHarvestAbilityId() const { return HarvestAbilityId; }

	/**
	 * Resolves the delay in seconds from montage start to the single notify that sends CommitEventTag.
	 * Fails when the notify is missing or duplicated, lies outside the montage, or the play rate is invalid.
	 */
	static bool ResolveCommitDelay(
		const UAnimMontage* Montage,
		FGameplayTag CommitEventTag,
		float PlayRate,
		float& OutDelaySeconds,
		FString& OutFailureReason);

protected:
	//~ UGameplayAbility interface
	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags,
		const FGameplayTagContainer* TargetTags,
		FGameplayTagContainer* OptionalRelevantTags) const override;
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;
	/** Applies the cooldown effect with its duration tuned by Ability.Tuning.Harvest.Cooldown. */
	virtual void ApplyCooldown(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;
	//~ End UGameplayAbility interface

	/**
	 * Server-side notification after the commit, with every selected target and its committed result.
	 * Use it for cosmetic follow-ups only; loot and stock have already been resolved. Not called when the commit
	 * summons a swarm, whose creatures harvest later. A stride calls it after every pulse that harvested.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Rpg|Harvesting", DisplayName = "On Harvest Resolved")
	void K2_OnHarvestResolved(const TArray<FRpgHarvestTargetEvaluation>& Results);

	/** Stable semantic id below Ability.Harvesting sent with every request, for example Ability.Harvesting.PickaxeStrike. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting", meta = (Categories = "Ability.Harvesting"))
	FGameplayTag HarvestAbilityId;

	/**
	 * Target selection used by the server commit and by the owning client's preview. Skill tree nodes tune AreaRadius,
	 * MaxTargets and MaxReachFromAvatar (Ability.Tuning.Harvest.AreaRadius, .MaxTargets, .Reach).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Targeting")
	FRpgHarvestTargetingParams Targeting;

	/**
	 * Stock sections this ability requests from each target; targets clamp it to their remaining stock. With a swarm,
	 * the sections one creature takes per strike. Tuned by Ability.Tuning.Harvest.Sections.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Yield", meta = (ClampMin = "1", UIMin = "1", UIMax = "16"))
	int32 SectionsPerTarget = 1;

	/**
	 * When true, a single-target swing that strikes a resource's active weak point takes that resource's weak-point
	 * bonus sections in addition, from the same stock. Ignored for area targeting. Designer-tuned static data.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Yield")
	bool bCanHitWeakPoints = false;

	/**
	 * Material conversions this ability can make, for example two wood into one charcoal. Each is active while the
	 * owner has its RequiredOwnerTag, usually granted by a learned skill tree form, and applies to the merged rewards
	 * of one execution or one summoned swarm. The server captures the active ones when execution starts; the preview
	 * shows them. Designer data.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Yield")
	TArray<FRpgHarvestYieldConversion> YieldConversions;

	/** Multiplier applied to the source tool's HarvestPower; scales yield-sensitive loot rows. One keeps the tool's power. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Yield", meta = (ClampMin = "0.01", UIMin = "0.1", UIMax = "5.0"))
	float HarvestPowerScale = 1.0f;

	/** Optional trade skill that unlocks this ability, for example Skill.Gathering.Mining. Empty means always unlocked. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Unlock", meta = (Categories = "Skill"))
	FGameplayTag RequiredSkillTag;

	/** Minimum level of RequiredSkillTag; activation fails with Ability.ActivateFail.Harvesting.SkillLevel below it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Unlock", meta = (ClampMin = "1", UIMin = "1", UIMax = "100"))
	int32 MinimumSkillLevel = 1;

	/** When true, the ability previews its targets while the input is held and executes on release. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Execution")
	bool bAimWhileInputHeld = false;

	/**
	 * Camera mode the owning client uses while a hold-to-aim harvest is held, for example a higher view onto the
	 * ground; it is cleared on release or when the ability ends. Empty keeps the pawn camera. Cosmetic.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Execution", meta = (EditCondition = "bAimWhileInputHeld"))
	TSubclassOf<URpgCameraMode> AimCameraMode;

	/** Montage played on execution. Must contain exactly one RPG Gameplay Event notify sending CommitEventTag. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Execution")
	TObjectPtr<UAnimMontage> HarvestMontage;

	/** Montage play rate; scales the commit delay. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Execution", meta = (ClampMin = "0.1", UIMin = "0.5", UIMax = "3.0"))
	float MontagePlayRate = 1.0f;

	/** GameplayEvent tag of the montage notify that marks the moment of extraction. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Execution", meta = (Categories = "GameplayEvent"))
	FGameplayTag CommitEventTag;

	/** Delay in seconds from execution to the commit when no montage is set; zero commits immediately. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Execution", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "3.0", Units = "s"))
	float CommitDelaySeconds = 0.0f;

	/**
	 * Cosmetic cue the server executes on the harvester's ability system when the commit harvested something; a stride
	 * executes it on every pulse that harvests, a swarm when it is summoned. Every machine plays it, the harvesting
	 * client included.
	 * - Single-target swings: Location and Normal are the struck point.
	 * - Area harvests: Location is the area's center (the aim point, or the harvester for an area around it), Normal
	 *   points up, and RawMagnitude carries the tuned area radius in centimeters.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Feedback", meta = (Categories = "GameplayCue"))
	FGameplayTag SuccessGameplayCue;

	/**
	 * Success cues of skill tree forms. The first entry whose RequiredOwnerTag the owner has replaces
	 * SuccessGameplayCue, for example Ash Wave's burning ground on Death Wave. The server captures the choice with the
	 * tuned values when execution starts. Designer data, cosmetic.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Feedback")
	TArray<FRpgHarvestFormGameplayCue> FormSuccessGameplayCues;

	/**
	 * Cosmetic cue the server executes once for every resource the commit harvests directly, besides
	 * SuccessGameplayCue, for example Rift Grip's lightning strike on each ore vein. Every machine plays it, the
	 * harvesting client included.
	 * - Location is the top center of the resource's bounds as presented before the commit (the struck instance for
	 *   instanced resources), Normal points up, and RawMagnitude carries the bounds' horizontal radius in centimeters.
	 * - At most 8 per commit, in selection order (nearest to the area's center first), so they fit the unreliable
	 *   multicast budget per net update. Strides execute it on every pulse. Swarms and resources taken along by a
	 *   chain execute none.
	 * - It plays at the commit; the presentation wave does not delay it.
	 * Designer data.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Feedback", meta = (Categories = "GameplayCue"))
	FGameplayTag TargetGameplayCue;

	/** Cosmetic cue executed on the server at the aim point when the commit harvested nothing, for example an empty node. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Feedback", meta = (Categories = "GameplayCue"))
	FGameplayTag NoYieldGameplayCue;

	/**
	 * Speed in cm/s at which the presentation of a multi-target harvest travels outward from the harvested target
	 * nearest to the harvester, so an area power fells its targets one after another. Stock and rewards change at once,
	 * and every machine presents each target at the same server time. Zero presents every target at once. Cosmetic,
	 * designer-tuned; honored by instanced resources.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Feedback", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "5000.0", ForceUnits = "cm/s"))
	float PresentationWaveSpeed = 0.0f;

	/**
	 * Swarm the commit summons at the aim point instead of harvesting directly, for area targeting only. Its creatures
	 * work through the selected resources (up to MaxTargets) one strike at a time until they are empty, for the player;
	 * the preview marks every one of them. The swarm keeps working after a tool switch. Empty harvests directly.
	 * Designer content.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Swarm")
	TSubclassOf<ARpgHarvestSwarm> SwarmClass;

	/**
	 * Burn a direct hit lights on each resource it works while the source tool's runes give burn work and burn time,
	 * for example a socketed Brand. The server spawns one per player and resource; it replicates for its presentation.
	 * Empty lights none. Designer content.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Burn")
	TSubclassOf<ARpgHarvestBurn> BurnClass;

	/**
	 * Strike an Echo sends onto the struck resource after every EchoHitsPerEcho accepted own hits, while the source
	 * tool's runes give echo work (Ability.Tuning.Harvest.EchoWork). Its work is that fraction of the hit's own work.
	 * The server spawns it; it replicates for its presentation. Empty echoes nothing. Designer content.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Rune Flow")
	TSubclassOf<ARpgHarvestRuneStrike> EchoStrikeClass;

	/**
	 * Strike a Kraftuebertrag sends to the nearest other resource the tool can work, within RuneFlowRadius, with a
	 * share of a depleting hit's actual overkill (Ability.Tuning.Harvest.TransferWork). Empty carries nothing.
	 * Designer content.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Rune Flow")
	TSubclassOf<ARpgHarvestRuneStrike> TransferStrikeClass;

	/** Accepted own hits that charge one echo before tunings (Ability.Tuning.Harvest.EchoHits), 1 to 16. Designer-tuned. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Rune Flow", meta = (ClampMin = "1", ClampMax = "16", UIMin = "1", UIMax = "8"))
	int32 EchoHitsPerEcho = 3;

	/**
	 * Seconds an echo charge waits for the next own hit before it starts over. The charge also ends when the tool's
	 * abilities are removed. Designer-tuned.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Rune Flow", meta = (ClampMin = "0.1", UIMin = "1.0", UIMax = "15.0", Units = "s"))
	float EchoChargeTimeoutSeconds = 5.0f;

	/**
	 * Radius in centimeters around a struck resource in which a transfer, or an echo split by a talent, finds further
	 * resources the tool can work, nearest first. Designer-tuned.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Rune Flow", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "2000.0", Units = "cm"))
	float RuneFlowRadius = 800.0f;

	/**
	 * Strike an Erntesprung sends on from the struck resource to the nearest other resource the tool can work, and on
	 * from there, LeapJumps times, while the source tool's runes give leap work (Ability.Tuning.Harvest.LeapWork). Its
	 * work is that fraction of the hit's own work. Empty leaps nowhere. Designer content.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Spread")
	TSubclassOf<ARpgHarvestRuneStrike> LeapStrikeClass;

	/**
	 * Strike a Durchschlag drives into the nearest resource the tool can work behind the struck one, in the hit's
	 * direction, while the source tool's runes give pierce work (Ability.Tuning.Harvest.PierceWork). Empty pierces
	 * nothing. Designer content.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Spread")
	TSubclassOf<ARpgHarvestRuneStrike> PierceStrikeClass;

	/**
	 * Radius in centimeters in which a leap finds the next resource from the one it left, before tunings
	 * (Ability.Tuning.Harvest.SpreadReach). A burn that wanders uses it too. Designer-tuned.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Spread", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "2000.0", Units = "cm"))
	float LeapRadius = 600.0f;

	/** Leaps one own hit makes, one after another and never twice onto the same resource, 1 to 8. Designer-tuned. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Spread", meta = (ClampMin = "1", ClampMax = "8", UIMin = "1", UIMax = "4"))
	int32 LeapJumps = 2;

	/** Seconds each further leap lands after the one before, on top of the strike's own delay. Designer-tuned. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Spread", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "0.5", Units = "s"))
	float LeapStepSeconds = 0.2f;

	/**
	 * Distance in centimeters behind the struck resource in which a pierce finds the next resource, before tunings
	 * (Ability.Tuning.Harvest.SpreadReach). Designer-tuned.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Spread", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "2000.0", Units = "cm"))
	float PierceDepth = 400.0f;

	/**
	 * Half angle in degrees of the cone behind the struck resource, around the hit's direction, in which a pierce finds
	 * the next resource. Designer-tuned.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Spread", meta = (ClampMin = "1.0", ClampMax = "89.0", UIMin = "10.0", UIMax = "60.0", Units = "deg"))
	float PierceConeDegrees = 35.0f;

	/**
	 * Creature count, flight and reassignment rules of the summoned swarm. Used only with a SwarmClass. Skill tree nodes
	 * tune CreatureCount, StrikeIntervalSeconds and StrikeRadius (Ability.Tuning.Harvest.Creatures, .StrikeInterval,
	 * .StrikeRadius).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Swarm")
	FRpgHarvestSwarmParams Swarm;

	/**
	 * Duration and pulse interval of a stride. With the AreaAroundHarvester shape and a duration, the commit starts a
	 * stride: every pulse harvests the targets around the harvester where it walks, and the rewards of all pulses reach
	 * the player as one delivery when the stride ends. The ability stays active meanwhile. Switching tools, dying or
	 * cancelling ends it early, and what was harvested is still delivered. The area tunings apply to each pulse.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Stride")
	FRpgHarvestStrideParams Stride;

	/**
	 * Looping cue added on the harvester while a stride runs and removed when it ends, for example a ring around the
	 * walking player. Its RawMagnitude carries the tuned area radius in centimeters. Replicated by the ability system,
	 * so every machine shows it, also after a late join. Cosmetic.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|Harvesting|Stride", meta = (Categories = "GameplayCue"))
	FGameplayTag StrideGameplayCue;

private:
	/** Server: executes CueTag, when set, at Location for the current activation; RawMagnitude carries Radius in cm. */
	void ExecuteHarvestCue(FGameplayTag CueTag, const FVector& Location, const FVector& Normal, float Radius = 0.0f) const;

	/**
	 * Server: executes the success cue of Values for a commit or pulse that harvested FirstHarvested, at the area's
	 * center for area harvests and at the struck point otherwise.
	 */
	void ExecuteSuccessCue(
		const FRpgHarvestTunedValues& Values,
		const FRpgHarvestPreview& Selection,
		const FRpgHarvestTargetEvaluation& FirstHarvested) const;

	/** Server: summons the swarm at the selection's aim point; returns whether any creature reserved a resource. */
	bool SummonSwarm(
		const FRpgHarvestPreview& Selection,
		const FRpgHarvestRequest& RequestTemplate,
		const FRpgHarvestSwarmParams& SwarmParams);

	/**
	 * Applies the swarm to an area selection: resources the swarm cannot see become out of reach, and every other
	 * resource reports its whole remaining stock, which the swarm takes. The server and the preview run it alike.
	 */
	void PlanSwarm(
		const UWorld& World,
		const AActor& Avatar,
		const FRpgHarvestTunedValues& Values,
		FRpgHarvestPreview& InOutSelection) const;

	UFUNCTION()
	void HandleAimInputReleased(float TimeHeld);

	UFUNCTION()
	void HandleMontageCompleted();

	UFUNCTION()
	void HandleMontageInterrupted();

	/**
	 * Server: commits every target of Selection that would be harvested, with the presentation wave from the harvester,
	 * then the chains of the targets it depleted, whose results it appends to Selection. A pending follow-up of Values
	 * on a target raises that hit's work, and a weak-point hit arms the next one. An open reward batch collects the
	 * rewards. Returns the first target that was harvested, or null.
	 */
	const FRpgHarvestTargetEvaluation* CommitSelection(
		FRpgHarvestPreview& Selection,
		const FRpgHarvestRequest& RequestTemplate,
		const FRpgHarvestTunedValues& Values);

	/**
	 * Server: sends rune strikes of StrikeClass with WorkSections sections of work, as follow-up work of
	 * StruckRequest. With bIncludeStruck the struck resource takes one while it has work left (an echo); the nearest
	 * other resources of the tool within RuneFlowRadius take the rest: one for a transfer, plus one more when Values
	 * split rune flow. Two strikes each do Values.RuneFlowSplit of the work; a single strike does all of it.
	 */
	void SendRuneFlowStrikes(
		TSubclassOf<ARpgHarvestRuneStrike> StrikeClass,
		UObject* StruckReceiver,
		const FRpgHarvestRequest& StruckRequest,
		bool bStruckHasWork,
		float WorkSections,
		const FRpgHarvestTunedValues& Values,
		bool bIncludeStruck) const;

	/**
	 * Server: sends the leaps and the pierce of Values from the struck resource as follow-up work of StruckRequest. A
	 * leap goes on LeapJumps times to the nearest resource not struck by this hit yet; a pierce strikes the nearest
	 * resource behind the struck one. With Values.SpreadSplit each step strikes two resources at that share.
	 */
	void SendSpreadStrikes(UObject* StruckReceiver, const FRpgHarvestRequest& StruckRequest, float HitWorkSections, const FRpgHarvestTunedValues& Values) const;

	/**
	 * Server: collects the resources around Center within Radius that the tool could work with follow-up work of
	 * StruckRequest, nearest first, up to MaxTargets the request would harvest (zero or less collects all). Follow-up
	 * work is an area request, so protected resources stay out.
	 */
	void CollectFollowUpCandidates(
		const FRpgHarvestRequest& StruckRequest,
		const FVector& Center,
		float Radius,
		int32 MaxTargets,
		const FRpgHarvestTunedValues& Values,
		TArray<FRpgHarvestTargetEvaluation>& OutCandidates) const;

	/** Server: starts the stride of the current activation with the values captured when execution started. */
	void StartStride(const FRpgHarvestRequest& RequestTemplate, const FRpgHarvestTunedValues& Values);

	/** Server: harvests once around the harvester's current location; the reward batch collects the rewards. */
	void PulseStride();

	/** Server: the stride's duration is over; delivers and ends the activation. */
	void HandleStrideElapsed();

	/** Server: stops the stride, removes its cue, and delivers its rewards (or discards them while the world ends). */
	void FinishStride();

	/** Spends cost and cooldown, plays the montage, and schedules the authoritative commit. */
	void BeginHarvestExecution();
	void ScheduleAuthorityCommit(float DelaySeconds);
	void ExecuteAuthorityCommit();
	void SetLocalAimPreview(bool bAiming) const;
	bool MeetsSkillRequirement(const FGameplayAbilityActorInfo& ActorInfo) const;
	void BuildRequestTemplate(
		const FGameplayAbilitySpec& Spec,
		const FGameplayAbilityActorInfo& ActorInfo,
		const FRpgHarvestTunedValues& Values,
		FRpgHarvestRequest& OutRequest) const;
	static bool GetViewPoint(const FGameplayAbilityActorInfo& ActorInfo, FVector& OutLocation, FRotator& OutRotation);
	void EvaluateTargetsFromView(
		const FGameplayAbilitySpec& Spec,
		const FGameplayAbilityActorInfo& ActorInfo,
		const FRpgHarvestTunedValues& Values,
		const FVector& ViewLocation,
		const FRotator& ViewRotation,
		FRpgHarvestPreview& OutPreview,
		bool bPreviewChains) const;

	/** Server-only wakeup for the pending commit of the current activation. */
	FTimerHandle CommitTimerHandle;

	/** True between a scheduled and an executed or cancelled commit of the current activation. */
	bool bCommitPending = false;

	/** Authority-only aim captured when execution starts; the commit selects its targets from it. */
	FVector CommitViewLocation = FVector::ZeroVector;
	FRotator CommitViewRotation = FRotator::ZeroRotator;
	bool bHasCommitView = false;

	/** Authority-only tuned values captured when execution starts, so learning a node mid-swing changes nothing. */
	FRpgHarvestTunedValues CommitValues;
	bool bHasCommitValues = false;

	/**
	 * Server-only follow-up armed by the last weak-point hit: the struck target, its instance index, and the world time
	 * until which the next hit on it does follow-up work. A deadline below zero means none is armed.
	 */
	TWeakObjectPtr<UObject> FollowUpReceiver;
	int32 FollowUpItem = INDEX_NONE;
	double FollowUpDeadline = -1.0;

	/** Server-only own hits counted towards the next echo; it ends with this ability instance. */
	FRpgHarvestEchoCharge EchoCharge;

	/** Server-only state of the running stride: its pulse and end timers, captured values and pending rewards. */
	FTimerHandle StridePulseTimerHandle;
	FTimerHandle StrideEndTimerHandle;
	FRpgHarvestTunedValues StrideValues;

	UPROPERTY(Transient)
	FRpgHarvestRequest StrideRequestTemplate;

	TUniquePtr<FRpgHarvestRewardBatch> StrideRewardBatch;
	double StrideEndTime = 0.0;
	bool bStriding = false;
};
