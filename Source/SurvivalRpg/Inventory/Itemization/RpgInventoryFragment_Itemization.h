#pragma once

#include "CoreMinimal.h"
#include "RpgItemizationTypes.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"

#include "RpgInventoryFragment_Itemization.generated.h"

class FDataValidationContext;
class URpgItemizationProfile;

/**
 * Static item-definition seam that opts a definition into generated stats and owns their versioned save payload.
 * The fragment never rolls in OnInstanceCreated; loot generation applies explicit server-authored state.
 */
UCLASS(BlueprintType, EditInlineNew)
class SURVIVALRPG_API URpgInventoryFragment_Itemization final
	: public URpgInventoryItemFragment
{
	GENERATED_BODY()

public:
	/** Generation rules for this definition. Required before a generated state may be applied. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Itemization")
	TObjectPtr<URpgItemizationProfile> ItemizationProfile;

	/** Returns whether a generated or explicit legacy state is compatible with this fragment. */
	bool IsItemizationStateCompatible(const FRpgItemizationState& State) const;

	virtual FName GetRuntimeStateIdentifier() const override;
	virtual int32 GetRuntimeStateVersion() const override;
	virtual bool ExportRuntimeState(
		const URpgInventoryItemInstance* Instance,
		FRpgInventoryFragmentStatePayload& OutPayload) const override;
	virtual bool ValidateRuntimeState(
		const URpgInventoryItemInstance* Instance,
		const FRpgInventoryFragmentStatePayload& Payload) const override;
	virtual bool ImportRuntimeState(
		URpgInventoryItemInstance* Instance,
		const FRpgInventoryFragmentStatePayload& Payload) const override;
	virtual void CopyRuntimeState(
		const URpgInventoryItemInstance* Source,
		URpgInventoryItemInstance* Target) const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	/**
	 * Validates self-contained historical rolls without reapplying today's generation ranges.
	 * This keeps existing equipment loadable after designers rebalance or retire profile entries.
	 */
	bool IsPersistedItemizationStateCompatible(const FRpgItemizationState& State) const;

	bool DeserializeState(
		const FRpgInventoryFragmentStatePayload& Payload,
		FRpgItemizationState& OutState) const;
};
