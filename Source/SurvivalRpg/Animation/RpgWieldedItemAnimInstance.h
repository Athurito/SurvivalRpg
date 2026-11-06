#pragma once

#include "Animation/AnimInstance.h"
#include "RpgWieldedItemAnimInstance.generated.h"

/**
 * Animation base for a mesh on an actor that equipment attaches to a character, such as a bow whose string follows the
 * drawing hand. Its AnimBP may not read other actors on an animation worker, so this class snapshots the wielder's
 * read-only inputs on the game thread before each update: the wielder is the owner of the mesh this item's actor is
 * attached to. The AnimBP owns every reaction to these inputs; this class decides nothing.
 *
 * Tick the mesh after the wielder's animation, such as in TG_PostUpdateWork, so the snapshot reads the wielder's final
 * pose of the frame instead of the previous one.
 */
UCLASS()
class SURVIVALRPG_API URpgWieldedItemAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	/** Designer data: bone or socket of the wielder's mesh to track, such as hand_r for a bow's drawing hand. None tracks nothing. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wielder")
	FName WielderBone;

	/**
	 * Designer data: bone of this mesh whose reference pose is the frame of WielderBoneLocation, such as a bowstring's
	 * bone at rest. None, or a bone the mesh lacks, uses the mesh's component space.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wielder")
	FName LocalFrameBone;

	/**
	 * Game-thread snapshot, cm: where WielderBone is in LocalFrameBone's reference-pose frame, in this mesh's units. For a
	 * bowstring at rest, Z is how far the hand is behind the string. Zero while bHasWielderBone is false. Cosmetic.
	 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Wielder")
	FVector WielderBoneLocation = FVector::ZeroVector;

	/** Game-thread snapshot: the item is attached to a wielder mesh that has WielderBone. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Wielder")
	bool bHasWielderBone = false;

	/**
	 * Game-thread snapshot: the wielder's projectile shot shows as nocked or drawn: its own prediction on the wielder's
	 * machine, the replicated State.Ranged.Drawing elsewhere (URpgAbilitySystemComponent::IsRangedDrawingForPresentation).
	 */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Wielder")
	bool bWielderRangedDrawing = false;
};
