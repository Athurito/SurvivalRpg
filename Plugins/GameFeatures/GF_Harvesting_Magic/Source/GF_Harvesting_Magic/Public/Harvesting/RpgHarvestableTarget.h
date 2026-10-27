#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "Templates/SubclassOf.h"
#include "UObject/Interface.h"

#include "RpgHarvestableTarget.generated.h"

class AActor;

/**
 * Quantized harvest work shared by every harvestable representation. Requests bring work, resources keep the
 * residual work of their partly worked section, and every completed section is rewarded once.
 */
namespace RpgHarvestWork
{
	/** Work units in one stock section; work is counted in hundredths of a section. */
	inline constexpr int32 UnitsPerSection = 100;

	/** Largest work one request may bring, so oversized requests cannot overflow; anything beyond it is ignored. */
	inline constexpr int32 MaxRequestUnits = 64 * UnitsPerSection;

	/** Largest FRpgHarvestRequest::WorkScale a request may carry. */
	inline constexpr float MaxWorkScale = 16.0f;
}

/** Who did a harvest's work: the harvester's own action, or follow-up work that action caused. */
UENUM(BlueprintType)
enum class ERpgHarvestWorkSource : uint8
{
	/** The harvester's own swing, power, or manual harvest. Direct work may trigger effects that react to own hits. */
	Direct,

	/**
	 * Follow-up work another harvest caused, such as a helper creature, a felled root grove, or later burn, echo, and
	 * propagation effects. Secondary work never counts as a direct hit.
	 */
	Secondary
};

/**
 * Origin of one harvest request's work, carried through the commit into its result so that effects reacting to harvest
 * work can tell own hits from follow-up work and group the work of one player action. Server-side; set by the code
 * that builds the request, never by a client.
 */
USTRUCT(BlueprintType)
struct GF_HARVESTING_MAGIC_API FRpgHarvestProvenance
{
	GENERATED_BODY()

	/** Whether the harvester's own action did this work or follow-up work it caused. */
	UPROPERTY(BlueprintReadOnly, Category = "Harvesting")
	ERpgHarvestWorkSource Source = ERpgHarvestWorkSource::Direct;

	/**
	 * Server id of the player action at the root of this work, from MakeRootActionId; follow-up work keeps the id of
	 * the action that caused it. Zero for previews and work without a root action.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Harvesting")
	int32 RootActionId = 0;

	/**
	 * Effect that produced secondary work, for example a rune proc. Empty for direct work, and for helpers or chains
	 * that the request's AbilityId already identifies.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Harvesting")
	FGameplayTag ProcTag;

	/** Returns whether this is the harvester's own work. */
	bool IsDirect() const
	{
		return Source == ERpgHarvestWorkSource::Direct;
	}

	/** Returns this provenance as follow-up work of the same root action, produced by InProcTag. */
	FRpgHarvestProvenance MakeSecondary(const FGameplayTag& InProcTag = FGameplayTag()) const
	{
		FRpgHarvestProvenance Secondary = *this;
		Secondary.Source = ERpgHarvestWorkSource::Secondary;
		if (InProcTag.IsValid())
		{
			Secondary.ProcTag = InProcTag;
		}
		return Secondary;
	}

	/** Returns a new positive root action id, unique within the server session. Game thread only. */
	static int32 MakeRootActionId();
};

/**
 * Turns one harvested material into another at a whole-number ratio, for example two wood into one charcoal.
 *
 * A harvest ability lists the conversions it can make; each is active while the ability's owner has
 * RequiredOwnerTag, which a learned skill tree form usually grants. The reward batch of one harvest applies the
 * active conversions to its merged rewards, so remainders of several targets combine; what does not fill a whole
 * output stays the input material. Input and output are plain stackable materials without itemization.
 * Designer-tuned static data.
 */
USTRUCT(BlueprintType)
struct GF_HARVESTING_MAGIC_API FRpgHarvestYieldConversion
{
	GENERATED_BODY()

	/** Owner tag that activates the conversion, for example Harvest.Form.AshWave. Empty keeps it always active. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Harvesting|Yield")
	FGameplayTag RequiredOwnerTag;

	/** Material the harvest yields that is converted, for example ID_Wood. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Harvesting|Yield")
	TSubclassOf<URpgInventoryItemDefinition> InputItem;

	/** Material delivered instead, for example ID_Charcoal. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Harvesting|Yield")
	TSubclassOf<URpgInventoryItemDefinition> OutputItem;

	/** Input items that make one output item, 1 to 100. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Harvesting|Yield", meta = (ClampMin = "1", ClampMax = "100", UIMin = "1", UIMax = "10"))
	int32 InputPerOutput = 2;

	/** Returns whether the conversion names two different items and a positive ratio. */
	bool IsValid() const
	{
		return InputItem && OutputItem && InputItem != OutputItem && InputPerOutput >= 1;
	}

