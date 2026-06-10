#include "RpgUIScreenRegistry.h"

bool URpgUIScreenRegistry::FindScreen(FGameplayTag ScreenTag, FRpgUIScreenRegistryEntry& OutEntry) const
{
	if (!ScreenTag.IsValid())
	{
		return false;
	}

	for (const FRpgUIScreenRegistryEntry& Entry : Screens)
	{
		if (Entry.ScreenTag == ScreenTag)
		{
			OutEntry = Entry;
			return true;
		}
	}

	return false;
}
