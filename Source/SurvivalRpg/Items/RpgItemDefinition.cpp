#include "RpgItemDefinition.h"

#include "Fragments/RpgItemFragment.h"

const URpgItemFragment* URpgItemDefinition::FindFragmentByClass(TSubclassOf<URpgItemFragment> FragmentClass) const
{
	if (FragmentClass == nullptr)
	{
		return nullptr;
	}

	for (const URpgItemFragment* Fragment : Fragments)
	{
		if (Fragment != nullptr && Fragment->IsA(FragmentClass))
		{
			return Fragment;
		}
	}

	return nullptr;
}

void URpgItemDefinition::AddFragment(URpgItemFragment* Fragment)
{
	if (Fragment != nullptr)
	{
		Fragments.Add(Fragment);
	}
}
