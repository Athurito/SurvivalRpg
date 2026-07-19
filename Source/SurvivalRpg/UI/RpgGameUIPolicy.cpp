#include "RpgGameUIPolicy.h"

void URpgGameUIPolicy::PostInitProperties()
{
	Super::PostInitProperties();
	ApplyRootLayoutClass();
}

void URpgGameUIPolicy::ApplyRootLayoutClass()
{
	if (RootLayoutClass.IsNull())
	{
		ensureAlwaysMsgf(
			false,
			TEXT("RpgGameUIPolicy requires a config-authored RootLayoutClass."));
		return;
	}

	SetLayoutWidgetClass(RootLayoutClass);
}
