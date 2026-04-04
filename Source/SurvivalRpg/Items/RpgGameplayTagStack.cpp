#include "RpgGameplayTagStack.h"

FString FRpgGameplayTagStack::GetDebugString() const
{
	return FString::Printf(TEXT("%sx%d"), *Tag.ToString(), StackCount);
}

void FRpgGameplayTagStackContainer::AddStack(FGameplayTag Tag, int32 StackCount)
{
	if (!Tag.IsValid() || StackCount <= 0)
	{
		return;
	}

	for (FRpgGameplayTagStack& Stack : Stacks)
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

	FRpgGameplayTagStack& NewStack = Stacks.Emplace_GetRef(Tag, StackCount);
	TagToCountMap.Add(Tag, StackCount);
	MarkItemDirty(NewStack);
}

void FRpgGameplayTagStackContainer::RemoveStack(FGameplayTag Tag, int32 StackCount)
{
	if (!Tag.IsValid() || StackCount <= 0)
	{
		return;
	}

	for (auto It = Stacks.CreateIterator(); It; ++It)
	{
		FRpgGameplayTagStack& Stack = *It;
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

void FRpgGameplayTagStackContainer::SetStackCount(FGameplayTag Tag, int32 NewCount)
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

int32 FRpgGameplayTagStackContainer::GetStackCount(FGameplayTag Tag) const
{
	if (const int32* FoundCount = TagToCountMap.Find(Tag))
	{
		return *FoundCount;
	}

	return 0;
}

bool FRpgGameplayTagStackContainer::ContainsTag(FGameplayTag Tag) const
{
	return GetStackCount(Tag) > 0;
}

void FRpgGameplayTagStackContainer::RebuildTagToCountMap()
{
	TagToCountMap.Reset();

	for (const FRpgGameplayTagStack& Stack : Stacks)
	{
		TagToCountMap.FindOrAdd(Stack.Tag) = Stack.StackCount;
	}
}

void FRpgGameplayTagStackContainer::PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize)
{
	for (const int32 RemovedIndex : RemovedIndices)
	{
		TagToCountMap.Remove(Stacks[RemovedIndex].Tag);
	}
}

void FRpgGameplayTagStackContainer::PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize)
{
	for (const int32 AddedIndex : AddedIndices)
	{
		const FRpgGameplayTagStack& Stack = Stacks[AddedIndex];
		TagToCountMap.FindOrAdd(Stack.Tag) = Stack.StackCount;
	}
}

void FRpgGameplayTagStackContainer::PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize)
{
	for (const int32 ChangedIndex : ChangedIndices)
	{
		const FRpgGameplayTagStack& Stack = Stacks[ChangedIndex];
		TagToCountMap.FindOrAdd(Stack.Tag) = Stack.StackCount;
	}
}
