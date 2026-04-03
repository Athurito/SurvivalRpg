#include "RpgItemInstance.h"

#include "Net/UnrealNetwork.h"
#include "Fragments/RpgItemFragment.h"
#include "RpgItemDefinition.h"

URpgItemInstance::URpgItemInstance()
{
}

void URpgItemInstance::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, InstanceId);
	DOREPLIFETIME(ThisClass, ItemDefinition);
	DOREPLIFETIME(ThisClass, SourceHandle);
	DOREPLIFETIME(ThisClass, RollSeed);
	DOREPLIFETIME(ThisClass, StatTagStacks);
	DOREPLIFETIME(ThisClass, FragmentRuntimeStates);
}

void URpgItemInstance::InitializeItemInstance(URpgItemDefinition* InItemDefinition, const FRpgItemSourceHandle& InSourceHandle, int32 InRollSeed)
{
	ItemDefinition = InItemDefinition;
	SourceHandle = InSourceHandle;
	RollSeed = (InRollSeed != INDEX_NONE) ? InRollSeed : FMath::Rand();
	EnsureIdentity();

	if (ItemDefinition == nullptr)
	{
		return;
	}

	for (const URpgItemFragment* Fragment : ItemDefinition->GetFragments())
	{
		if (Fragment != nullptr)
		{
			Fragment->OnInstanceCreated(this);
		}
	}
}

URpgItemInstance* URpgItemInstance::DuplicateItemInstance(UObject* NewOuter) const
{
	if (NewOuter == nullptr)
	{
		return nullptr;
	}

	return Cast<URpgItemInstance>(StaticDuplicateObject(this, NewOuter));
}

void URpgItemInstance::AddStatTagStack(FGameplayTag Tag, int32 StackCount)
{
	if (Tag.IsValid() && StackCount > 0)
	{
		SetStatTagStackCount(Tag, GetStatTagStackCount(Tag) + StackCount);
	}
}

void URpgItemInstance::RemoveStatTagStack(FGameplayTag Tag, int32 StackCount)
{
	if (Tag.IsValid() && StackCount > 0)
	{
		const int32 NewCount = FMath::Max(0, GetStatTagStackCount(Tag) - StackCount);
		SetStatTagStackCount(Tag, NewCount);
	}
}

void URpgItemInstance::SetStatTagStackCount(FGameplayTag Tag, int32 NewCount)
{
	if (Tag.IsValid())
	{
		const int32 ExistingIndex = StatTagStacks.IndexOfByPredicate([&Tag](const FRpgItemTagStackEntry& Entry)
		{
			return Entry.Tag == Tag;
		});

		if (NewCount > 0)
		{
			if (ExistingIndex != INDEX_NONE)
			{
				StatTagStacks[ExistingIndex].StackCount = NewCount;
			}
			else
			{
				FRpgItemTagStackEntry& NewEntry = StatTagStacks.AddDefaulted_GetRef();
				NewEntry.Tag = Tag;
				NewEntry.StackCount = NewCount;
			}
		}
		else if (ExistingIndex != INDEX_NONE)
		{
			StatTagStacks.RemoveAtSwap(ExistingIndex);
		}
	}
}

int32 URpgItemInstance::GetStatTagStackCount(FGameplayTag Tag) const
{
	if (const FRpgItemTagStackEntry* FoundEntry = StatTagStacks.FindByPredicate([&Tag](const FRpgItemTagStackEntry& Entry)
	{
		return Entry.Tag == Tag;
	}))
	{
		return FoundEntry->StackCount;
	}

	return 0;
}

bool URpgItemInstance::HasStatTag(FGameplayTag Tag) const
{
	return GetStatTagStackCount(Tag) > 0;
}

const URpgItemFragment* URpgItemInstance::FindFragmentByClass(TSubclassOf<URpgItemFragment> FragmentClass) const
{
	return (ItemDefinition != nullptr && FragmentClass != nullptr)
		? ItemDefinition->FindFragmentByClass(FragmentClass)
		: nullptr;
}

void URpgItemInstance::EnsureIdentity()
{
	if (!InstanceId.IsValid())
	{
		InstanceId = FGuid::NewGuid();
	}
}
