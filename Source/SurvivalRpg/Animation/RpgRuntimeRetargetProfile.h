#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RpgRuntimeRetargetProfile.generated.h"

class UAnimInstance;
class UIKRetargeter;
class USkeletalMesh;

/** Designer-owned optional character appearance. Retargeting never replaces the gameplay mesh or its sockets. */
UCLASS(BlueprintType)
class SURVIVALRPG_API URpgRuntimeRetargetProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Cosmetic mesh to display. Unset intentionally keeps the original gameplay mesh visible with no additional mesh tick. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Retarget")
	TObjectPtr<USkeletalMesh> TargetMesh;

	/** Presentation AnimBP with an unlimited-LOD Retarget Pose From Mesh node; reads this component's direct retargeter and gameplay mesh. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Retarget")
	TSubclassOf<UAnimInstance> RetargetAnimClass;

	/** Hard-referenced source-to-target retargeter, available before the target AnimBP initializes; never selected by a runtime asset name. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Retarget")
	TObjectPtr<UIKRetargeter> Retargeter;

	/** Cosmetic transform relative to PawnExtension's gameplay mesh, in centimeters/degrees; scale must be positive. Does not change the capsule. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Retarget")
	FTransform RelativeTransform = FTransform::Identity;

	/** Validates the configured AnimBP and rig bones. A null source checks only asset configuration; a null target is valid disabled configuration. */
	bool ValidateConfiguration(const USkeletalMesh* SourceMesh, FText& OutError) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
