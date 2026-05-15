// Fill out your copyright notice in the Description page of Project Settings.

#include "RpgExperienceDefinition.h"

#include "GameFeatureAction.h"
#include "RpgExperienceActionSet.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

URpgExperienceDefinition::URpgExperienceDefinition()
{
}

#if WITH_EDITOR
EDataValidationResult URpgExperienceDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);

	int32 EntryIndex = 0;
	for (const UGameFeatureAction* Action : Actions)
	{
		if (Action)
		{
			Result = CombineDataValidationResults(Result, Action->IsDataValid(Context));
		}
		else
		{
			Result = EDataValidationResult::Invalid;
			Context.AddError(FText::Format(NSLOCTEXT("RpgSystem", "ActionEntryIsNull", "Null entry at index {0} in Actions"), FText::AsNumber(EntryIndex)));
		}

		++EntryIndex;
	}

	return Result;
}
#endif

#if WITH_EDITORONLY_DATA
void URpgExperienceDefinition::UpdateAssetBundleData()
{
	Super::UpdateAssetBundleData();

	for (UGameFeatureAction* Action : Actions)
	{
		if (Action)
		{
			Action->AddAdditionalAssetBundleData(AssetBundleData);
		}
	}
}
#endif
