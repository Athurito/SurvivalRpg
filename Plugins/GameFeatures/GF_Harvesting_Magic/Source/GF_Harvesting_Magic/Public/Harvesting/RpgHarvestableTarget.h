#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"

#include "RpgHarvestableTarget.generated.h"

class AActor;

/** Server-authored request passed from a harvesting ability or interaction to a harvestable actor or component. */
USTRUCT(BlueprintType)
struct GF_HARVESTING_MAGIC_API FRpgHarvestRequest
{
	GENERATED_BODY()

	/** Pawn or actor performing the harvest. Gameplay code must not trust a client-supplied value. */
	UPROPERTY(BlueprintReadOnly, Category = "Harvesting")
	TObjectPtr<AActor> Harvester = nullptr;

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
	 * Number of logical stock sections this request wants to extract from one target.
	 * Targets clamp the value to their remaining stock; values below one are rejected as invalid.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Harvesting", meta = (ClampMin = "1", UIMin = "1"))
	int32 RequestedSections = 1;

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
};

/** Authoritative or previewed outcome of one harvest request against one target. */
UENUM(BlueprintType)
enum class ERpgHarvestOutcome : uint8
{
	/** Stock was (or would be) extracted and its reward delivered. */
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

	/** Stock sections extracted by a commit, or the sections an evaluation would extract. */
	UPROPERTY(BlueprintReadOnly, Category = "Harvesting")
	int32 SectionsTaken = 0;

	/** Stock sections left on the target after this result. */
	UPROPERTY(BlueprintReadOnly, Category = "Harvesting")
	int32 RemainingSections = 0;

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

	/** Returns whether the request was (or would be) harvested. */
	bool IsSuccess() const
	{
		return Outcome == ERpgHarvestOutcome::Harvested;
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
