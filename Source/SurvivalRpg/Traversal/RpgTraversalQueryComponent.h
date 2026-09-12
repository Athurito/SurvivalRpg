#pragma once

#include "Components/ActorComponent.h"
#include "RpgTraversalQueryComponent.generated.h"

class ACharacter;
class UAnimMontage;
class UPrimitiveComponent;

/** Game-thread result of a designer-owned traversal query; it describes a proposal, never authoritative movement. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgTraversalQueryResult
{
	GENERATED_BODY()

	/** Source GASP action value: 0 none, 1 hurdle, 2 vault, 3 mantle. Only mantle is admitted by this slice. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal") uint8 ActionType = 0;
	/** Whether the prepared obstacle supplied a usable front ledge. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal") bool HasFrontLedge = false;
	/** World-space front ledge location in centimeters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal") FVector FrontLedgeLocation = FVector::ZeroVector;
	/** World-space outward normal of the front ledge. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal") FVector FrontLedgeNormal = FVector::ZeroVector;
	/** Whether the prepared obstacle supplied a usable opposite ledge. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal") bool HasBackLedge = false;
	/** World-space opposite ledge location in centimeters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal") FVector BackLedgeLocation = FVector::ZeroVector;
	/** World-space outward normal of the opposite ledge. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal") FVector BackLedgeNormal = FVector::ZeroVector;
	/** Whether the query found walkable ground beyond the obstacle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal") bool HasBackFloor = false;
	/** World-space ground position beyond the obstacle in centimeters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal") FVector BackFloorLocation = FVector::ZeroVector;
	/** Front ledge height above capsule feet in centimeters, measured by the query. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal") double ObstacleHeight = 0.0;
	/** Usable depth across the obstacle in centimeters, measured by the query. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal") double ObstacleDepth = 0.0;
	/** Opposite ledge height above its ground in centimeters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal") double BackLedgeHeight = 0.0;
	/** Exact prepared collider hit by the source query. Native validation independently checks this component. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal") TObjectPtr<UPrimitiveComponent> HitComponent = nullptr;
	/** Designer chooser's pose-matched montage; native validation checks its configured eligibility. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal") TObjectPtr<UAnimMontage> ChosenMontage = nullptr;
	/** Pose-matched montage entry time in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal") double StartTime = 0.0;
	/** Montage playback multiplier. The current source mantle family uses 1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal") double PlayRate = 1.0;
};

/** Server-checkable eligibility copied from one designer-owned mantle chooser row. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgTraversalAnimationEntry
{
	GENERATED_BODY()

	/** Allowed project-owned montage. Client proposals cannot introduce other animations. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal") TObjectPtr<UAnimMontage> Montage = nullptr;
	/** Minimum front ledge height above capsule feet, in centimeters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal") float MinHeight = 0.0f;
	/** Maximum front ledge height above capsule feet, in centimeters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal") float MaxHeight = 150.0f;
	/** Minimum horizontal approach speed in centimeters per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal") float MinSpeed = 0.0f;
	/** Maximum horizontal approach speed in centimeters per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal") float MaxSpeed = 100.0f;
	/** True selects a falling catch entry; false selects a grounded standing, walking or running entry. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal") bool bAirborne = false;
	/** Earliest pose-search entry time allowed by this chooser row, in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal") float MinStartTime = 0.0f;
	/** Latest pose-search entry time allowed by this chooser row, in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal") float MaxStartTime = 0.1f;
	/** Montage time used to validate support: the source unconditional blend-out start, or clip end when exiting is conditional. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Traversal", meta = (ClampMin = "0", Units = "s")) float HandoffTime = 0.0f;
};

/** Native query contract for copied GASP Blueprint geometry and pose selection; GAS owns execution and replication. */
UCLASS(Abstract, Blueprintable, ClassGroup = (Movement), meta = (BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgTraversalQueryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URpgTraversalQueryComponent();

	/** Runs the source geometry and chooser on the game thread without moving the character or starting a montage. */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "Rpg|Traversal")
	bool QueryTraversal(FRpgTraversalQueryResult& OutResult);

	/** Source chooser row contracts used by the authority to validate a predicted montage and entry time. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Traversal|Validation")
	TArray<FRpgTraversalAnimationEntry> AllowedMantleAnimations;

	/** Horizontal speed tolerance in cm/s for client/server sampling at a gait boundary; geometry remains independently checked. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Traversal|Validation", meta = (ClampMin = "0", ClampMax = "100", Units = "cm/s"))
	float NetworkSpeedTolerance = 75.0f;

	/** Catch-animation height tolerance in centimeters for owner/server sampling while falling; physical reach and clearance remain server checked. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Traversal|Validation", meta = (ClampMin = "0", ClampMax = "50", Units = "cm"))
	float NetworkAirborneHeightTolerance = 30.0f;

	/** Checks the proposed animation against server-observed movement and the configured source chooser row. */
	bool IsAnimationAllowed(const ACharacter& Character, const FRpgTraversalQueryResult& Result) const;
};