	bool operator==(const FRpgHarvestYieldConversion& Other) const
	{
		return RequiredOwnerTag == Other.RequiredOwnerTag && InputItem == Other.InputItem &&
			OutputItem == Other.OutputItem && InputPerOutput == Other.InputPerOutput;
	}
};

/** Server-authored request passed from a harvesting ability or interaction to a harvestable actor or component. */
USTRUCT(BlueprintType)
struct GF_HARVESTING_MAGIC_API FRpgHarvestRequest
{
	GENERATED_BODY()

	/**
	 * Beneficiary of the harvest: the pawn, controller, or player state that receives the rewards and XP and whose trade
	 * skills are checked. Gameplay code must not trust a client-supplied value.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Harvesting")
	TObjectPtr<AActor> Harvester = nullptr;

	/**
	 * Actor that physically strikes the resource when it is not the Harvester, for example a summoned swarm. Presentation
	 * that depends on where the strike came from, such as the felling direction, uses it; rewards, XP and skill checks
	 * always use the Harvester. Null when the Harvester strikes the resource itself.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Harvesting")
	TObjectPtr<AActor> PhysicalHarvester = nullptr;

	/** Stable semantic ability id below Ability.Harvesting, used by targets and telemetry to identify the harvest method. */
	UPROPERTY(BlueprintReadOnly, Category = "Harvesting", meta = (Categories = "Ability.Harvesting"))
	FGameplayTag AbilityId;

	/** Authoritative server trace origin in world space, in centimeters. */
	UPROPERTY(BlueprintReadOnly, Category = "Harvesting")
	FVector TraceOrigin = FVector::ZeroVector;

	/** Authoritative server hit selected for this harvesting request. */
	UPROPERTY(BlueprintReadOnly, Category = "Harvesting")
	FHitResult Hit;

