#include "RpgInventoryViewModels.h"

#include "GameFramework/GameplayMessageSubsystem.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemTraits.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryViewModels)

void URpgInventoryFragmentViewModel::InitializeFromEntry(const FRpgInventoryEntryView& Entry)
{
	ItemInstance = Entry.Instance;
	EntryId = Entry.EntryId;
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ItemInstance);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(EntryId);
}

void URpgInventoryStackFragmentViewModel::InitializeFromEntry(const FRpgInventoryEntryView& Entry)
{
	Super::InitializeFromEntry(Entry);

	StackCount = Entry.StackCount;
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(StackCount);
}

void URpgInventoryTraitsFragmentViewModel::InitializeFromEntry(const FRpgInventoryEntryView& Entry)
{
	Super::InitializeFromEntry(Entry);

	const URpgInventoryFragment_ItemTraits* Traits = Entry.Instance ? Entry.Instance->FindFragmentByClass<URpgInventoryFragment_ItemTraits>() : nullptr;
	ItemCategory = Traits ? Traits->ItemCategory : ERpgInventoryItemCategory::Misc;
	ItemTags = Traits ? Traits->ItemTags : FGameplayTagContainer();
	bCanAssignToQuickBar = Traits ? Traits->bCanAssignToQuickBar : false;
	bIsMaterial = Traits ? Traits->IsMaterial() : false;

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ItemCategory);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ItemTags);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bCanAssignToQuickBar);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bIsMaterial);
}

void URpgInventoryUiDataFragmentViewModel::InitializeFromEntry(const FRpgInventoryEntryView& Entry)
{
	Super::InitializeFromEntry(Entry);

	const URpgInventoryFragment_UIData* UIData = Entry.Instance ? Entry.Instance->FindFragmentByClass<URpgInventoryFragment_UIData>() : nullptr;
	Icon = UIData ? UIData->Icon : TSoftObjectPtr<UTexture2D>();
	ShortDisplayName = UIData ? UIData->ShortDisplayName : FText::GetEmpty();
	Description = UIData ? UIData->Description : FText::GetEmpty();
	PresentationTags = UIData ? UIData->PresentationTags : FGameplayTagContainer();

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Icon);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ShortDisplayName);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Description);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(PresentationTags);
}

void URpgInventoryEntryViewModel::InitializeFromEntry(
	const FRpgInventoryEntryView& Entry,
	const TMap<TSubclassOf<URpgInventoryItemFragment>, TSubclassOf<URpgInventoryFragmentViewModel>>& FragmentViewModelClasses)
{
	InventoryOwner = Entry.InventoryOwner;
	ItemInstance = Entry.Instance;
	EntryId = Entry.EntryId;
	StackCount = Entry.StackCount;
	SortIndex = Entry.SortIndex;
	DisplayName = FText::GetEmpty();
	ShortDisplayName = FText::GetEmpty();
	Description = FText::GetEmpty();
	Icon.Reset();
	ItemCategory = ERpgInventoryItemCategory::Misc;
	ItemTags.Reset();
	PresentationTags.Reset();
	bCanDrag = ItemInstance != nullptr && StackCount > 0;
	bCanAssignToQuickBar = false;
	FragmentViewModels.Reset();

	if (ItemInstance)
	{
		if (const TSubclassOf<URpgInventoryItemDefinition> ItemDef = ItemInstance->GetItemDef())
		{
			if (const URpgInventoryItemDefinition* ItemCDO = GetDefault<URpgInventoryItemDefinition>(ItemDef))
			{
				DisplayName = ItemCDO->DisplayName;
			}
		}

		if (const URpgInventoryFragment_UIData* UIData = ItemInstance->FindFragmentByClass<URpgInventoryFragment_UIData>())
		{
			Icon = UIData->Icon;
			ShortDisplayName = UIData->ShortDisplayName.IsEmpty() ? DisplayName : UIData->ShortDisplayName;
			Description = UIData->Description;
			PresentationTags = UIData->PresentationTags;
		}
		else
		{
			ShortDisplayName = DisplayName;
		}

		if (const URpgInventoryFragment_ItemTraits* Traits = ItemInstance->FindFragmentByClass<URpgInventoryFragment_ItemTraits>())
		{
			ItemCategory = Traits->ItemCategory;
			ItemTags = Traits->ItemTags;
			bCanAssignToQuickBar = Traits->bCanAssignToQuickBar;
		}
	}

	auto AddFragmentViewModel = [this, &Entry](TSubclassOf<URpgInventoryFragmentViewModel> ViewModelClass)
	{
		if (!ViewModelClass)
		{
			return;
		}

		URpgInventoryFragmentViewModel* FragmentViewModel = NewObject<URpgInventoryFragmentViewModel>(this, ViewModelClass);
		if (FragmentViewModel)
		{
			FragmentViewModel->InitializeFromEntry(Entry);
			FragmentViewModels.Add(FragmentViewModel);
		}
	};

	AddFragmentViewModel(URpgInventoryStackFragmentViewModel::StaticClass());

	if (ItemInstance)
	{
		for (const TPair<TSubclassOf<URpgInventoryItemFragment>, TSubclassOf<URpgInventoryFragmentViewModel>>& Mapping : FragmentViewModelClasses)
		{
			if (Mapping.Key && Mapping.Value && ItemInstance->FindFragmentByClass(Mapping.Key) != nullptr)
			{
				AddFragmentViewModel(Mapping.Value);
			}
		}
	}

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(InventoryOwner);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ItemInstance);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(EntryId);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(StackCount);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SortIndex);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(DisplayName);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ShortDisplayName);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Description);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Icon);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ItemCategory);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ItemTags);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(PresentationTags);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bCanDrag);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bCanAssignToQuickBar);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(FragmentViewModels);
}

