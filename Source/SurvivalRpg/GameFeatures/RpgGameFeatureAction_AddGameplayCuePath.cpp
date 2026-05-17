// Fill out your copyright notice in the Description page of Project Settings.

#include "RpgGameFeatureAction_AddGameplayCuePath.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "RpgGameFeatures"

URpgGameFeatureAction_AddGameplayCuePath::URpgGameFeatureAction_AddGameplayCuePath()
{
	DirectoryPathsToAdd.Add(FDirectoryPath{ TEXT("/GameplayCues") });
}

#if WITH_EDITOR
EDataValidationResult URpgGameFeatureAction_AddGameplayCuePath::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	for (const FDirectoryPath& Directory : DirectoryPathsToAdd)
	{
		if (Directory.Path.IsEmpty())
		{
			Context.AddError(FText::Format(LOCTEXT("InvalidCuePath", "'{0}' is not a valid cue path."), FText::FromString(Directory.Path)));
			Result = CombineDataValidationResults(Result, EDataValidationResult::Invalid);
		}
	}

	return CombineDataValidationResults(Result, EDataValidationResult::Valid);
}
#endif

#undef LOCTEXT_NAMESPACE