	/**
	 * Resource revision observed before ability cost/commit, read through IRpgHarvestableTarget::GetHarvestRevision.
	 * Revisioned targets require an exact match; INDEX_NONE is reserved for targets without revision semantics.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Harvesting")
	int32 ExpectedRevision = INDEX_NONE;

	/** Relative tool or spell power multiplied into yield-scaled loot rows. One is the baseline harvest. */
	UPROPERTY(BlueprintReadOnly, Category = "Harvesting", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float HarvestPower = 1.0f;

	/**
	 * Number of logical stock sections of work this request brings to one target, before WorkScale. Targets clamp the
	 * work to their remaining stock; values below one are rejected as invalid.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Harvesting", meta = (ClampMin = "1", UIMin = "1"))
	int32 RequestedSections = 1;

	/**
	 * Multiplier on the work of RequestedSections, above 0 and at most RpgHarvestWork::MaxWorkScale; for example 1.25
	 * for a talent that adds a quarter section, or 0.3 for an echo. Work is quantized to hundredths of a section. A
	 * section completes, and is rewarded once, when the work on it adds up to a whole section; work that falls short
	 * stays on the resource as residual progress. Changes only how fast stock depletes, never its total yield;
	 * HarvestPower scales yield. Server-side.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Harvesting", meta = (ClampMin = "0.01", ClampMax = "16.0"))
	float WorkScale = 1.0f;

	/**
	 * True for requests that complete whatever stock is left, such as a felled root grove taking its other trees along.
	 * They apply exactly the remaining work, ignore RequestedSections and WorkScale beyond validation, never strike
	 * weak points, and never report overkill.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Harvesting")
	bool bFinishResource = false;

	/** Origin of this request's work; targets copy it into the result unchanged. Server-side. */
	UPROPERTY(BlueprintReadOnly, Category = "Harvesting")
	FRpgHarvestProvenance Provenance;

	/**
	 * Harvesting tool category carried by the request, resolved on the server from the source item.
	 * Empty for tool-less harvesting such as manual interaction or spells.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Harvesting", meta = (Categories = "Tool.Harvesting"))
	FGameplayTag ToolTag;

	/**
	 * True for precise single-target swings that may strike a target's active weak point. When the swing's aim from
	 * Hit.TraceStart passes through it, or Hit.ImpactPoint lies on it, the target extracts its profile's weak-point
	 * bonus sections in addition to RequestedSections, from the same stock. False for area powers and manual harvesting.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Harvesting")
	bool bCanHitWeakPoint = false;

	/**
	 * True for area powers and other harvests that affect every resource in a region instead of one the player singled
	 * out. Resources inside a URpgHarvestProtectionComponent box reject such requests as Protected.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Harvesting")
	bool bAreaHarvest = false;

	/**
	 * Cosmetic delay in seconds, 0 to 2.5, before every machine presents this change of the target, so an area power
	 * can fell its targets one after another like a wave. The stock and the reward change at once. Honored by
	 * instanced resources (URpgHarvestableInstancesComponent); other targets present at once.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Harvesting", meta = (ClampMin = "0.0", ClampMax = "2.5", Units = "s"))
	float PresentationDelaySeconds = 0.0f;

	/**
	 * Yield conversions the harvesting ability had active when its execution started. The harvest's reward batch
	 * applies them to the merged rewards; targets ignore them. Server-side; empty for manual harvesting.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Harvesting")
	TArray<FRpgHarvestYieldConversion> YieldConversions;

	/** Returns the actor that physically strikes the resource: PhysicalHarvester when set, otherwise the Harvester. */
	AActor* GetStrikingActor() const
	{
		return PhysicalHarvester ? PhysicalHarvester.Get() : Harvester.Get();
	}

	/**
	 * Returns the quantized work of RequestedSections and WorkScale in work units, at most
	 * RpgHarvestWork::MaxRequestUnits, or zero for requests whose sections or scale are invalid. Weak-point bonus work
	 * and bFinishResource are applied by the target.
	 */
	int32 GetRequestedWork() const
	{
		if (RequestedSections < 1 || !FMath::IsFinite(WorkScale) || WorkScale <= 0.0f || WorkScale > RpgHarvestWork::MaxWorkScale)
		{
			return 0;
		}
		const double Work = static_cast<double>(RequestedSections) * RpgHarvestWork::UnitsPerSection * WorkScale;
		return static_cast<int32>(FMath::Clamp(FMath::RoundToDouble(Work), 0.0, static_cast<double>(RpgHarvestWork::MaxRequestUnits)));
	}
};

/** Authoritative or previewed outcome of one harvest request against one target. */
UENUM(BlueprintType)
enum class ERpgHarvestOutcome : uint8
{
	/** The target accepted (or would accept) the work; completed sections were extracted and their reward delivered. */
	Harvested,

	/** The target is inactive or has no remaining stock. */
	Depleted,

	/** The target requires a harvesting tool the request does not carry. */
	WrongTool,

	/** The harvester does not meet the target's trade-skill requirement. */
	SkillGate,

	/** The request's expected revision no longer matches the target. */
	Stale,

	/** Malformed request, wrong target, missing configuration, or missing authority for a commit. */
	Invalid,

	/** The reward could not be materialized; the target state is unchanged and the harvest may be retried. */
	DeliveryFailed,

	/** An area harvest reached a resource inside a harvest protection box; single-target harvests are unaffected. */
	Protected
};

/** How the reward of a successful harvest reached the harvester. */
UENUM(BlueprintType)
enum class ERpgHarvestDelivery : uint8
{
	/** No reward was delivered, for example on rejection or during a read-only evaluation. */
	None,

	/** The loot rolls produced no items; the harvest still consumed its stock. */
	Empty,

	/** The complete reward batch was added to the harvester's inventory. */
	Inventory,

	/** The complete reward batch overflowed into one replicated world drop. */
	WorldDrop
};

/** Result of evaluating or committing one harvest request. Read-only output; never authoritative input. */
USTRUCT(BlueprintType)
struct GF_HARVESTING_MAGIC_API FRpgHarvestResult
{
	GENERATED_BODY()

	/** Why the request succeeded or was rejected. */
	UPROPERTY(BlueprintReadOnly, Category = "Harvesting")
	ERpgHarvestOutcome Outcome = ERpgHarvestOutcome::Invalid;

	/**
	 * Stock sections a commit completed, or an evaluation would complete; each is rewarded and awards XP once. Zero
	 * when the work only advanced the partly worked section.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Harvesting")
	int32 SectionsTaken = 0;

	/** Stock sections left on the target after this result, including a partly worked one. */
	UPROPERTY(BlueprintReadOnly, Category = "Harvesting")
	int32 RemainingSections = 0;

