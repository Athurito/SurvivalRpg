#pragma once

#include "Components/SceneComponent.h"
#include "RpgMantleAnchorComponent.generated.h"

class ACharacter;
class UPrimitiveComponent;

/** Authored entry for a static, prepared mantle obstacle; +X points across the front ledge onto its top. */
UCLASS(ClassGroup = (Movement), meta = (BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgMantleAnchorComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	URpgMantleAnchorComponent();

	/** Sibling collision component traversed by this entry. None selects the owner's primitive root. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mantle")
	FName ColliderComponentName;

	/** Standing feet position after this entry, in unscaled anchor-local centimeters; static designer data. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mantle", meta = (Units = "cm"))
	FVector LandingOffset = FVector(50.0, 0.0, 0.0);

	/** Maximum horizontal distance from character center to the front ledge, in centimeters. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mantle|Entry", meta = (ClampMin = "1", Units = "cm"))
	float MaxApproachDistance = 180.0f;

	/** Accepted lateral distance from this entry's center line, in centimeters. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mantle|Entry", meta = (ClampMin = "0", Units = "cm"))
	float EntryHalfWidth = 45.0f;

	/** Minimum dot product between character forward and entry forward; 1 requires exact alignment. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mantle|Entry", meta = (ClampMin = "0", ClampMax = "1"))
	float MinFacingDot = 0.85f;

	/** Minimum ledge height above standing character feet, in centimeters. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mantle|Entry", meta = (ClampMin = "0", Units = "cm"))
	float MinHeight = 90.0f;

	/** Maximum ledge height above standing character feet, in centimeters. Match the selected montage family. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mantle|Entry", meta = (ClampMin = "0", Units = "cm"))
	float MaxHeight = 150.0f;

	/** Resolves the static collision component. Returns null for unsupported or missing authored geometry. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Mantle")
	UPrimitiveComponent* GetTraversedComponent() const;

	/** Fixed world-space feet destination derived from authored data, identical on client and server. */
	UFUNCTION(BlueprintPure, Category = "Rpg|Mantle")
	FVector GetLandingLocation() const;

	/** Validates authored entry limits against current character geometry without mutating gameplay state. */
	bool IsEntryInRange(const ACharacter& Character) const;
};
