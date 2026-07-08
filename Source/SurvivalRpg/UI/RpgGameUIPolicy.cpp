#include "RpgGameUIPolicy.h"

#include "RpgPrimaryGameLayout.h"

URpgGameUIPolicy::URpgGameUIPolicy()
{
	RootLayoutClass = URpgPrimaryGameLayout::StaticClass();
	ApplyRootLayoutClass();
}

void URpgGameUIPolicy::PostInitProperties()
{
	Super::PostInitProperties();
	ApplyRootLayoutClass();
}

void URpgGameUIPolicy::ApplyRootLayoutClass()
{
	if (!RootLayoutClass.IsNull())
	{
		SetLayoutWidgetClass(RootLayoutClass);
	}
}
