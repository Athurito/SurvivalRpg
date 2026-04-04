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

	URpgItemInstance* DuplicatedItem = Cast<URpgItemInstance>(StaticDuplicateObject(this, NewOuter));
	if (DuplicatedItem != nullptr)
	{
		DuplicatedItem->StatTagStacks.RebuildTagToCountMap();
	}

	return DuplicatedItem;
}

void URpgItemInstance::AddStatTagStack(FGameplayTag Tag, int32 StackCount)
{
	StatTagStacks.AddStack(Tag, StackCount);
}

void URpgItemInstance::RemoveStatTagStack(FGameplayTag Tag, int32 StackCount)
{
	StatTagStacks.RemoveStack(Tag, StackCount);
}

void URpgItemInstance::SetStatTagStackCount(FGameplayTag Tag, int32 NewCount)
{
	StatTagStacks.SetStackCount(Tag, NewCount);
}

int32 URpgItemInstance::GetStatTagStackCount(FGameplayTag Tag) const
{
	return StatTagStacks.GetStackCount(Tag);
}

bool URpgItemInstance::HasStatTag(FGameplayTag Tag) const
{
	return StatTagStacks.ContainsTag(Tag);
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
