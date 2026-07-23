#include "RpgUIScreenRegistry.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"

namespace
{
	bool IsStrictTagDescendant(
		const FGameplayTag Tag,
		const FName RootTagName)
	{
		const FGameplayTag RootTag =
			FGameplayTag::RequestGameplayTag(
				RootTagName,
				/*ErrorIfNotFound=*/ false);
		return Tag.IsValid() &&
			RootTag.IsValid() &&
			Tag != RootTag &&
			Tag.MatchesTag(RootTag);
	}
}
#endif

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

#if WITH_EDITOR

#define LOCTEXT_NAMESPACE "RpgUIScreenRegistry"

EDataValidationResult URpgUIScreenRegistry::IsDataValid(
	FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(
		Super::IsDataValid(Context),
		EDataValidationResult::Valid);
	TMap<FGameplayTag, int32> FirstIndexByScreenTag;

	const auto AddEntryError =
		[&Context, &Result](const FText& Error)
		{
			Context.AddError(Error);
			Result = EDataValidationResult::Invalid;
		};

	for (int32 EntryIndex = 0;
		EntryIndex < Screens.Num();
		++EntryIndex)
	{
		const FRpgUIScreenRegistryEntry& Entry =
			Screens[EntryIndex];

		// Mirror the non-loading routing prerequisites enforced by
		// URpgUIScreenSubsystem::OpenScreen, then add editor-only namespace,
		// uniqueness, and concrete-class checks.
		if (!Entry.ScreenTag.IsValid())
		{
			AddEntryError(FText::Format(
				LOCTEXT(
					"MissingScreenTag",
					"Screens[{0}].ScreenTag is unset. Assign a unique semantic tag below UI.Screen."),
				FText::AsNumber(EntryIndex)));
		}
		else
		{
			if (!IsStrictTagDescendant(
				Entry.ScreenTag,
				TEXT("UI.Screen")))
			{
				AddEntryError(FText::Format(
					LOCTEXT(
						"InvalidScreenTagNamespace",
						"Screens[{0}].ScreenTag '{1}' must be a descendant of UI.Screen (for example UI.Screen.Inventory)."),
					FText::AsNumber(EntryIndex),
					FText::FromName(
						Entry.ScreenTag.GetTagName())));
			}

			if (const int32* FirstEntryIndex =
					FirstIndexByScreenTag.Find(
						Entry.ScreenTag))
			{
				AddEntryError(FText::Format(
					LOCTEXT(
						"DuplicateScreenTag",
						"Screens[{0}].ScreenTag '{1}' duplicates Screens[{2}].ScreenTag. Keep exactly one mapping for each semantic screen."),
					FText::AsNumber(EntryIndex),
					FText::FromName(
						Entry.ScreenTag.GetTagName()),
					FText::AsNumber(*FirstEntryIndex)));
			}
			else
			{
				FirstIndexByScreenTag.Add(
					Entry.ScreenTag,
					EntryIndex);
			}
		}

		if (!Entry.LayerTag.IsValid())
		{
			AddEntryError(FText::Format(
				LOCTEXT(
					"MissingLayerTag",
					"Screens[{0}].LayerTag is unset. Assign the UI.Layer tag of a CommonUI layer owned by the root layout."),
				FText::AsNumber(EntryIndex)));
		}
		else if (!IsStrictTagDescendant(
			Entry.LayerTag,
			TEXT("UI.Layer")))
		{
			AddEntryError(FText::Format(
				LOCTEXT(
					"InvalidLayerTagNamespace",
					"Screens[{0}].LayerTag '{1}' must be a descendant of UI.Layer and identify a CommonUI layer."),
				FText::AsNumber(EntryIndex),
				FText::FromName(
					Entry.LayerTag.GetTagName())));
		}

		if (Entry.WidgetClass.IsNull())
		{
			AddEntryError(FText::Format(
				LOCTEXT(
					"MissingWidgetClass",
					"Screens[{0}].WidgetClass is unset. Assign the authored CommonActivatableWidget class that CommonUI should push."),
				FText::AsNumber(EntryIndex)));
		}
		else
		{
			const UClass* LoadedWidgetClass =
				Entry.WidgetClass.LoadSynchronous();
			if (!LoadedWidgetClass)
			{
				AddEntryError(FText::Format(
					LOCTEXT(
						"UnresolvedWidgetClass",
						"Screens[{0}].WidgetClass '{1}' could not be loaded. Fix or replace the authored class reference."),
					FText::AsNumber(EntryIndex),
					FText::FromString(
						Entry.WidgetClass
							.ToSoftObjectPath()
							.ToString())));
			}
			else if (!LoadedWidgetClass->IsChildOf(
				UCommonActivatableWidget::StaticClass()))
			{
				AddEntryError(FText::Format(
					LOCTEXT(
						"InvalidWidgetBaseClass",
						"Screens[{0}].WidgetClass '{1}' is not a CommonActivatableWidget class."),
					FText::AsNumber(EntryIndex),
					FText::FromString(
						LoadedWidgetClass->GetPathName())));
			}
			else if (LoadedWidgetClass->HasAnyClassFlags(
				CLASS_Abstract))
			{
				AddEntryError(FText::Format(
					LOCTEXT(
						"AbstractWidgetClass",
						"Screens[{0}].WidgetClass '{1}' is abstract. Assign a concrete CommonActivatableWidget class."),
					FText::AsNumber(EntryIndex),
					FText::FromString(
						LoadedWidgetClass->GetPathName())));
			}
		}

		if (!Entry.bSingleInstance)
		{
			Context.AddWarning(FText::Format(
				LOCTEXT(
					"LegacyMultipleInstanceFlag",
					"Screens[{0}] has legacy bSingleInstance=false data. UI.Screen mappings are always single-instance per local player; resave or migrate this registry entry."),
				FText::AsNumber(EntryIndex)));
		}
	}

	return Result;
}

#undef LOCTEXT_NAMESPACE

#endif
