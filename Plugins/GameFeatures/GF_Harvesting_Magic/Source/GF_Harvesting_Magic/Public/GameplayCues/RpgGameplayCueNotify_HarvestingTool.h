#pragma once

#include "GameplayCueNotify_Actor.h"

#include "RpgGameplayCueNotify_HarvestingTool.generated.h"

class UStaticMesh;
class UStaticMeshComponent;

/**
 * Persistent cosmetic cue that attaches a designer-selected placeholder tool mesh to the harvester.
 *
 * Reward and tool authority remain on the server-side harvesting ability. This notify only presents
 * the replicated cue locally and may be replaced by production equipment visuals later.
 */
UCLASS(Blueprintable, meta = (DisplayName = "RPG Harvesting Tool Gameplay Cue"))
class GF_HARVESTING_MAGIC_API ARpgGameplayCueNotify_HarvestingTool
	: public AGameplayCueNotify_Actor
{
	GENERATED_BODY()

public:
	explicit ARpgGameplayCueNotify_HarvestingTool(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool OnActive_Implementation(
		AActor* MyTarget,
		const FGameplayCueParameters& Parameters) override;
	virtual bool WhileActive_Implementation(
		AActor* MyTarget,
		const FGameplayCueParameters& Parameters) override;
	virtual bool OnRemove_Implementation(
		AActor* MyTarget,
		const FGameplayCueParameters& Parameters) override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

protected:
	/** Cosmetic mesh shown while the persistent cue is active; never used for tool validation. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Harvesting Tool Cue")
	TObjectPtr<UStaticMeshComponent> ToolMeshComponent;

	/** Placeholder or production tool mesh assigned by the owning GameFeature content asset. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Harvesting Tool Cue")
	TObjectPtr<UStaticMesh> ToolMesh;

	/** Skeletal mesh socket or bone used for the cosmetic tool attachment. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Harvesting Tool Cue")
	FName AttachSocketName = TEXT("hand_r");

	/** Designer-authored offset from the selected socket, including placeholder scale. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Harvesting Tool Cue")
	FTransform ToolRelativeTransform = FTransform(
		FRotator::ZeroRotator,
		FVector::ZeroVector,
		FVector(0.35, 0.04, 0.03));

private:
	bool ShowToolOnTarget(AActor* MyTarget);
};
