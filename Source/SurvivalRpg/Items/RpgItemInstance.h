#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "RpgItemSourceHandle.h"
#include "SurvivalRpg/Systems/RpgGameplayTagStack.h"
#include "RpgItemInstance.generated.h"

class URpgItemDefinition;
class URpgItemFragment;

USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgItemFragmentRuntimeState
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	FGameplayTag FragmentTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	FGameplayTagContainer StateTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	TMap<FName, int32> IntValues;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	TMap<FName, float> FloatValues;
};

UCLASS(BlueprintType)
class SURVIVALRPG_API URpgItemInstance : public UObject
{
	GENERATED_BODY()

public:
	URpgItemInstance();

	virtual bool IsSupportedForNetworking() const override { return true; }
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category = "Item")
	void InitializeItemInstance(URpgItemDefinition* InItemDefinition, const FRpgItemSourceHandle& InSourceHandle, int32 InRollSeed = -1);

	UFUNCTION(BlueprintCallable, Category = "Item")
	URpgItemInstance* DuplicateItemInstance(UObject* NewOuter) const;

	UFUNCTION(BlueprintCallable, Category = "Item")
	void AddStatTagStack(FGameplayTag Tag, int32 StackCount);

	UFUNCTION(BlueprintCallable, Category = "Item")
	void RemoveStatTagStack(FGameplayTag Tag, int32 StackCount);

	UFUNCTION(BlueprintCallable, Category = "Item")
	void SetStatTagStackCount(FGameplayTag Tag, int32 NewCount);

	UFUNCTION(BlueprintCallable, Category = "Item")
	int32 GetStatTagStackCount(FGameplayTag Tag) const;

	UFUNCTION(BlueprintCallable, Category = "Item")
	bool HasStatTag(FGameplayTag Tag) const;

	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Item", meta = (DeterminesOutputType = "FragmentClass"))
	const URpgItemFragment* FindFragmentByClass(TSubclassOf<URpgItemFragment> FragmentClass) const;

	template <typename ResultClass>
	const ResultClass* FindFragmentByClass() const
	{
		return Cast<ResultClass>(FindFragmentByClass(ResultClass::StaticClass()));
	}

	const FGuid& GetInstanceId() const { return InstanceId; }
	URpgItemDefinition* GetItemDefinition() const { return ItemDefinition; }
	const FRpgItemSourceHandle& GetSourceHandle() const { return SourceHandle; }
	int32 GetRollSeed() const { return RollSeed; }
	const TArray<FRpgItemFragmentRuntimeState>& GetFragmentRuntimeStates() const { return FragmentRuntimeStates; }

private:
	void EnsureIdentity();

	UPROPERTY(Replicated)
	FGuid InstanceId;

	UPROPERTY(Replicated)
	TObjectPtr<URpgItemDefinition> ItemDefinition;

	UPROPERTY(Replicated)
	FRpgItemSourceHandle SourceHandle;

	UPROPERTY(Replicated)
	int32 RollSeed = INDEX_NONE;

	UPROPERTY(Replicated)
	FRpgGameplayTagStackContainer StatTagStacks;

	UPROPERTY(Replicated)
	TArray<FRpgItemFragmentRuntimeState> FragmentRuntimeStates;
};
