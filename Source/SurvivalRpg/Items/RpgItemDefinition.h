#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "RpgItemDefinition.generated.h"

class URpgItemFragment;

UCLASS(BlueprintType, Const)
class SURVIVALRPG_API URpgItemDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("RpgItemDefinition"), GetFName());
	}

	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Item", meta = (DeterminesOutputType = "FragmentClass"))
	const URpgItemFragment* FindFragmentByClass(TSubclassOf<URpgItemFragment> FragmentClass) const;

	template <typename ResultClass>
	const ResultClass* FindFragmentByClass() const
	{
		return Cast<ResultClass>(FindFragmentByClass(ResultClass::StaticClass()));
	}

	const TArray<TObjectPtr<URpgItemFragment>>& GetFragments() const { return Fragments; }
	const FGameplayTag& GetItemTypeTag() const { return ItemTypeTag; }
	const FGameplayTagContainer& GetItemTags() const { return ItemTags; }
	void AddFragment(URpgItemFragment* Fragment);

private:
	UPROPERTY(EditDefaultsOnly, Instanced, Category = "Item")
	TArray<TObjectPtr<URpgItemFragment>> Fragments;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (AllowPrivateAccess = "true", Categories = "Item.Type"))
	FGameplayTag ItemTypeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (AllowPrivateAccess = "true"))
	FGameplayTagContainer ItemTags;
};
