#include "RpgViewModelEntryBox.h"

#include "Blueprint/IUserListEntry.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "Blueprint/UserWidget.h"
#include "SurvivalRpg/UI/RpgMvvmListEntryWidgets.h"

#if WITH_EDITOR
#include "Editor/WidgetCompilerLog.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgViewModelEntryBox)

#define LOCTEXT_NAMESPACE "RpgViewModelEntryBox"

void URpgViewModelEntryBox::SetViewModelItems(const TArray<UObject*>& Items)
{
	// Drop entries whose item is gone; survivors keep their widget.
	for (int32 Index = EntryItems.Num() - 1; Index >= 0; --Index)
	{
		if (!Items.Contains(EntryItems[Index].Get()))
		{
			RemoveEntryAt(Index);
		}
	}

	// Survivors must lead Items in the same order; anything else rebuilds from scratch.
	bool bPrefixMatches = EntryItems.Num() <= Items.Num();
	for (int32 Index = 0; bPrefixMatches && Index < EntryItems.Num(); ++Index)
	{
		bPrefixMatches = EntryItems[Index].Get() == Items[Index];
	}
	if (!bPrefixMatches)
	{
		ClearViewModelItems();
	}

	for (int32 Index = EntryItems.Num(); Index < Items.Num(); ++Index)
	{
		AppendEntry(Items[Index]);
	}
}

void URpgViewModelEntryBox::ClearViewModelItems()
{
	for (int32 Index = EntryItems.Num() - 1; Index >= 0; --Index)
	{
		RemoveEntryAt(Index);
	}
	ResetInternal();
	EntryItems.Reset();
}

void URpgViewModelEntryBox::RemoveEntryAt(int32 Index)
{
	const TArray<UUserWidget*>& Entries = GetAllEntries();
	if (Entries.IsValidIndex(Index) && Entries[Index])
	{
		UUserWidget* Entry = Entries[Index];
		if (Entry->Implements<UUserListEntry>())
		{
			IUserListEntry::ReleaseEntry(*Entry);
		}
		RemoveEntryInternal(Entry);
	}
	if (EntryItems.IsValidIndex(Index))
	{
		EntryItems.RemoveAt(Index);
	}
}

void URpgViewModelEntryBox::AppendEntry(UObject* Item)
{
	UUserWidget* Entry = EntryWidgetClass ? CreateEntryInternal(EntryWidgetClass) : nullptr;
	if (!Entry)
	{
		return;
	}
	if (URpgMvvmListEntryWidget* MvvmEntry = Cast<URpgMvvmListEntryWidget>(Entry))
	{
		MvvmEntry->SetEntryItem(Item);
	}
	else if (URpgMvvmListEntryButton* MvvmButton = Cast<URpgMvvmListEntryButton>(Entry))
	{
		MvvmButton->SetEntryItem(Item);
	}
	else if (Entry->Implements<UUserObjectListEntry>())
	{
		IUserObjectListEntry::Execute_OnListItemObjectSet(Entry, Item);
	}
	EntryItems.Add(Item);
}

#if WITH_EDITOR

const FText URpgViewModelEntryBox::GetPaletteCategory()
{
	return LOCTEXT("PaletteCategory", "RPG HUD");
}

void URpgViewModelEntryBox::ValidateCompiledDefaults(IWidgetCompilerLog& CompileLog) const
{
	Super::ValidateCompiledDefaults(CompileLog);
	if (!EntryWidgetClass)
	{
		CompileLog.Error(FText::Format(
			LOCTEXT("MissingEntryClass", "{0} has no EntryWidgetClass."),
			FText::FromName(GetFName())));
	}
}

#endif

#undef LOCTEXT_NAMESPACE
