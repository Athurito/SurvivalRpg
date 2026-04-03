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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	TObjectPtr<URpgItemDefinition> ItemDefinition = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "0.0"))
	float Weight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "1"))
	int32 MinDropCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "1"))
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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (AllowPrivateAccess = "true", ClampMin = "1"))
	int32 MinRolls = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (AllowPrivateAccess = "true", ClampMin = "1"))
	int32 MaxRolls = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (AllowPrivateAccess = "true"))
	TArray<FRpgLootTableEntry> Entries;
};
