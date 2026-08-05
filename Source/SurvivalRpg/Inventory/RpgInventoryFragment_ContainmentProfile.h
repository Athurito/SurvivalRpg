#pragma once

#include "GameplayTagContainer.h"
#include "RpgInventoryItemDefinition.h"

#include "RpgInventoryFragment_ContainmentProfile.generated.h"

/** Designer-facing risk classification for an item that must remain a concrete contained instance. */
UENUM(BlueprintType)
enum class ERpgInventoryContainmentRisk : uint8
{
	/** Stable while sealed; intended for the first containment progression slice. */
	Controlled,

	/** Actively unstable and expected to contribute meaningful containment strain. */
	Unstable,

	/** Carries corruption risk and may require shielding or quarantine. */
	Corrupted,

	/** Extreme portal object whose storage should visibly approach or exceed safe operating limits. */
	Critical
};

/** One deterministic material amount consumed by a contained-item stabilization transaction. */
USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgInventoryContainmentResourceCost
{
	GENERATED_BODY()

	/** Item definition consumed by the authoritative stabilization transaction. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Storage|Containment", meta = (AssetBundles = "Server"))
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition;

	/** Positive number of units required from available, non-reserved local resources. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Storage|Containment", meta = (ClampMin = "1", UIMin = "1"))
	int32 Count = 1;

	bool IsValid() const { return ItemDefinition != nullptr && Count > 0; }
};

/**
 * Static containment requirements for one SpecialContainedItem definition.
 *
 * The fragment describes placement requirements and risk only. Mutable instability, analysis, quarantine, and
 * extraction state belongs on the concrete item instance and remains server-authoritative.
 */
UCLASS(BlueprintType, meta = (DisplayName = "Containment Profile"))
class SURVIVALRPG_API URpgInventoryFragment_ContainmentProfile final : public URpgInventoryItemFragment
{
	GENERATED_BODY()

public:
	/** Broad static risk class used by storage presentation and containment policy. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Storage|Containment")
	ERpgInventoryContainmentRisk RiskClass = ERpgInventoryContainmentRisk::Controlled;

	/** Number of sealed containment slots occupied by one concrete item instance. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Storage|Containment", meta = (ClampMin = "1", UIMin = "1"))
	int32 RequiredSealedSlots = 1;

	/** Minimum containment strength required from the target domain or anchor. Abstract non-negative network units. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Storage|Containment", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float RequiredContainmentStrength = 0.0f;

	/** Minimum corruption protection required from the target domain or anchor. Abstract non-negative network units. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Storage|Containment", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float RequiredCorruptionProtection = 0.0f;

	/** Baseline instability contributed by this definition before mutable instance modifiers. Abstract non-negative units. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Storage|Containment", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float InstabilityValue = 0.0f;

	/** Operating strain added while this instance is stored. Abstract non-negative units evaluated by the authority. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Storage|Containment", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ContainmentStrain = 0.0f;

	/** Requires an isolated quarantine-compatible slot instead of normal shared sealed placement. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Storage|Containment")
	bool bRequiresQuarantine = false;

	/** Additional functional capabilities required by the target containment domain or anchor. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Storage|Containment", meta = (Categories = "Storage.Capability"))
	FGameplayTagContainer RequiredContainmentCapabilityTags;

	/** Exact definition/count costs consumed when the concrete item is successfully stabilized. Empty means free. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Storage|Containment|Stabilization", meta = (TitleProperty = "ItemDefinition"))
	TArray<FRpgInventoryContainmentResourceCost> StabilizationCosts;

	/**
	 * Domains the stabilized concrete instance may move into after a successful operation.
	 * Empty keeps the instance in containment; every authored entry must be a strict child of Storage.Domain.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Storage|Containment|Stabilization", meta = (Categories = "Storage.Domain"))
	FGameplayTagContainer AllowedStabilizedDestinationDomains;

	/** Exact physical item definition granted by a successful extraction. Empty disables extraction for this definition. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Storage|Containment|Extraction", meta = (AssetBundles = "Server"))
	TSubclassOf<URpgInventoryItemDefinition> ExtractionOutputDefinition;

	/** Deterministic output quantity granted when ExtractionOutputDefinition is configured; otherwise this must be zero. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Storage|Containment|Extraction", meta = (ClampMin = "0", UIMin = "0"))
	int32 ExtractionOutputCount = 0;

	/** Additional network strain caused by one successful extraction, expressed as an integer percentage from 0 to 100. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Storage|Containment|Extraction", meta = (ClampMin = "0", ClampMax = "100", UIMin = "0", UIMax = "100", Units = "Percent"))
	int32 ExtractionStrain = 0;

	/** Returns whether this definition authors one complete deterministic extraction result. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Storage|Containment")
	bool HasExtractionOutput() const
	{
		return ExtractionOutputDefinition != nullptr && ExtractionOutputCount > 0;
	}

	/** Validates local scalar and gameplay-tag requirements without inspecting mutable item state. */
	bool IsStructurallyValid() const;

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
};
