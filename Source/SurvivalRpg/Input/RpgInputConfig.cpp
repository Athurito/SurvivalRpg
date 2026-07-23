// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgInputConfig.h"

#include "InputAction.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace
{
FGameplayTag ResolveGameplayTag(FName TagName)
{
	return TagName.IsNone() ? FGameplayTag() : FGameplayTag::RequestGameplayTag(TagName);
}

#if WITH_EDITOR
bool IsStrictInputTagDescendant(const FGameplayTag Tag)
{
	const FGameplayTag InputTagRoot =
		FGameplayTag::RequestGameplayTag(
			TEXT("InputTag"),
			/*ErrorIfNotFound=*/ false);
	return Tag.IsValid() &&
		InputTagRoot.IsValid() &&
		Tag != InputTagRoot &&
		Tag.MatchesTag(InputTagRoot);
}

struct FInputMappingLocation
{
	FString PropertyPath;
	const UInputAction* InputAction = nullptr;
};
#endif
}

URpgInputConfig::URpgInputConfig(const FObjectInitializer& ObjectInitializer)
{
}

const UInputAction* URpgInputConfig::FindNativeInputActionForTag(const FGameplayTag& InputTag, bool bLogNotFound) const
{
	for (const FRpgInputAction& Action : NativeInputActions)
	{
		if (Action.InputAction && (Action.InputTag == InputTag))
		{
			return Action.InputAction;
		}
	}

	if (bLogNotFound)
	{
		UE_LOG(LogTemp, Error, TEXT("Can't find NativeInputAction for InputTag [%s] on InputConfig [%s]."), *InputTag.ToString(), *GetNameSafe(this));
	}

	return nullptr;
}

const UInputAction* URpgInputConfig::FindAbilityInputActionForTag(const FGameplayTag& InputTag, bool bLogNotFound) const
{
	for (const FRpgInputAction& Action : AbilityInputActions)
	{
		if (Action.InputAction && (Action.InputTag == InputTag))
		{
			return Action.InputAction;
		}
	}

	if (bLogNotFound)
	{
		UE_LOG(LogTemp, Error, TEXT("Can't find AbilityInputAction for InputTag [%s] on InputConfig [%s]."), *InputTag.ToString(), *GetNameSafe(this));
	}

	return nullptr;
}

void URpgInputConfig::ClearAbilityInputActions()
{
	AbilityInputActions.Reset();
}

void URpgInputConfig::AddAbilityInputActionByTagName(const UInputAction* InputAction, FName InputTagName)
{
	FRpgInputAction& NewAction = AbilityInputActions.AddDefaulted_GetRef();
	NewAction.InputAction = InputAction;
	NewAction.InputTag = ResolveGameplayTag(InputTagName);
}

#if WITH_EDITOR

#define LOCTEXT_NAMESPACE "RpgInputConfig"

EDataValidationResult URpgInputConfig::IsDataValid(
	FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(
		Super::IsDataValid(Context),
		EDataValidationResult::Valid);
	TMap<FGameplayTag, FInputMappingLocation> FirstMappingByTag;
	const FText ConfigPath = FText::FromString(GetPathName());

	const auto AddMappingError =
		[&Context, &Result](const FText& Error)
		{
			Context.AddError(Error);
			Result = EDataValidationResult::Invalid;
		};

	const auto ValidateMappings =
		[&AddMappingError, &ConfigPath, &FirstMappingByTag](
			const TArray<FRpgInputAction>& Mappings,
			const TCHAR* PropertyName)
		{
			for (int32 MappingIndex = 0;
				MappingIndex < Mappings.Num();
				++MappingIndex)
			{
				const FRpgInputAction& Mapping =
					Mappings[MappingIndex];
				const FString PropertyPath = FString::Printf(
					TEXT("%s[%d]"),
					PropertyName,
					MappingIndex);
				const FText PropertyPathText =
					FText::FromString(PropertyPath);

				if (!Mapping.InputAction)
				{
					AddMappingError(FText::Format(
						LOCTEXT(
							"NullInputAction",
							"Input config '{0}': {1}.InputAction is null. Assign an Enhanced Input action asset."),
						ConfigPath,
						PropertyPathText));
				}

				if (!Mapping.InputTag.IsValid())
				{
					AddMappingError(FText::Format(
						LOCTEXT(
							"MissingInputTag",
							"Input config '{0}': {1}.InputTag is unset. Assign a unique semantic tag below InputTag."),
						ConfigPath,
						PropertyPathText));
					continue;
				}

				if (!IsStrictInputTagDescendant(
					Mapping.InputTag))
				{
					AddMappingError(FText::Format(
						LOCTEXT(
							"InvalidInputTagNamespace",
							"Input config '{0}': {1}.InputTag '{2}' must be a strict descendant of InputTag; the InputTag root and other namespaces are not valid mappings."),
						ConfigPath,
						PropertyPathText,
						FText::FromName(
							Mapping.InputTag.GetTagName())));
				}

				if (const FInputMappingLocation* FirstMapping =
						FirstMappingByTag.Find(
							Mapping.InputTag))
				{
					const FText FirstPropertyPath =
						FText::FromString(
							FirstMapping->PropertyPath);
					if (Mapping.InputAction &&
						Mapping.InputAction ==
							FirstMapping->InputAction)
					{
						AddMappingError(FText::Format(
							LOCTEXT(
								"ExactDuplicateInputMapping",
								"Input config '{0}': {1} exactly duplicates {2} with InputAction '{3}' and InputTag '{4}'. Keep one mapping for this tag across NativeInputActions and AbilityInputActions."),
							ConfigPath,
							PropertyPathText,
							FirstPropertyPath,
							FText::FromString(
								Mapping.InputAction->
									GetPathName()),
							FText::FromName(
								Mapping.InputTag.
									GetTagName())));
					}
					else
					{
						AddMappingError(FText::Format(
							LOCTEXT(
								"DuplicateInputTag",
								"Input config '{0}': {1}.InputTag '{2}' duplicates {3}.InputTag. Each input tag may be mapped only once across NativeInputActions and AbilityInputActions."),
							ConfigPath,
							PropertyPathText,
							FText::FromName(
								Mapping.InputTag.
									GetTagName()),
							FirstPropertyPath));
					}
				}
				else
				{
					FirstMappingByTag.Add(
						Mapping.InputTag,
						{
							PropertyPath,
							Mapping.InputAction
						});
				}
			}
		};

	ValidateMappings(
		NativeInputActions,
		TEXT("NativeInputActions"));
	ValidateMappings(
		AbilityInputActions,
		TEXT("AbilityInputActions"));

	return Result;
}

#undef LOCTEXT_NAMESPACE

#endif