URpgInventoryPanelViewModel::URpgInventoryPanelViewModel()
{
	FragmentViewModelClasses.Add(URpgInventoryFragment_UIData::StaticClass(), URpgInventoryUiDataFragmentViewModel::StaticClass());
	FragmentViewModelClasses.Add(URpgInventoryFragment_ItemTraits::StaticClass(), URpgInventoryTraitsFragmentViewModel::StaticClass());
}

void URpgInventoryPanelViewModel::BindInventory(URpgInventoryManagerComponent* InInventory)
{
	if (ObservedInventory.Get() == InInventory)
	{
		RefreshEntries();
		return;
	}

	UnbindInventory();
	ObservedInventory = InInventory;
	RegisterInventoryMessageListener();
	RefreshEntries();
}

void URpgInventoryPanelViewModel::UnbindInventory()
{
	UnregisterInventoryMessageListener();
	ObservedInventory.Reset();
	Entries.Reset();
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Entries);
}

void URpgInventoryPanelViewModel::RefreshEntries()
{
	Entries.Reset();

	URpgInventoryManagerComponent* Inventory = ObservedInventory.Get();
	if (!Inventory)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Entries);
		return;
	}

	TArray<FRpgInventoryEntryView> EntryViews = Inventory->GetAllEntries();
	EntryViews.Sort([](const FRpgInventoryEntryView& A, const FRpgInventoryEntryView& B)
	{
		return A.SortIndex < B.SortIndex;
	});

	Entries.Reserve(EntryViews.Num());
	for (const FRpgInventoryEntryView& EntryView : EntryViews)
	{
		URpgInventoryEntryViewModel* EntryViewModel = NewObject<URpgInventoryEntryViewModel>(this);
		if (EntryViewModel)
		{
			EntryViewModel->InitializeFromEntry(EntryView, FragmentViewModelClasses);
			Entries.Add(EntryViewModel);
		}
	}

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(Entries);
}

void URpgInventoryPanelViewModel::BeginDestroy()
{
	UnbindInventory();

	Super::BeginDestroy();
}

void URpgInventoryPanelViewModel::RegisterInventoryMessageListener()
{
	UnregisterInventoryMessageListener();

	URpgInventoryManagerComponent* Inventory = ObservedInventory.Get();
	UWorld* World = Inventory ? Inventory->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(World);
	const FGameplayTag InventoryChangedTag = FGameplayTag::RequestGameplayTag(TEXT("Rpg.Inventory.Message.StackChanged"));
	InventoryChangedHandle = MessageSubsystem.RegisterListener<FRpgInventoryChangeMessage>(
		InventoryChangedTag,
		this,
		&ThisClass::HandleInventoryChanged);
}

void URpgInventoryPanelViewModel::UnregisterInventoryMessageListener()
{
	if (InventoryChangedHandle.IsValid())
	{
		InventoryChangedHandle.Unregister();
	}
}

void URpgInventoryPanelViewModel::HandleInventoryChanged(FGameplayTag Channel, const FRpgInventoryChangeMessage& Message)
{
	if (ObservedInventory.Get() == Message.InventoryOwner)
	{
		RefreshEntries();
	}
}
