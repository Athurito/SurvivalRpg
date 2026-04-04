#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RpgLootTable.generated.h"

class URpgItemDefinition;

USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgLootTableEntry
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ToolTip = "Item definition that can be produced by this loot entry. The spawned pickup inherits its fragments, visuals, and equipment behavior from that definition."))
	TObjectPtr<URpgItemDefinition> ItemDefinition = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "0.0", ToolTip = "Relative chance for this entry when the table rolls one item. A weight of 2 is chosen about twice as often as a weight of 1."))
	float Weight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "1", ToolTip = "Minimum number of copies produced when this entry wins a roll."))
	int32 MinDropCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "1", ToolTip = "Maximum number of copies produced when this entry wins a roll. Keep this equal to MinDropCount for a fixed amount."))
	int32 MaxDropCount = 1;
};

UCLASS(BlueprintType, Const)
class SURVIVALRPG_API URpgLootTable : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("RpgLootTable"), GetFName());
	}

	TArray<TObjectPtr<URpgItemDefinition>> RollItemDefinitions(FRandomStream& RandomStream) const;

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (AllowPrivateAccess = "true", ClampMin = "1", ToolTip = "Minimum number of loot-entry rolls executed each time this table is used."))
	int32 MinRolls = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (AllowPrivateAccess = "true", ClampMin = "1", ToolTip = "Maximum number of loot-entry rolls executed each time this table is used."))
	int32 MaxRolls = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (AllowPrivateAccess = "true", ToolTip = "Weighted list of item definitions that this table can generate."))
	TArray<FRpgLootTableEntry> Entries;
};
