#include "GameplayTagStack.h"

FString FGameplayTagStack::GetDebugString() const
{
	return FString::Printf(TEXT("%sx%d"), *Tag.ToString(), StackCount);
}

void FGameplayTagStackContainer::AddStack(FGameplayTag Tag, int32 StackCount)
{
	if (!Tag.IsValid() || StackCount <= 0)
	{
		return;
	}

	for (FGameplayTagStack& Stack : Stacks)
	{
		if (Stack.Tag == Tag)
		{
			const int32 NewCount = Stack.StackCount + StackCount;
			Stack.StackCount = NewCount;
			TagToCountMap.FindOrAdd(Tag) = NewCount;
			MarkItemDirty(Stack);
			return;
		}
	}

	FGameplayTagStack& NewStack = Stacks.Emplace_GetRef(Tag, StackCount);
	TagToCountMap.Add(Tag, StackCount);
	MarkItemDirty(NewStack);
}

void FGameplayTagStackContainer::RemoveStack(FGameplayTag Tag, int32 StackCount)
{
	if (!Tag.IsValid() || StackCount <= 0)
	{
		return;
	}

	for (auto It = Stacks.CreateIterator(); It; ++It)
	{
		FGameplayTagStack& Stack = *It;
		if (Stack.Tag != Tag)
		{
			continue;
		}

		if (Stack.StackCount <= StackCount)
		{
			It.RemoveCurrent();
			TagToCountMap.Remove(Tag);
			MarkArrayDirty();
		}
		else
		{
			const int32 NewCount = Stack.StackCount - StackCount;
			Stack.StackCount = NewCount;
			TagToCountMap.FindOrAdd(Tag) = NewCount;
			MarkItemDirty(Stack);
		}

		return;
	}
}

void FGameplayTagStackContainer::SetStackCount(FGameplayTag Tag, int32 NewCount)
{
	const int32 OldCount = GetStackCount(Tag);
	if (NewCount <= 0)
	{
		RemoveStack(Tag, OldCount);
		return;
	}

	const int32 Delta = NewCount - OldCount;
	if (Delta > 0)
	{
		AddStack(Tag, Delta);
	}
	else if (Delta < 0)
	{
		RemoveStack(Tag, -Delta);
	}
}

int32 FGameplayTagStackContainer::GetStackCount(FGameplayTag Tag) const
{
	if (const int32* FoundCount = TagToCountMap.Find(Tag))
	{
		return *FoundCount;
	}

	return 0;
}

bool FGameplayTagStackContainer::ContainsTag(FGameplayTag Tag) const
{
	return GetStackCount(Tag) > 0;
}

void FGameplayTagStackContainer::GetSemanticStacks(TArray<TPair<FGameplayTag, int32>>& OutStacks) const
{
	OutStacks.Reset(TagToCountMap.Num());
	for (const TPair<FGameplayTag, int32>& Pair : TagToCountMap)
	{
		if (Pair.Key.IsValid() && Pair.Value > 0)
		{
			OutStacks.Emplace(Pair.Key, Pair.Value);
		}
	}

	OutStacks.Sort([](const TPair<FGameplayTag, int32>& A, const TPair<FGameplayTag, int32>& B)
	{
		return A.Key.ToString() < B.Key.ToString();
	});
}

void FGameplayTagStackContainer::RebuildTagToCountMap()
{
	TagToCountMap.Reset();

	for (const FGameplayTagStack& Stack : Stacks)
	{
		TagToCountMap.FindOrAdd(Stack.Tag) = Stack.StackCount;
	}
}

void FGameplayTagStackContainer::PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize)
{
	for (const int32 RemovedIndex : RemovedIndices)
	{
		TagToCountMap.Remove(Stacks[RemovedIndex].Tag);
	}
}

void FGameplayTagStackContainer::PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize)
{
	for (const int32 AddedIndex : AddedIndices)
	{
		const FGameplayTagStack& Stack = Stacks[AddedIndex];
		TagToCountMap.FindOrAdd(Stack.Tag) = Stack.StackCount;
	}
}

void FGameplayTagStackContainer::PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize)
{
	for (const int32 ChangedIndex : ChangedIndices)
	{
		const FGameplayTagStack& Stack = Stacks[ChangedIndex];
		TagToCountMap.FindOrAdd(Stack.Tag) = Stack.StackCount;
	}
}