	/**
	 * Work the target accepted, in RpgHarvestWork units: the request's work, including weak-point bonus work, up to
	 * the work the target had left. Positive for every successful result.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Harvesting")
	int32 WorkApplied = 0;

	/**
	 * Work of the request beyond what the target had left, in RpgHarvestWork units; positive only when the result
	 * depletes the target. Requests with bFinishResource never report overkill.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Harvesting")
	int32 OverkillWork = 0;

	/** Work the target has left after this result, in RpgHarvestWork units; zero once depleted. */
	UPROPERTY(BlueprintReadOnly, Category = "Harvesting")
	int32 RemainingWork = 0;

	/** Residual work on the partly worked section after this result, from 0 to RpgHarvestWork::UnitsPerSection - 1. */
	UPROPERTY(BlueprintReadOnly, Category = "Harvesting")
	int32 SectionWork = 0;

	/** Origin of the request's work, copied from FRpgHarvestRequest::Provenance. */
	UPROPERTY(BlueprintReadOnly, Category = "Harvesting")
	FRpgHarvestProvenance Provenance;

	/** Total stock sections of the target's current revision. */
	UPROPERTY(BlueprintReadOnly, Category = "Harvesting")
	int32 SectionCount = 0;

	/** True when a commit emptied the target's stock and depleted it. */
	UPROPERTY(BlueprintReadOnly, Category = "Harvesting")
	bool bDepleted = false;

	/** True when the request struck the target's active weak point; SectionsTaken then includes the bonus. */
	UPROPERTY(BlueprintReadOnly, Category = "Harvesting")
	bool bWeakPointHit = false;

	/**
	 * Delivery path of a committed reward; None for evaluations and rejections. A multi-target harvest ability delivers
	 * the rewards of all its targets as one batch and reports that batch's path on every harvested target.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Harvesting")
	ERpgHarvestDelivery Delivery = ERpgHarvestDelivery::None;

	/** Returns whether the request was (or would be) harvested: its work was accepted, with or without a completed section. */
	bool IsSuccess() const
	{
		return Outcome == ERpgHarvestOutcome::Harvested;
	}

	/**
	 * Returns whether this result is an accepted own hit: direct work the target accepted. It counts even when it
	 * completed no section yet, which grants no reward or XP for that section.
	 */
	bool IsAcceptedDirectHit() const
	{
		return IsSuccess() && WorkApplied > 0 && Provenance.IsDirect();
	}

	/** Builds a rejection that reports the target's current stock without extracting anything. */
	static FRpgHarvestResult MakeRejected(
		const ERpgHarvestOutcome InOutcome,
		const int32 InRemainingSections = 0,
		const int32 InSectionCount = 0)
	{
		FRpgHarvestResult Result;
		Result.Outcome = InOutcome;
		Result.RemainingSections = InRemainingSections;
		Result.SectionCount = InSectionCount;
		return Result;
	}
};

/** Blueprint/C++ contract implemented by resource actors or components that accept harvesting requests. */
UINTERFACE(BlueprintType)
class GF_HARVESTING_MAGIC_API URpgHarvestableTarget : public UInterface
{
	GENERATED_BODY()
};

class GF_HARVESTING_MAGIC_API IRpgHarvestableTarget
{
	GENERATED_BODY()

public:
	/**
	 * Returns the current revision for the harvestable addressed by Hit. Implementations return INDEX_NONE when Hit
	 * does not address them. Read-only and valid on the server and on clients with replicated state.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Rpg|Harvesting")
	int32 GetHarvestRevision(const FHitResult& Hit) const;

	/**
	 * Read-only evaluation of what CommitHarvest would do with the current replicated state, without mutating anything.
	 * Valid on the server and on owning clients for target previews; the server re-evaluates inside CommitHarvest.
	 * Unimplemented targets report Invalid.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Rpg|Harvesting")
	FRpgHarvestResult EvaluateHarvest(const FRpgHarvestRequest& Request) const;

	/**
	 * Extracts stock and delivers rewards on authority. Implementations must not mutate on a rejected result.
	 * Unimplemented targets report Invalid.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Rpg|Harvesting")
	FRpgHarvestResult CommitHarvest(const FRpgHarvestRequest& Request);
};
