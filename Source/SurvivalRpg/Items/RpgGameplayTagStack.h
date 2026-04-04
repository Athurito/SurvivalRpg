#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "RpgGameplayTagStack.generated.h"

struct FNetDeltaSerializeInfo;
struct FRpgGameplayTagStackContainer;

USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgGameplayTagStack : public FFastArraySerializerItem
{
	GENERATED_BODY()

	FRpgGameplayTagStack() = default;

	FRpgGameplayTagStack(FGameplayTag InTag, int32 InStackCount)
		: Tag(InTag)
		, StackCount(InStackCount)
	{
	}

	FString GetDebugString() const;

private:
	friend FRpgGameplayTagStackContainer;

	UPROPERTY()
	FGameplayTag Tag;

	UPROPERTY()
	int32 StackCount = 0;
};

USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgGameplayTagStackContainer : public FFastArraySerializer
{
	GENERATED_BODY()

public:
	void AddStack(FGameplayTag Tag, int32 StackCount);
	void RemoveStack(FGameplayTag Tag, int32 StackCount);
	void SetStackCount(FGameplayTag Tag, int32 NewCount);
	int32 GetStackCount(FGameplayTag Tag) const;
	bool ContainsTag(FGameplayTag Tag) const;
	void RebuildTagToCountMap();

	void PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize);
	void PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize);
	void PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize);

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FastArrayDeltaSerialize<FRpgGameplayTagStack, FRpgGameplayTagStackContainer>(Stacks, DeltaParms, *this);
	}

private:
	UPROPERTY()
	TArray<FRpgGameplayTagStack> Stacks;

	TMap<FGameplayTag, int32> TagToCountMap;
};

template<>
struct TStructOpsTypeTraits<FRpgGameplayTagStackContainer> : public TStructOpsTypeTraitsBase2<FRpgGameplayTagStackContainer>
{
	enum
	{
		WithNetDeltaSerializer = true,
	};
};
