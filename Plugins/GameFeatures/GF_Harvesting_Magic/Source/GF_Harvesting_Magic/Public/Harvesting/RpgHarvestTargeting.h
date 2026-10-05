#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Harvesting/RpgHarvestableTarget.h"

#include "RpgHarvestTargeting.generated.h"

class AActor;
class UWorld;

/** How a harvest ability selects its targets from the harvester's view. */
UENUM(BlueprintType)
enum class ERpgHarvestTargetShape : uint8
{
	/** The first harvestable hit along the view ray, such as a tool swing or a focused spell. */
	SingleTarget,

	/**
	 * Every harvestable within AreaRadius around the point the view ray hits, such as an area power. When the ray
	 * hits nothing, the aim point is the ground below the ray end, kept within MaxReachFromAvatar.
	 */
	AreaAtAimPoint
};

/**
 * Designer-tuned static targeting rules of one harvest ability. Read by the server at commit time and by the
 * owning client's preview, so both select targets with the same query.
 */
USTRUCT(BlueprintType)
struct GF_HARVESTING_MAGIC_API FRpgHarvestTargetingParams
{
	GENERATED_BODY()

	/** Target selection shape. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting")
	ERpgHarvestTargetShape Shape = ERpgHarvestTargetShape::SingleTarget;

	/** Length of the view ray from the camera, in centimeters; covers the camera boom plus the useful range. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float MaxAimDistance = 800.0f;

	/** Sweep radius of the view ray in centimeters; zero uses a line trace. Larger values forgive imprecise aim. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float AimRadius = 0.0f;

	/**
	 * Maximum distance in centimeters between the harvester and the hit (single target) or the aim point (area).
	 * Targets beyond it are previewed as out of reach and never committed.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float MaxReachFromAvatar = 250.0f;

	/** Radius around the aim point in centimeters that collects targets. AreaAtAimPoint only. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm", EditCondition = "Shape == ERpgHarvestTargetShape::AreaAtAimPoint"))
	float AreaRadius = 0.0f;

	/**
	 * Maximum number of targets an area harvests, nearest to the aim point first. Nearer targets the ability cannot
	 * harvest, such as protected or depleted ones, are previewed but do not count. AreaAtAimPoint only.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting", meta = (ClampMin = "1", UIMin = "1", EditCondition = "Shape == ERpgHarvestTargetShape::AreaAtAimPoint"))
	int32 MaxTargets = 1;

	/** Collision channel used by the view ray and the area overlap. Harvestable meshes must block or overlap it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;
};

/** One target selected by a harvest query together with its read-only evaluation. Cosmetic outside the server commit. */
USTRUCT(BlueprintType)
struct GF_HARVESTING_MAGIC_API FRpgHarvestTargetEvaluation
{
	GENERATED_BODY()

	/** Actor or component implementing IRpgHarvestableTarget for this target. */
	UPROPERTY(BlueprintReadOnly, Category = "Harvesting")
	TWeakObjectPtr<UObject> Receiver;

	/** Hit that addresses the target; Item carries the instance index for instanced resources. */
	UPROPERTY(BlueprintReadOnly, Category = "Harvesting")
	FHitResult Hit;

	/** Evaluation of the ability's request against the target, or the committed result after a server commit. */
	UPROPERTY(BlueprintReadOnly, Category = "Harvesting")
	FRpgHarvestResult Result;

	/** False when the target lies beyond MaxReachFromAvatar; such targets are only previewed. */
	UPROPERTY(BlueprintReadOnly, Category = "Harvesting")
	bool bInReach = false;

	/** Returns whether a commit would extract stock from this target. */
	bool WouldHarvest() const
	{
		return bInReach && Result.IsSuccess();
	}
};

/** Local read model of what the active harvest ability would hit. Cosmetic and never authoritative. */
USTRUCT(BlueprintType)
struct GF_HARVESTING_MAGIC_API FRpgHarvestPreview
{
	GENERATED_BODY()

	/** Ability id of the previewed harvest ability; invalid when no harvest ability is available. */
	UPROPERTY(BlueprintReadOnly, Category = "Harvesting")
	FGameplayTag AbilityId;

	/** True while a hold-to-aim ability is held; false for the always-on primary preview. */
	UPROPERTY(BlueprintReadOnly, Category = "Harvesting")
	bool bIsAiming = false;

	/** World-space point in centimeters where the view ray hit, or its end when it hit nothing. */
	UPROPERTY(BlueprintReadOnly, Category = "Harvesting")
	FVector AimPoint = FVector::ZeroVector;

	/** True when the previewed ability targets an area around AimPoint. */
	UPROPERTY(BlueprintReadOnly, Category = "Harvesting")
	bool bHasArea = false;

	/** Area radius in centimeters, valid when bHasArea is true. */
	UPROPERTY(BlueprintReadOnly, Category = "Harvesting")
	float AreaRadius = 0.0f;

	/** Selected targets, nearest first. */
	UPROPERTY(BlueprintReadOnly, Category = "Harvesting")
	TArray<FRpgHarvestTargetEvaluation> Targets;

	/** Returns whether a harvest ability is being previewed at all. */
	bool HasAbility() const
	{
		return AbilityId.IsValid();
	}

	/** Compares the presentation-relevant content so listeners are only notified about real changes. */
	bool IsEquivalent(const FRpgHarvestPreview& Other, float LocationTolerance = 5.0f) const;
};

/** Stateless target query shared by harvest abilities on the server and previews on owning clients. */
struct GF_HARVESTING_MAGIC_API FRpgHarvestTargeting
{
	/** Returns the IRpgHarvestableTarget addressed by Hit: the hit component first, then its actor, then other components. */
	static UObject* FindReceiver(const FHitResult& Hit);

	/**
	 * Selects targets from the view and evaluates RequestTemplate against each without mutating anything.
	 * Harvester, ability id, tool, power, and section count come from RequestTemplate; Hit and revision are filled in.
	 * Returns the aim point used for area selection.
	 */
	static FVector SelectAndEvaluate(
		const UWorld& World,
		const FRpgHarvestTargetingParams& Params,
		const FVector& ViewLocation,
		const FRotator& ViewRotation,
		const AActor& Avatar,
		const FRpgHarvestRequest& RequestTemplate,
		TArray<FRpgHarvestTargetEvaluation>& OutTargets);
};
