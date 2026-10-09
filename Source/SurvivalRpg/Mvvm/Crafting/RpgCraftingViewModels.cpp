#include "RpgCraftingViewModels.h"

#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "SurvivalRpg/Base/RpgStorageAccessRules.h"
#include "SurvivalRpg/Base/RpgBaseBuildableDefinition.h"
#include "SurvivalRpg/Base/RpgWorldStorageKnowledgeComponent.h"
#include "SurvivalRpg/Core/Game/RpgGameStateBase.h"
#include "SurvivalRpg/Crafting/RpgCraftingCapacity.h"
#include "SurvivalRpg/Crafting/RpgCraftingCategoryCatalog.h"
#include "SurvivalRpg/Crafting/RpgCraftingRecipeDefinition.h"
#include "SurvivalRpg/Crafting/RpgRecipeUnlockComponent.h"
#include "SurvivalRpg/Equipment/RpgEquipmentDefinition.h"
#include "SurvivalRpg/Inventory/Itemization/RpgInventoryFragment_Itemization.h"
#include "SurvivalRpg/Inventory/Itemization/RpgItemizationProfile.h"
#include "SurvivalRpg/Inventory/RpgInventoryContainerComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_EquippableItem.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemTraits.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "Templates/Identity.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgCraftingViewModels)

namespace
{
	constexpr ETextIdenticalModeFlags CraftingTextIdentityFlags =
		ETextIdenticalModeFlags::DeepCompare |
		ETextIdenticalModeFlags::LexicalCompareInvariants;

	namespace CraftingRefreshDomains
	{
		constexpr uint8 Station = 1 << 0;
		constexpr uint8 RecipesAndDetails = 1 << 1;
		constexpr uint8 Order = 1 << 2;
		constexpr uint8 All = Station | RecipesAndDetails | Order;
	}

	/** Seconds between re-resolving the connected chests while a screen polls order progress. */
	constexpr double ConnectedStorageRefreshInterval = 1.0;

	using FFieldChanges = TArray<UE::FieldNotification::FFieldId, TInlineAllocator<32>>;

	template <typename T>
	bool AssignIfChanged(T& Field, const TIdentity_T<T>& NewValue)
	{
		if (Field == NewValue)
		{
			return false;
		}
		Field = NewValue;
		return true;
	}

	bool AssignIfChanged(FText& Field, const FText& NewValue)
	{
		if (Field.IdenticalTo(NewValue, CraftingTextIdentityFlags))
		{
			return false;
		}
		Field = NewValue;
		return true;
	}

	const URpgInventoryItemDefinition* GetItemDefinitionCDO(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition)
	{
		return ItemDefinition ? GetDefault<URpgInventoryItemDefinition>(ItemDefinition) : nullptr;
	}

	FText GetItemDisplayName(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition)
	{
		const URpgInventoryItemDefinition* ItemCDO = GetItemDefinitionCDO(ItemDefinition);
		return ItemCDO ? ItemCDO->DisplayName : FText::GetEmpty();
	}

	TSoftObjectPtr<UTexture2D> GetItemIcon(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition)
	{
		const URpgInventoryItemDefinition* ItemCDO = GetItemDefinitionCDO(ItemDefinition);
		const URpgInventoryFragment_UIData* UIData = ItemCDO ? Cast<URpgInventoryFragment_UIData>(ItemCDO->FindFragmentByClass(URpgInventoryFragment_UIData::StaticClass())) : nullptr;
		return UIData ? UIData->Icon : TSoftObjectPtr<UTexture2D>();
	}

	TSoftObjectPtr<UTexture2D> GetRecipeIcon(const URpgCraftingRecipeDefinition* Recipe)
	{
		if (!Recipe)
		{
			return TSoftObjectPtr<UTexture2D>();
		}
		if (!Recipe->Icon.IsNull())
		{
			return Recipe->Icon;
		}
		return Recipe->OutputItems.Num() > 0 ? GetItemIcon(Recipe->OutputItems[0].ItemDefinition) : TSoftObjectPtr<UTexture2D>();
	}

	const URpgItemizationProfile* GetItemizationProfile(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition)
	{
		const URpgInventoryItemDefinition* ItemCDO = GetItemDefinitionCDO(ItemDefinition);
		const URpgInventoryFragment_Itemization* Fragment = ItemCDO
			? Cast<URpgInventoryFragment_Itemization>(ItemCDO->FindFragmentByClass(URpgInventoryFragment_Itemization::StaticClass()))
			: nullptr;
		const URpgItemizationProfile* Profile = Fragment ? Fragment->ItemizationProfile.Get() : nullptr;
		return Profile && Profile->HasValidConfiguration() ? Profile : nullptr;
	}

	FRpgInventoryGridSize GetItemFootprint(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition)
	{
		const URpgInventoryFragment_SpatialItem* Spatial = URpgInventoryItemDefinition::ResolveValidSpatialItemFragment(ItemDefinition);
		return Spatial ? Spatial->Footprint : FRpgInventoryGridSize();
	}

	float GetItemEquipLoadWeight(TSubclassOf<URpgInventoryItemDefinition> ItemDefinition)
	{
		const URpgInventoryItemDefinition* ItemCDO = GetItemDefinitionCDO(ItemDefinition);
		const URpgInventoryFragment_EquippableItem* Equippable = ItemCDO
			? Cast<URpgInventoryFragment_EquippableItem>(ItemCDO->FindFragmentByClass(URpgInventoryFragment_EquippableItem::StaticClass()))
			: nullptr;
		const TSubclassOf<URpgEquipmentDefinition> EquipmentDefinition = Equippable ? Equippable->GetEquipmentDefinition() : nullptr;
		return EquipmentDefinition ? GetDefault<URpgEquipmentDefinition>(EquipmentDefinition)->EquipLoadWeight : 0.0f;
	}

	FString NormalizeSearchString(const FText& Text)
	{
		return Text.ToString().ToLower();
	}

	FString GetRecipeSortName(const URpgCraftingRecipeDefinition* Recipe)
	{
		return Recipe ? Recipe->DisplayName.ToString() : FString();
	}

	float GetServerWorldTimeSeconds(const UObject* WorldContextObject)
	{
		const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
		const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
		return GameState ? GameState->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0f);
	}

	FText MakePauseResumeButtonText(bool bIsPaused)
	{
		return bIsPaused
			? NSLOCTEXT("RpgCrafting", "ResumeCraftingStationButton", "Resume")
			: NSLOCTEXT("RpgCrafting", "PauseCraftingStationButton", "Pause");
	}

	ERpgCraftingRecipeState MakeRecipeState(bool bInIsUnlocked, bool bInCanCraftOne)
	{
		if (!bInIsUnlocked)
		{
			return ERpgCraftingRecipeState::Locked;
		}
		return bInCanCraftOne
			? ERpgCraftingRecipeState::Craftable
			: ERpgCraftingRecipeState::MissingResources;
	}

	/** Duration for plans and remaining time: "12 s", "1 min 05 s", "1 h 02 min". */
	FText MakeDurationText(float Seconds)
	{
		// The small bias keeps float noise such as 12.0001 from showing as 13 s.
		const int32 TotalSeconds = FMath::CeilToInt(FMath::Max(0.0f, Seconds) - 0.001f);
		FNumberFormattingOptions TwoDigits;
		TwoDigits.SetMinimumIntegralDigits(2).SetUseGrouping(false);
		if (TotalSeconds < 60)
		{
			return FText::Format(NSLOCTEXT("RpgCrafting", "DurationSeconds", "{0} s"), FText::AsNumber(TotalSeconds));
		}
		if (TotalSeconds < 3600)
		{
			return FText::Format(
				NSLOCTEXT("RpgCrafting", "DurationMinutes", "{0} min {1} s"),
				FText::AsNumber(TotalSeconds / 60),
				FText::AsNumber(TotalSeconds % 60, &TwoDigits));
		}
		return FText::Format(
			NSLOCTEXT("RpgCrafting", "DurationHours", "{0} h {1} min"),
			FText::AsNumber(TotalSeconds / 3600, &FNumberFormattingOptions::DefaultNoGrouping()),
			FText::AsNumber((TotalSeconds / 60) % 60, &TwoDigits));
	}

	FText MakeStationDisplayName(const URpgCraftingStationComponent* Station)
	{
		if (!Station)
		{
			return FText::GetEmpty();
		}
		const FText AuthoredName = Station->GetStationDisplayName();
		return AuthoredName.IsEmpty()
			? NSLOCTEXT("RpgCrafting", "DefaultStationDisplayName", "Crafting Station")
			: AuthoredName;
	}

	FText MakeUnitNoun(const URpgCraftingStationComponent* Station, int32 Count)
	{
		const FRpgCraftingStationPresentation Presentation = Station ? Station->GetStationPresentation() : FRpgCraftingStationPresentation();
		if (Count == 1)
		{
			return Presentation.UnitSingular.IsEmpty() ? NSLOCTEXT("RpgCrafting", "DefaultUnitSingular", "piece") : Presentation.UnitSingular;
		}
		return Presentation.UnitPlural.IsEmpty() ? NSLOCTEXT("RpgCrafting", "DefaultUnitPlural", "pieces") : Presentation.UnitPlural;
	}

	FText MakeYieldText(int32 Count)
	{
		return Count > 0
			? FText::Format(NSLOCTEXT("RpgCrafting", "RecipeYieldFormat", "×{0}"), FText::AsNumber(Count))
			: FText::GetEmpty();
	}

	FText MakeCountedName(int32 Count, const FText& Name)
	{
		return FText::Format(NSLOCTEXT("RpgCrafting", "CountedName", "{0} {1}"), FText::AsNumber(Count), Name);
	}

	FText MakeStatValueText(float Value)
	{
		FNumberFormattingOptions Options;
		Options.SetMaximumFractionalDigits(1).SetUseGrouping(false);
		return FText::AsNumber(Value, &Options);
	}

	FText MakeCellsText(const FRpgInventoryGridSize& Size)
	{
		return FText::Format(NSLOCTEXT("RpgCrafting", "CellSize", "{0} × {1}"), FText::AsNumber(Size.Width), FText::AsNumber(Size.Height));
	}

	FText JoinTexts(const TArray<FText>& Texts, const FText& Separator)
	{
		FString Joined;
		for (int32 Index = 0; Index < Texts.Num(); ++Index)
		{
			if (Index > 0)
			{
				Joined += Separator.ToString();
			}
			Joined += Texts[Index].ToString();
		}
		return FText::FromString(Joined);
	}

	/** Levels of a category below Crafting.Category: 1 for a group, 2 for a subcategory. */
	int32 GetCategoryDepth(const FGameplayTag& Category)
	{
		const FGameplayTag Root = URpgCraftingCategoryCatalog::GetCategoryRoot();
		if (!Root.IsValid() || !Category.IsValid() || Category == Root || !Category.MatchesTag(Root))
		{
			return 0;
		}
		int32 Depth = 0;
		for (FGameplayTag Tag = Category; Tag.IsValid() && Tag != Root; Tag = Tag.RequestDirectParent())
		{
			++Depth;
		}
		return Depth;
	}

	/** Subcategory of a recipe category: its ancestor two levels below the root, or invalid for a group-level tag. */
	FGameplayTag ResolveSubcategory(const FGameplayTag& Category)
	{
		FGameplayTag Tag = Category;
		int32 Depth = GetCategoryDepth(Tag);
		while (Depth > 2)
		{
			Tag = Tag.RequestDirectParent();
			--Depth;
		}
		return Depth == 2 ? Tag : FGameplayTag();
	}

	FText GetCategoryName(const URpgCraftingCategoryCatalog* Catalog, const FGameplayTag& Category)
	{
		const FRpgCraftingCategoryDisplay* Row = Catalog ? Catalog->FindCategory(Category) : nullptr;
		return Row && !Row->DisplayName.IsEmpty() ? Row->DisplayName : URpgCraftingCategoryCatalog::MakeFallbackCategoryName(Category);
	}

	TSoftObjectPtr<UTexture2D> GetCategoryIcon(const URpgCraftingCategoryCatalog* Catalog, const FGameplayTag& Category)
	{
		const FRpgCraftingCategoryDisplay* Row = Catalog ? Catalog->FindCategory(Category) : nullptr;
		return Row ? Row->Icon : TSoftObjectPtr<UTexture2D>();
	}

	int32 GetCategorySortOrder(const URpgCraftingCategoryCatalog* Catalog, const FGameplayTag& Category)
	{
		const FRpgCraftingCategoryDisplay* Row = Catalog ? Catalog->FindCategory(Category) : nullptr;
		return Row ? Row->SortOrder : 0;
	}

	FText MakeTierLabel(int32 Tier)
	{
		return FText::Format(NSLOCTEXT("RpgCrafting", "TierLabel", "Tier {0}"), URpgCraftingCategoryCatalog::MakeTierNumeral(Tier));
	}

	FText MakeTierTitle(const URpgCraftingCategoryCatalog* Catalog, int32 Tier)
	{
		const FRpgCraftingTierDisplay* Row = Catalog ? Catalog->FindTier(Tier) : nullptr;
		return Row && !Row->DisplayName.IsEmpty()
			? FText::Format(NSLOCTEXT("RpgCrafting", "NamedTierTitle", "{0} · {1}"), URpgCraftingCategoryCatalog::MakeTierNumeral(Tier), Row->DisplayName)
			: MakeTierLabel(Tier);
	}

	const URpgInventoryContainerComponent* FindStorageContainer(const URpgInventoryManagerComponent* Inventory)
	{
		const AActor* Owner = Inventory ? Inventory->GetOwner() : nullptr;
		const URpgInventoryContainerComponent* Container = Owner ? Owner->FindComponentByClass<URpgInventoryContainerComponent>() : nullptr;
		return Container && Container->GetInventoryManager() == Inventory ? Container : nullptr;
	}

	/** Names of a chest's assignments: exact items by their name, categories by the tag's last segment. */
	TArray<FText> MakeAssignmentNames(const URpgInventoryContainerComponent* Container)
	{
		TArray<FText> Names;
		TSet<FString> Seen;
		for (const FRpgStorageAssignment& Rule : Container ? Container->GetAssignments() : TArray<FRpgStorageAssignment>())
		{
			FText Name;
			if (Rule.ItemDefinition)
			{
				Name = GetItemDisplayName(Rule.ItemDefinition);
			}
			else if (Rule.Category.IsValid())
			{
				FString Leaf = Rule.Category.GetTagName().ToString();
				int32 Dot = INDEX_NONE;
				if (Leaf.FindLastChar(TEXT('.'), Dot)) { Leaf.RightChopInline(Dot + 1); }
				Name = FText::FromString(Leaf);
			}
			if (!Name.IsEmpty() && !Seen.Contains(Name.ToString()))
			{
				Seen.Add(Name.ToString());
				Names.Add(Name);
			}
		}
		return Names;
	}

	/** "80 Wood · 12 Ore" for the two largest stocks of a chest, with "+N more" kinds; "Empty" for an empty chest. */
	FText MakeStorageContentsText(const URpgInventoryManagerComponent* Inventory)
	{
		TMap<UClass*, int32> Counts;
		for (const URpgInventoryItemInstance* Item : Inventory ? Inventory->GetAllItems() : TArray<URpgInventoryItemInstance*>())
		{
			UClass* Definition = Item ? Item->GetItemDef().Get() : nullptr;
			if (Definition && !Counts.Contains(Definition))
			{
				Counts.Add(Definition, Inventory->GetTotalItemCountByDefinition(Definition));
			}
		}
		Counts.ValueSort(TGreater<int32>());
		TArray<FText> Parts;
		for (const TPair<UClass*, int32>& Pair : Counts)
		{
			if (Parts.Num() == 2) { break; }
			Parts.Add(MakeCountedName(Pair.Value, GetItemDisplayName(Pair.Key)));
		}
		if (Parts.IsEmpty())
		{
			return NSLOCTEXT("RpgCrafting", "StorageEmpty", "Empty");
		}
		FText Text = JoinTexts(Parts, NSLOCTEXT("RpgCrafting", "NameSeparator", " · "));
		return Counts.Num() > 2
			? FText::Format(NSLOCTEXT("RpgCrafting", "StorageContentsMore", "{0} · +{1} more"), Text, FText::AsNumber(Counts.Num() - 2))
			: Text;
	}

	/** Chest names for display: the buildable's name plus the chest's assignments, numbered when names repeat. */
	TArray<FText> MakeStorageNames(const TArray<URpgInventoryManagerComponent*>& Inventories)
	{
		TArray<FText> BaseNames;
		TMap<FString, int32> NameCounts;
		for (const URpgInventoryManagerComponent* Inventory : Inventories)
		{
			const URpgInventoryContainerComponent* Container = FindStorageContainer(Inventory);
			const URpgBaseBuildableDefinition* Buildable = Container ? Container->GetBuildableDefinition() : nullptr;
			FText Name = Buildable && !Buildable->DisplayName.IsEmpty() ? Buildable->DisplayName : NSLOCTEXT("RpgCrafting", "DefaultStorageName", "Storage Chest");
			const TArray<FText> Assignments = MakeAssignmentNames(Container);
			if (!Assignments.IsEmpty())
			{
				Name = FText::Format(NSLOCTEXT("RpgCrafting", "AssignedStorageName", "{0} ({1})"), Name,
					JoinTexts(Assignments, NSLOCTEXT("RpgCrafting", "ListSeparator", ", ")));
			}
			++NameCounts.FindOrAdd(Name.ToString());
			BaseNames.Add(MoveTemp(Name));
		}
		TArray<FText> Names;
		TMap<FString, int32> Numbering;
		for (const FText& BaseName : BaseNames)
		{
			if (NameCounts[BaseName.ToString()] <= 1)
			{
				Names.Add(BaseName);
				continue;
			}
			const int32 Number = ++Numbering.FindOrAdd(BaseName.ToString());
			Names.Add(FText::Format(NSLOCTEXT("RpgCrafting", "NumberedStorageName", "{0} {1}"), BaseName, FText::AsNumber(Number)));
		}
		return Names;
	}

	template <typename ViewModelType>
	ViewModelType* ReuseOrCreate(UObject* Outer, TArray<TObjectPtr<ViewModelType>>& Pool, int32 Index)
	{
		return Pool.IsValidIndex(Index) && Pool[Index] ? Pool[Index].Get() : NewObject<ViewModelType>(Outer);
	}
}

#define RPG_CRAFTING_SET(Changes, Member, NewValue) \
	if (AssignIfChanged(Member, NewValue)) { Changes.Add(ThisClass::FFieldNotificationClassDescriptor::Member); }

static void BroadcastChanges(UMVVMViewModelBase& ViewModel, const FFieldChanges& Changes)
{
	for (const UE::FieldNotification::FFieldId& FieldId : Changes)
	{
		ViewModel.BroadcastFieldValueChanged(FieldId);
	}
}

void URpgCraftingIngredientViewModel::InitializeIngredient(TSubclassOf<URpgInventoryItemDefinition> InItemDefinition, int32 InPerUnitCount, int32 InQuantity, int32 InAvailableCount)
{
	const int32 NewPerUnitCount = FMath::Max(0, InPerUnitCount);
	const int32 NewRequiredCount = static_cast<int32>(FMath::Min<int64>(static_cast<int64>(NewPerUnitCount) * FMath::Max(1, InQuantity), MAX_int32));
	const int32 NewAvailableCount = FMath::Max(0, InAvailableCount);
	FFieldChanges Changes;
	RPG_CRAFTING_SET(Changes, ItemDefinition, InItemDefinition);
	RPG_CRAFTING_SET(Changes, DisplayName, GetItemDisplayName(InItemDefinition));
	RPG_CRAFTING_SET(Changes, Icon, GetItemIcon(InItemDefinition));
	RPG_CRAFTING_SET(Changes, PerUnitCount, NewPerUnitCount);
	RPG_CRAFTING_SET(Changes, RequiredCount, NewRequiredCount);
	RPG_CRAFTING_SET(Changes, AvailableCount, NewAvailableCount);
	RPG_CRAFTING_SET(Changes, MissingCount, FMath::Max(0, NewRequiredCount - NewAvailableCount));
	RPG_CRAFTING_SET(Changes, bHasEnough, NewAvailableCount >= NewRequiredCount);
	RPG_CRAFTING_SET(Changes, bHasEnoughForOneUnit, NewAvailableCount >= NewPerUnitCount);
	BroadcastChanges(*this, Changes);
}

void URpgCraftingOutputViewModel::InitializeOutput(TSubclassOf<URpgInventoryItemDefinition> InItemDefinition, int32 InOutputCount)
{
	FFieldChanges Changes;
	RPG_CRAFTING_SET(Changes, ItemDefinition, InItemDefinition);
	RPG_CRAFTING_SET(Changes, DisplayName, GetItemDisplayName(InItemDefinition));
	RPG_CRAFTING_SET(Changes, Icon, GetItemIcon(InItemDefinition));
	RPG_CRAFTING_SET(Changes, OutputCount, FMath::Max(0, InOutputCount));
	BroadcastChanges(*this, Changes);
}

void URpgCraftingRecipeViewModel::InitializeRecipe(URpgCraftingStationComponent* InStation, URpgCraftingRecipeDefinition* InRecipe, int32 InAffordableUnitCount)
{
	const bool bNewIsUnlocked = InStation && InRecipe && InStation->IsRecipeUnlocked(InRecipe);
	const int32 NewAffordableUnitCount = InRecipe ? FMath::Max(0, InAffordableUnitCount) : 0;
	const bool bNewCanCraftOne = bNewIsUnlocked && NewAffordableUnitCount > 0;

	FText NewOutputSummary;
	FText NewYieldText;
	FText NewDetailText;
	if (InRecipe && InRecipe->OutputItems.Num() == 1)
	{
		const FRpgCraftingOutputItem& OutputItem = InRecipe->OutputItems[0];
		NewOutputSummary = FText::Format(
			NSLOCTEXT("RpgCrafting", "SingleOutputSummary", "{0}x {1}"),
			FText::AsNumber(OutputItem.Count),
			GetItemDisplayName(OutputItem.ItemDefinition));
		NewYieldText = MakeYieldText(OutputItem.Count);
	}
	else if (InRecipe && InRecipe->OutputItems.Num() > 1)
	{
		NewOutputSummary = FText::Format(
			NSLOCTEXT("RpgCrafting", "MultiOutputSummary", "{0} Outputs"),
			FText::AsNumber(InRecipe->OutputItems.Num()));
	}
	if (InRecipe && !bNewIsUnlocked)
	{
		NewDetailText = NSLOCTEXT("RpgCrafting", "RecipeLocked", "Locked");
	}
	else if (InRecipe && !InRecipe->OutputItems.IsEmpty())
	{
		const FRpgCraftingOutputItem& FirstOutput = InRecipe->OutputItems[0];
		const bool bSingleItem = URpgInventoryManagerComponent::GetEffectiveMaxStackSizeForDefinition(FirstOutput.ItemDefinition) <= 1;
		const FRpgInventoryGridSize Footprint = GetItemFootprint(FirstOutput.ItemDefinition);
		NewDetailText = bSingleItem && Footprint.IsValid()
			? FText::Format(NSLOCTEXT("RpgCrafting", "RecipeDetailCells", "{0} cells"), MakeCellsText(Footprint))
			: FText::Format(
				NSLOCTEXT("RpgCrafting", "RecipeDetailYield", "{0} per {1} · {2}"),
				FText::AsNumber(FirstOutput.Count),
				MakeUnitNoun(InStation, 1),
				MakeDurationText(InRecipe->CraftTime));
	}

	FString NewSearchString;
	if (InRecipe)
	{
		NewSearchString += NormalizeSearchString(InRecipe->DisplayName);
		NewSearchString += TEXT(" ");
		NewSearchString += NormalizeSearchString(InRecipe->Description);
		NewSearchString += TEXT(" ");
		NewSearchString += NewOutputSummary.ToString().ToLower();
		for (const FText& Keyword : InRecipe->SearchKeywords)
		{
			NewSearchString += TEXT(" ");
			NewSearchString += NormalizeSearchString(Keyword);
		}
		for (const FRpgCraftingOutputItem& OutputItem : InRecipe->OutputItems)
		{
			NewSearchString += TEXT(" ");
			NewSearchString += GetItemDisplayName(OutputItem.ItemDefinition).ToString().ToLower();
		}
	}

	FFieldChanges Changes;
	RPG_CRAFTING_SET(Changes, RecipeDefinition, InRecipe);
	RPG_CRAFTING_SET(Changes, DisplayName, InRecipe ? InRecipe->DisplayName : FText::GetEmpty());
	RPG_CRAFTING_SET(Changes, Description, InRecipe ? InRecipe->Description : FText::GetEmpty());
	RPG_CRAFTING_SET(Changes, Icon, GetRecipeIcon(InRecipe));
	RPG_CRAFTING_SET(Changes, RecipeCategory, InRecipe ? InRecipe->RecipeCategory : FGameplayTag());
	RPG_CRAFTING_SET(Changes, RecipeTier, InRecipe ? InRecipe->RecipeTier : 1);
	RPG_CRAFTING_SET(Changes, CraftTime, InRecipe ? InRecipe->CraftTime : 0.0f);
	RPG_CRAFTING_SET(Changes, SortPriority, InRecipe ? InRecipe->SortPriority : 0);
	RPG_CRAFTING_SET(Changes, AffordableUnitCount, NewAffordableUnitCount);
	RPG_CRAFTING_SET(Changes, bIsUnlocked, bNewIsUnlocked);
	RPG_CRAFTING_SET(Changes, bCanCraftOne, bNewCanCraftOne);
	RPG_CRAFTING_SET(Changes, bHasMissingResources, bNewIsUnlocked && !bNewCanCraftOne);
	RPG_CRAFTING_SET(Changes, RecipeState, MakeRecipeState(bNewIsUnlocked, bNewCanCraftOne));
	RPG_CRAFTING_SET(Changes, OutputSummary, NewOutputSummary);
	RPG_CRAFTING_SET(Changes, YieldText, NewYieldText);
	RPG_CRAFTING_SET(Changes, DetailText, NewDetailText);
	SearchString = MoveTemp(NewSearchString);
	BroadcastChanges(*this, Changes);
}

bool URpgCraftingRecipeViewModel::MatchesSearchText(const FText& InSearchText) const
{
	const FString Query = NormalizeSearchString(InSearchText).TrimStartAndEnd();
	return Query.IsEmpty() || SearchString.Contains(Query);
}

void URpgCraftingTierSectionViewModel::InitializeSection(int32 InTier, const FText& InTitleText, int32 InRecipeCount)
{
	FFieldChanges Changes;
	RPG_CRAFTING_SET(Changes, Tier, InTier);
	RPG_CRAFTING_SET(Changes, TitleText, InTitleText);
	RPG_CRAFTING_SET(Changes, RecipeCount, InRecipeCount);
	BroadcastChanges(*this, Changes);
}

void URpgCraftingCategoryViewModel::InitializeCategory(const FGameplayTag& InCategoryTag, ERpgCraftingCategoryRowKind InKind,
	const FText& InDisplayName, const TSoftObjectPtr<UTexture2D>& InIcon, int32 InRecipeCount, bool bInExpanded, bool bInSelected)
{
	FFieldChanges Changes;
	RPG_CRAFTING_SET(Changes, CategoryTag, InCategoryTag);
	RPG_CRAFTING_SET(Changes, Kind, InKind);
	RPG_CRAFTING_SET(Changes, DisplayName, InDisplayName);
	RPG_CRAFTING_SET(Changes, Icon, InIcon);
	RPG_CRAFTING_SET(Changes, RecipeCount, InRecipeCount);
	RPG_CRAFTING_SET(Changes, bExpanded, bInExpanded);
	RPG_CRAFTING_SET(Changes, bSelected, bInSelected);
	BroadcastChanges(*this, Changes);
}

void URpgCraftingTierOptionViewModel::InitializeOption(int32 InTier, const FText& InLabel, bool bInSelected)
{
	FFieldChanges Changes;
	RPG_CRAFTING_SET(Changes, Tier, InTier);
	RPG_CRAFTING_SET(Changes, Label, InLabel);
	RPG_CRAFTING_SET(Changes, bSelected, bInSelected);
	BroadcastChanges(*this, Changes);
}

void URpgCraftingStorageOptionViewModel::InitializeOption(FName InContainerId, const FText& InDisplayName, int32 InFreeCells,
	int32 InTotalCells, int32 InUnitsThatFit, const FText& InCapacityText, const FText& InContentsText, bool bInSelected, bool bInSuggested)
{
	FFieldChanges Changes;
	RPG_CRAFTING_SET(Changes, ContainerId, InContainerId);
	RPG_CRAFTING_SET(Changes, DisplayName, InDisplayName);
	RPG_CRAFTING_SET(Changes, FreeCells, InFreeCells);
	RPG_CRAFTING_SET(Changes, TotalCells, InTotalCells);
	RPG_CRAFTING_SET(Changes, UnitsThatFit, InUnitsThatFit);
	RPG_CRAFTING_SET(Changes, CapacityText, InCapacityText);
	RPG_CRAFTING_SET(Changes, ContentsText, InContentsText);
	RPG_CRAFTING_SET(Changes, bSelected, bInSelected);
	RPG_CRAFTING_SET(Changes, bSuggested, bInSuggested);
	BroadcastChanges(*this, Changes);
}

void URpgCraftingDetailRowViewModel::InitializeRow(const FText& InLabel, const FText& InValueText, bool bInEmphasized)
{
	FFieldChanges Changes;
	RPG_CRAFTING_SET(Changes, Label, InLabel);
	RPG_CRAFTING_SET(Changes, ValueText, InValueText);
	RPG_CRAFTING_SET(Changes, bEmphasized, bInEmphasized);
	BroadcastChanges(*this, Changes);
}

void URpgCraftingStationViewModel::BeginDestroy()
{
	UnbindCraftingStation();
	Super::BeginDestroy();
}

void URpgCraftingStationViewModel::BroadcastFields(TConstArrayView<UE::FieldNotification::FFieldId> Fields)
{
	for (const UE::FieldNotification::FFieldId& FieldId : Fields)
	{
		BroadcastFieldValueChanged(FieldId);
	}
}

void URpgCraftingStationViewModel::BindCraftingStation(URpgCraftingStationComponent* InStation, AActor* InRequestingActor)
{
	if (ObservedStation == InStation && RequestingActor == InRequestingActor)
	{
		BindWorldKnowledgeListener();
		Refresh();
		return;
	}

	const bool bObservedStationChanged = ObservedStation != InStation;
	const bool bRequestingActorChanged = RequestingActor != InRequestingActor;

	UnregisterMessageListeners();
	UnbindWorldKnowledgeListener();
	ObservedStation = InStation;
	RequestingActor = InRequestingActor;
	RegisterMessageListeners();
	BindWorldKnowledgeListener();
	Refresh();

	if (bObservedStationChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(ObservedStation);
	}
	if (bRequestingActorChanged)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(RequestingActor);
	}
}

void URpgCraftingStationViewModel::UnbindCraftingStation()
{
	UnregisterMessageListeners();
	UnbindWorldKnowledgeListener();
	CancelQueuedRefresh();
	ConnectedStorage.Reset();

	FFieldChanges Changes;
	RPG_CRAFTING_SET(Changes, ObservedStation, nullptr);
	RPG_CRAFTING_SET(Changes, RequestingActor, nullptr);
	RPG_CRAFTING_SET(Changes, SelectedRecipe, nullptr);
	RPG_CRAFTING_SET(Changes, CraftQuantity, 1);
	BroadcastFields(Changes);

	// Without a station every rebuild resets its projection to the unbound state.
	RebuildStationState();
	RebuildRecipeList();
	RebuildSelectedRecipeDetails();
}

void URpgCraftingStationViewModel::SetPresentationCatalog(URpgCraftingCategoryCatalog* InCatalog)
{
	if (PresentationCatalog == InCatalog)
	{
		return;
	}
	PresentationCatalog = InCatalog;
	if (ObservedStation)
	{
		RefreshRecipesAndDetails();
	}
}

void URpgCraftingStationViewModel::Refresh()
{
	CancelQueuedRefresh();
	RefreshStationState();
	RefreshRecipesAndDetails();
}

void URpgCraftingStationViewModel::RefreshStationState()
{
	SatisfyPendingRefresh(CraftingRefreshDomains::Station | CraftingRefreshDomains::Order);
	ResolveConnectedStorage();
	RebuildStationState();
	RebuildOrderStrip();
}

void URpgCraftingStationViewModel::RefreshRecipesAndDetails()
{
	SatisfyPendingRefresh(CraftingRefreshDomains::RecipesAndDetails);
	URpgCraftingRecipeDefinition* PreviousSelectedRecipe = SelectedRecipe.Get();
	const int32 PreviousCraftQuantity = CraftQuantity;
	RebuildRecipeList();
	RebuildSelectedRecipeDetails();

	if (PreviousSelectedRecipe != SelectedRecipe.Get())
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SelectedRecipe);
	}
	if (PreviousCraftQuantity != CraftQuantity)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(CraftQuantity);
	}
}

void URpgCraftingStationViewModel::RefreshSelectedRecipeDetails()
{
	const int32 PreviousCraftQuantity = CraftQuantity;
	RebuildSelectedRecipeDetails();
	if (PreviousCraftQuantity != CraftQuantity)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(CraftQuantity);
	}
}

void URpgCraftingStationViewModel::RefreshOrderProgress()
{
	const UWorld* World = ObservedStation ? ObservedStation->GetWorld() : nullptr;
	if (World && World->GetRealTimeSeconds() - LastConnectedStorageResolveTime >= ConnectedStorageRefreshInterval &&
		ResolveConnectedStorage())
	{
		// A chest came, went or moved; everything that counts materials or room needs a rebuild.
		RequestRefresh(CraftingRefreshDomains::All);
	}
	RebuildOrderStrip();
}

void URpgCraftingStationViewModel::SelectRecipe(URpgCraftingRecipeDefinition* RecipeDefinition)
{
	if (SelectedRecipe == RecipeDefinition)
	{
		return;
	}

	const int32 PreviousCraftQuantity = CraftQuantity;
	SelectedRecipe = RecipeDefinition;
	CraftQuantity = 1;
	RebuildSelectedRecipeDetails();
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SelectedRecipe);
	if (PreviousCraftQuantity != CraftQuantity)
	{
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(CraftQuantity);
	}
}

void URpgCraftingStationViewModel::SetSearchText(FText InSearchText)
{
	if (SearchText.ToString().Equals(InSearchText.ToString(), ESearchCase::CaseSensitive))
	{
		return;
	}

	SearchText = InSearchText;
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(SearchText);
	RefreshRecipesAndDetails();
}

void URpgCraftingStationViewModel::SetCategoryFilter(FGameplayTag InCategoryFilter)
{
	if (CategoryFilter == InCategoryFilter)
	{
		return;
	}

	CategoryFilter = InCategoryFilter;
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(CategoryFilter);
	RefreshRecipesAndDetails();
}

void URpgCraftingStationViewModel::ActivateCategoryRow(URpgCraftingCategoryViewModel* Row)
{
	if (!Row)
	{
		return;
	}
	switch (Row->GetKind())
	{
	case ERpgCraftingCategoryRowKind::All:
		SetCategoryFilter(FGameplayTag());
		break;
	case ERpgCraftingCategoryRowKind::Group:
		if (CategoryFilter == Row->GetCategoryTag())
		{
			ToggleCategoryGroup(Row->GetCategoryTag());
		}
		else
		{
			CollapsedGroups.Remove(Row->GetCategoryTag());
			SetCategoryFilter(Row->GetCategoryTag());
		}
		break;
	case ERpgCraftingCategoryRowKind::Subcategory:
		SetCategoryFilter(Row->GetCategoryTag());
		break;
	}
}

void URpgCraftingStationViewModel::ToggleCategoryGroup(FGameplayTag GroupTag)
{
	if (!GroupTag.IsValid())
	{
		return;
	}
	if (CollapsedGroups.Remove(GroupTag) == 0)
	{
		CollapsedGroups.Add(GroupTag);
	}
	RefreshRecipesAndDetails();
}

void URpgCraftingStationViewModel::SetTierFilter(int32 InTierFilter)
{
	const int32 NewTierFilter = FMath::Max(0, InTierFilter);
	if (TierFilter == NewTierFilter)
	{
		return;
	}

	TierFilter = NewTierFilter;
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(TierFilter);
	RefreshRecipesAndDetails();
}

void URpgCraftingStationViewModel::CycleTierFilter(int32 Direction)
{
	if (TierOptions.IsEmpty() || Direction == 0)
	{
		return;
	}
	int32 Index = TierOptions.IndexOfByPredicate([this](const TObjectPtr<URpgCraftingTierOptionViewModel>& Option)
	{
		return Option && Option->GetTier() == TierFilter;
	});
	Index = (FMath::Max(0, Index) + (Direction > 0 ? 1 : -1) + TierOptions.Num()) % TierOptions.Num();
	SetTierFilter(TierOptions[Index] ? TierOptions[Index]->GetTier() : 0);
}

void URpgCraftingStationViewModel::ToggleTierSortDirection()
{
	bTierSortAscending = !bTierSortAscending;
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(bTierSortAscending);
	RefreshRecipesAndDetails();
}

void URpgCraftingStationViewModel::SetCraftQuantity(int32 InCraftQuantity)
{
	const int32 MaxQuantity = ObservedStation ? ObservedStation->GetMaxOrderQuantity() : 1;
	const int32 NewCraftQuantity = FMath::Clamp(InCraftQuantity, 1, MaxQuantity);
	if (CraftQuantity == NewCraftQuantity)
	{
		return;
	}

	CraftQuantity = NewCraftQuantity;
	RebuildSelectedRecipeDetails();
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(CraftQuantity);
}

void URpgCraftingStationViewModel::IncreaseCraftQuantity(int32 Delta)
{
	SetCraftQuantity(CraftQuantity + Delta);
}

void URpgCraftingStationViewModel::SetCraftQuantityToMax()
{
	SetCraftQuantity(FMath::Max(1, MaxSelectedCraftQuantity));
}

void URpgCraftingStationViewModel::SelectTargetStorage(FName ContainerId)
{
	// None picks automatic storing.
	if (!bHasActiveOrder && SelectedRecipe)
	{
		TargetPickByRecipe.Add(FObjectKey(SelectedRecipe.Get()), ContainerId);
	}
	RefreshSelectedRecipeDetails();
}

FName URpgCraftingStationViewModel::CycleTargetStorage(int32 Direction)
{
	if (TargetStorageOptions.IsEmpty() || Direction == 0)
	{
		return SelectedTargetContainerId;
	}
	int32 Index = TargetStorageOptions.IndexOfByPredicate([this](const TObjectPtr<URpgCraftingStorageOptionViewModel>& Option)
	{
		return Option && Option->GetContainerId() == SelectedTargetContainerId;
	});
	Index = (FMath::Max(0, Index) + (Direction > 0 ? 1 : -1) + TargetStorageOptions.Num()) % TargetStorageOptions.Num();
	const FName NewTarget = TargetStorageOptions[Index] ? TargetStorageOptions[Index]->GetContainerId() : NAME_None;
	SelectTargetStorage(NewTarget);
	return NewTarget;
}

TArray<URpgCraftingCategoryViewModel*> URpgCraftingStationViewModel::GetCategoryRows() const
{
	return TArray<URpgCraftingCategoryViewModel*>(CategoryRows);
}

TArray<UObject*> URpgCraftingStationViewModel::GetRecipeListItems() const
{
	return TArray<UObject*>(RecipeListItems);
}

TArray<URpgCraftingRecipeViewModel*> URpgCraftingStationViewModel::GetFilteredRecipes() const
{
	return TArray<URpgCraftingRecipeViewModel*>(FilteredRecipes);
}

TArray<URpgCraftingIngredientViewModel*> URpgCraftingStationViewModel::GetSelectedIngredients() const
{
	return TArray<URpgCraftingIngredientViewModel*>(SelectedIngredients);
}

TArray<URpgCraftingOutputViewModel*> URpgCraftingStationViewModel::GetSelectedOutputs() const
{
	return TArray<URpgCraftingOutputViewModel*>(SelectedOutputs);
}

TArray<URpgCraftingDetailRowViewModel*> URpgCraftingStationViewModel::GetPreviewRows() const
{
	return TArray<URpgCraftingDetailRowViewModel*>(PreviewRows);
}

TArray<URpgCraftingTierOptionViewModel*> URpgCraftingStationViewModel::GetTierOptions() const
{
	return TArray<URpgCraftingTierOptionViewModel*>(TierOptions);
}

TArray<URpgCraftingStorageOptionViewModel*> URpgCraftingStationViewModel::GetTargetStorageOptions() const
{
	return TArray<URpgCraftingStorageOptionViewModel*>(TargetStorageOptions);
}

void URpgCraftingStationViewModel::RegisterMessageListeners()
{
	UnregisterMessageListeners();
	UWorld* World = ObservedStation ? ObservedStation->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(World);
	CraftingStationChangedHandle = MessageSubsystem.RegisterListener<FRpgCraftingStationChangeMessage>(
		FGameplayTag::RequestGameplayTag(TEXT("Rpg.Crafting.Message.StationChanged")),
		this,
		&ThisClass::HandleCraftingStationChanged);
	RecipeUnlockChangedHandle = MessageSubsystem.RegisterListener<FRpgRecipeUnlockChangeMessage>(
		FGameplayTag::RequestGameplayTag(TEXT("Rpg.Crafting.Message.RecipeUnlockChanged")),
		this,
		&ThisClass::HandleRecipeUnlockChanged);
	InventoryChangedHandle = MessageSubsystem.RegisterListener<FRpgInventoryChangeMessage>(
		FGameplayTag::RequestGameplayTag(TEXT("Rpg.Inventory.Message.StackChanged")),
		this,
		&ThisClass::HandleInventoryChanged);
}

void URpgCraftingStationViewModel::UnregisterMessageListeners()
{
	if (CraftingStationChangedHandle.IsValid())
	{
		CraftingStationChangedHandle.Unregister();
	}
	if (RecipeUnlockChangedHandle.IsValid())
	{
		RecipeUnlockChangedHandle.Unregister();
	}
	if (InventoryChangedHandle.IsValid())
	{
		InventoryChangedHandle.Unregister();
	}
}

void URpgCraftingStationViewModel::BindWorldKnowledgeListener()
{
	UnbindWorldKnowledgeListener();

	UWorld* World = ObservedStation ? ObservedStation->GetWorld() : nullptr;
	ARpgGameStateBase* GameState = World ? World->GetGameState<ARpgGameStateBase>() : nullptr;
	URpgWorldStorageKnowledgeComponent* Knowledge = GameState ? GameState->GetWorldStorageKnowledgeComponent() : nullptr;
	if (!Knowledge)
	{
		return;
	}

	ObservedWorldKnowledge = Knowledge;
	Knowledge->OnKnowledgeChanged.AddUniqueDynamic(this, &ThisClass::HandleWorldKnowledgeChanged);
}

void URpgCraftingStationViewModel::UnbindWorldKnowledgeListener()
{
	if (URpgWorldStorageKnowledgeComponent* Knowledge = ObservedWorldKnowledge.Get())
	{
		Knowledge->OnKnowledgeChanged.RemoveDynamic(this, &ThisClass::HandleWorldKnowledgeChanged);
	}
	ObservedWorldKnowledge.Reset();
}

void URpgCraftingStationViewModel::RequestRefresh(uint8 RefreshDomains)
{
	PendingRefreshDomains |= RefreshDomains;

	UWorld* World = ObservedStation ? ObservedStation->GetWorld() : nullptr;
	if (!World)
	{
		FlushPendingRefreshes();
		return;
	}

	RefreshQueue.Queue(World, this, &ThisClass::ExecuteQueuedRefresh);
}

void URpgCraftingStationViewModel::ExecuteQueuedRefresh()
{
	if (!RefreshQueue.Consume())
	{
		return;
	}

	FlushPendingRefreshes();
}

void URpgCraftingStationViewModel::FlushPendingRefreshes()
{
	const uint8 RefreshDomains = PendingRefreshDomains;
	PendingRefreshDomains = 0;

	if ((RefreshDomains & CraftingRefreshDomains::Station) != 0)
	{
		RefreshStationState();
	}
	if ((RefreshDomains & CraftingRefreshDomains::RecipesAndDetails) != 0)
	{
		RefreshRecipesAndDetails();
	}
	if ((RefreshDomains & CraftingRefreshDomains::Order) != 0 && (RefreshDomains & CraftingRefreshDomains::Station) == 0)
	{
		RebuildOrderStrip();
	}
}

void URpgCraftingStationViewModel::CancelQueuedRefresh()
{
	RefreshQueue.Cancel();
	PendingRefreshDomains = 0;
}

void URpgCraftingStationViewModel::SatisfyPendingRefresh(uint8 RefreshDomains)
{
	if ((PendingRefreshDomains & RefreshDomains) == 0)
	{
		return;
	}

	PendingRefreshDomains &= ~RefreshDomains;
	if (PendingRefreshDomains == 0)
	{
		RefreshQueue.Cancel();
	}
}

bool URpgCraftingStationViewModel::ResolveConnectedStorage()
{
	URpgCraftingStationComponent* Station = ObservedStation.Get();
	const TArray<URpgInventoryManagerComponent*> Resolved = Station ? Station->GetConnectedStorageInventories() : TArray<URpgInventoryManagerComponent*>();
	if (const UWorld* World = Station ? Station->GetWorld() : nullptr)
	{
		LastConnectedStorageResolveTime = World->GetRealTimeSeconds();
	}
	bool bChanged = Resolved.Num() != ConnectedStorage.Num();
	for (int32 Index = 0; !bChanged && Index < Resolved.Num(); ++Index)
	{
		bChanged = ConnectedStorage[Index].Get() != Resolved[Index];
	}
	if (bChanged)
	{
		ConnectedStorage.Reset(Resolved.Num());
		for (URpgInventoryManagerComponent* Inventory : Resolved)
		{
			ConnectedStorage.Add(Inventory);
		}
	}
	return bChanged;
}

TArray<URpgInventoryManagerComponent*> URpgCraftingStationViewModel::GetCachedConnectedStorage() const
{
	TArray<URpgInventoryManagerComponent*> Result;
	Result.Reserve(ConnectedStorage.Num());
	for (const TWeakObjectPtr<URpgInventoryManagerComponent>& Inventory : ConnectedStorage)
	{
		if (Inventory.IsValid())
		{
			Result.Add(Inventory.Get());
		}
	}
	return Result;
}

void URpgCraftingStationViewModel::RebuildStationState()
{
	URpgCraftingStationComponent* Station = ObservedStation.Get();
	const TArray<URpgInventoryManagerComponent*> Storage = GetCachedConnectedStorage();
	const TArray<FText> StorageNames = MakeStorageNames(Storage);
	const FRpgCraftingOrder* Order = Station && Station->HasCraftingOrder() ? &Station->GetCurrentOrder() : nullptr;

	FText NewConnectedStorageText;
	if (Station)
	{
		NewConnectedStorageText = Storage.IsEmpty()
			? NSLOCTEXT("RpgCrafting", "NoConnectedStorage", "No connected chest")
			: FText::Format(
				NSLOCTEXT("RpgCrafting", "ConnectedStorageCount", "Materials from {0} connected {0}|plural(one=chest,other=chests)"),
				FText::AsNumber(Storage.Num()));
	}

	FFieldChanges Changes;
	RPG_CRAFTING_SET(Changes, StationDisplayName, MakeStationDisplayName(Station));
	RPG_CRAFTING_SET(Changes, StationIcon, Station ? Station->GetStationIcon() : TSoftObjectPtr<UTexture2D>());
	RPG_CRAFTING_SET(Changes, ConnectedStorageCount, Storage.Num());
	RPG_CRAFTING_SET(Changes, ConnectedStorageText, NewConnectedStorageText);
	RPG_CRAFTING_SET(Changes, ConnectedStorageNamesText, JoinTexts(StorageNames, NSLOCTEXT("RpgCrafting", "NameSeparator", " · ")));
	RPG_CRAFTING_SET(Changes, bHasConnectedStorage, !Storage.IsEmpty());
	RPG_CRAFTING_SET(Changes, ActiveOrderId, Order ? Order->OrderId : FGuid());
	RPG_CRAFTING_SET(Changes, bHasActiveOrder, Order != nullptr);
	RPG_CRAFTING_SET(Changes, bStationPaused, Order && Order->bPaused);
	RPG_CRAFTING_SET(Changes, PauseResumeButtonText, MakePauseResumeButtonText(Order && Order->bPaused));
	BroadcastFields(Changes);
	RebuildActionAvailability();
}

void URpgCraftingStationViewModel::RebuildRecipeList()
{
	URpgCraftingStationComponent* Station = ObservedStation.Get();
	const URpgCraftingCategoryCatalog* Catalog = PresentationCatalog.Get();
	const TArray<URpgInventoryManagerComponent*> Storage = GetCachedConnectedStorage();
	const TArray<URpgCraftingRecipeDefinition*> OfferedRecipes = Station ? Station->GetAvailableRecipes() : TArray<URpgCraftingRecipeDefinition*>();
	const int32 MaxOrderQuantity = Station ? Station->GetMaxOrderQuantity() : 1;

	// Category tree: counts cover every offered recipe, independent of search and tier filters, so the tree stays stable.
	TMap<FGameplayTag, int32> GroupCounts;
	TMap<FGameplayTag, TMap<FGameplayTag, int32>> SubcategoryCounts;
	TSet<int32> OfferedTiers;
	for (const URpgCraftingRecipeDefinition* Recipe : OfferedRecipes)
	{
		if (!Recipe)
		{
			continue;
		}
		OfferedTiers.Add(FMath::Max(1, Recipe->RecipeTier));
		const FGameplayTag Group = URpgCraftingCategoryCatalog::ResolveCategoryGroup(Recipe->RecipeCategory);
		if (!Group.IsValid())
		{
			continue;
		}
		++GroupCounts.FindOrAdd(Group);
		if (const FGameplayTag Subcategory = ResolveSubcategory(Recipe->RecipeCategory); Subcategory.IsValid())
		{
			++SubcategoryCounts.FindOrAdd(Group).FindOrAdd(Subcategory);
		}
	}
	const auto SortTags = [Catalog](TArray<FGameplayTag>& Tags)
	{
		Tags.Sort([Catalog](const FGameplayTag& A, const FGameplayTag& B)
		{
			const int32 OrderA = GetCategorySortOrder(Catalog, A);
			const int32 OrderB = GetCategorySortOrder(Catalog, B);
			return OrderA != OrderB ? OrderA < OrderB : GetCategoryName(Catalog, A).CompareToCaseIgnored(GetCategoryName(Catalog, B)) < 0;
		});
	};

	FGameplayTag NewCategoryFilter = CategoryFilter;
	if (NewCategoryFilter.IsValid() && !GroupCounts.Contains(NewCategoryFilter) &&
		!SubcategoryCounts.FindRef(URpgCraftingCategoryCatalog::ResolveCategoryGroup(NewCategoryFilter)).Contains(NewCategoryFilter))
	{
		NewCategoryFilter = FGameplayTag();
	}

	TMap<FString, URpgCraftingCategoryViewModel*> PreviousRows;
	for (URpgCraftingCategoryViewModel* Row : CategoryRows)
	{
		if (Row)
		{
			PreviousRows.Add(FString::Printf(TEXT("%d:%s"), static_cast<int32>(Row->GetKind()), *Row->GetCategoryTag().ToString()), Row);
		}
	}
	TArray<TObjectPtr<URpgCraftingCategoryViewModel>> NewCategoryRows;
	const auto AddRow = [this, &PreviousRows, &NewCategoryRows](ERpgCraftingCategoryRowKind Kind, const FGameplayTag& Tag, const FText& Name,
		const TSoftObjectPtr<UTexture2D>& Icon, int32 Count, bool bExpanded, bool bSelected)
	{
		URpgCraftingCategoryViewModel* Row = PreviousRows.FindRef(FString::Printf(TEXT("%d:%s"), static_cast<int32>(Kind), *Tag.ToString()));
		if (!Row)
		{
			Row = NewObject<URpgCraftingCategoryViewModel>(this);
		}
		Row->InitializeCategory(Tag, Kind, Name, Icon, Count, bExpanded, bSelected);
		NewCategoryRows.Add(Row);
	};
	if (Station)
	{
		AddRow(ERpgCraftingCategoryRowKind::All, FGameplayTag(), NSLOCTEXT("RpgCrafting", "AllRecipes", "All recipes"),
			TSoftObjectPtr<UTexture2D>(), OfferedRecipes.Num(), true, !NewCategoryFilter.IsValid());
		TArray<FGameplayTag> Groups;
		GroupCounts.GetKeys(Groups);
		SortTags(Groups);
		for (const FGameplayTag& Group : Groups)
		{
			const bool bExpanded = !CollapsedGroups.Contains(Group);
			AddRow(ERpgCraftingCategoryRowKind::Group, Group, GetCategoryName(Catalog, Group), GetCategoryIcon(Catalog, Group),
				GroupCounts[Group], bExpanded, NewCategoryFilter == Group);
			if (!bExpanded)
			{
				continue;
			}
			TArray<FGameplayTag> Subcategories;
			SubcategoryCounts.FindRef(Group).GetKeys(Subcategories);
			SortTags(Subcategories);
			for (const FGameplayTag& Subcategory : Subcategories)
			{
				AddRow(ERpgCraftingCategoryRowKind::Subcategory, Subcategory, GetCategoryName(Catalog, Subcategory),
					GetCategoryIcon(Catalog, Subcategory), SubcategoryCounts[Group][Subcategory], false, NewCategoryFilter == Subcategory);
			}
		}
	}

	// Tier filter options: all tiers plus every tier the station offers.
	TArray<int32> Tiers = OfferedTiers.Array();
	Tiers.Sort();
	int32 NewTierFilter = Tiers.Contains(TierFilter) ? TierFilter : 0;
	TMap<int32, URpgCraftingTierOptionViewModel*> PreviousTierOptions;
	for (URpgCraftingTierOptionViewModel* Option : TierOptions)
	{
		if (Option)
		{
			PreviousTierOptions.Add(Option->GetTier(), Option);
		}
	}
	TArray<TObjectPtr<URpgCraftingTierOptionViewModel>> NewTierOptions;
	if (Station)
	{
		Tiers.Insert(0, 0);
		for (const int32 Tier : Tiers)
		{
			URpgCraftingTierOptionViewModel* Option = PreviousTierOptions.FindRef(Tier);
			if (!Option)
			{
				Option = NewObject<URpgCraftingTierOptionViewModel>(this);
			}
			Option->InitializeOption(Tier, Tier == 0 ? NSLOCTEXT("RpgCrafting", "AllTiers", "All tiers") : MakeTierLabel(Tier), Tier == NewTierFilter);
			NewTierOptions.Add(Option);
		}
	}

	// Recipe rows left by the filters, grouped by tier.
	TMap<URpgCraftingRecipeDefinition*, URpgCraftingRecipeViewModel*> PreviousRecipeRows;
	for (URpgCraftingRecipeViewModel* Row : FilteredRecipes)
	{
		if (Row && Row->GetRecipeDefinition())
		{
			PreviousRecipeRows.Add(Row->GetRecipeDefinition(), Row);
		}
	}
	TArray<TObjectPtr<URpgCraftingRecipeViewModel>> NewFilteredRecipes;
	for (URpgCraftingRecipeDefinition* Recipe : OfferedRecipes)
	{
		if (!Recipe ||
			(NewCategoryFilter.IsValid() && !Recipe->RecipeCategory.MatchesTag(NewCategoryFilter)) ||
			(NewTierFilter > 0 && FMath::Max(1, Recipe->RecipeTier) != NewTierFilter))
		{
			continue;
		}
		URpgCraftingRecipeViewModel* Row = PreviousRecipeRows.FindRef(Recipe);
		if (!Row)
		{
			Row = NewObject<URpgCraftingRecipeViewModel>(this);
		}
		Row->InitializeRecipe(Station, Recipe, URpgCraftingStationComponent::CountAffordableUnits(Storage, Recipe, MaxOrderQuantity));
		if (Row->MatchesSearchText(SearchText))
		{
			NewFilteredRecipes.Add(Row);
		}
	}
	const bool bAscending = bTierSortAscending;
	NewFilteredRecipes.Sort([bAscending](const URpgCraftingRecipeViewModel& A, const URpgCraftingRecipeViewModel& B)
	{
		const URpgCraftingRecipeDefinition* RecipeA = A.GetRecipeDefinition();
		const URpgCraftingRecipeDefinition* RecipeB = B.GetRecipeDefinition();
		const int32 TierA = FMath::Max(1, RecipeA->RecipeTier);
		const int32 TierB = FMath::Max(1, RecipeB->RecipeTier);
		if (TierA != TierB)
		{
			return bAscending ? TierA < TierB : TierA > TierB;
		}
		if (RecipeA->SortPriority != RecipeB->SortPriority)
		{
			return RecipeA->SortPriority < RecipeB->SortPriority;
		}
		return GetRecipeSortName(RecipeA).Compare(GetRecipeSortName(RecipeB), ESearchCase::IgnoreCase) < 0;
	});

	TMap<int32, URpgCraftingTierSectionViewModel*> PreviousSections;
	for (UObject* Item : RecipeListItems)
	{
		if (URpgCraftingTierSectionViewModel* Section = Cast<URpgCraftingTierSectionViewModel>(Item))
		{
			PreviousSections.Add(Section->GetTier(), Section);
		}
	}
	TArray<TObjectPtr<UObject>> NewRecipeListItems;
	for (int32 Index = 0; Index < NewFilteredRecipes.Num();)
	{
		const int32 Tier = FMath::Max(1, NewFilteredRecipes[Index]->GetRecipeDefinition()->RecipeTier);
		int32 End = Index;
		while (End < NewFilteredRecipes.Num() && FMath::Max(1, NewFilteredRecipes[End]->GetRecipeDefinition()->RecipeTier) == Tier)
		{
			++End;
		}
		URpgCraftingTierSectionViewModel* Section = PreviousSections.FindRef(Tier);
		if (!Section)
		{
			Section = NewObject<URpgCraftingTierSectionViewModel>(this);
		}
		Section->InitializeSection(Tier, MakeTierTitle(Catalog, Tier), End - Index);
		NewRecipeListItems.Add(Section);
		for (; Index < End; ++Index)
		{
			NewRecipeListItems.Add(NewFilteredRecipes[Index]);
		}
	}

	// Keep the selection while it is visible; otherwise prefer the order's recipe, then the first row.
	URpgCraftingRecipeDefinition* NewSelectedRecipe = SelectedRecipe.Get();
	const auto IsVisible = [&NewFilteredRecipes](const URpgCraftingRecipeDefinition* Recipe)
	{
		return Recipe && NewFilteredRecipes.ContainsByPredicate([Recipe](const TObjectPtr<URpgCraftingRecipeViewModel>& Row)
		{
			return Row && Row->GetRecipeDefinition() == Recipe;
		});
	};
	if (!IsVisible(NewSelectedRecipe))
	{
		const URpgCraftingRecipeDefinition* OrderRecipe = Station && Station->HasCraftingOrder() ? Station->GetCurrentOrder().Recipe.Get() : nullptr;
		NewSelectedRecipe = IsVisible(OrderRecipe)
			? const_cast<URpgCraftingRecipeDefinition*>(OrderRecipe)
			: (NewFilteredRecipes.Num() > 0 ? NewFilteredRecipes[0]->GetRecipeDefinition() : nullptr);
		CraftQuantity = 1;
	}
	SelectedRecipe = NewSelectedRecipe;

	FText NewRecipeListTitle = NSLOCTEXT("RpgCrafting", "AllRecipes", "All recipes");
	if (NewCategoryFilter.IsValid())
	{
		NewRecipeListTitle = GetCategoryName(Catalog, NewCategoryFilter);
	}

	const bool bCategoryRowsChanged = CategoryRows != NewCategoryRows;
	const bool bTierOptionsChanged = TierOptions != NewTierOptions;
	const bool bRecipeItemsChanged = RecipeListItems != NewRecipeListItems;
	FFieldChanges Changes;
	RPG_CRAFTING_SET(Changes, CategoryFilter, NewCategoryFilter);
	RPG_CRAFTING_SET(Changes, CategoryRows, NewCategoryRows);
	RPG_CRAFTING_SET(Changes, RecipeListTitleText, Station ? NewRecipeListTitle : FText::GetEmpty());
	RPG_CRAFTING_SET(Changes, TierFilter, NewTierFilter);
	RPG_CRAFTING_SET(Changes, TierOptions, NewTierOptions);
	RPG_CRAFTING_SET(Changes, TierFilterText, Station ? (NewTierFilter > 0 ? MakeTierLabel(NewTierFilter) : NSLOCTEXT("RpgCrafting", "AllTiers", "All tiers")) : FText::GetEmpty());
	RPG_CRAFTING_SET(Changes, TierSortText, Station
		? (bTierSortAscending ? NSLOCTEXT("RpgCrafting", "TierSortAscending", "Tier ↑") : NSLOCTEXT("RpgCrafting", "TierSortDescending", "Tier ↓"))
		: FText::GetEmpty());
	RPG_CRAFTING_SET(Changes, FilteredRecipes, NewFilteredRecipes);
	RPG_CRAFTING_SET(Changes, RecipeListItems, NewRecipeListItems);
	RPG_CRAFTING_SET(Changes, FilteredRecipeCount, FilteredRecipes.Num());
	RPG_CRAFTING_SET(Changes, bHasFilteredRecipes, !FilteredRecipes.IsEmpty());
	BroadcastFields(Changes);
	if (bCategoryRowsChanged)
	{
		OnCategoriesChanged.Broadcast();
	}
	if (bTierOptionsChanged)
	{
		OnTierOptionsChanged.Broadcast();
	}
	if (bRecipeItemsChanged)
	{
		OnRecipesChanged.Broadcast();
	}
}

void URpgCraftingStationViewModel::RebuildSelectedRecipeDetails()
{
	URpgCraftingStationComponent* Station = ObservedStation.Get();
	AActor* Actor = RequestingActor.Get();
	const URpgCraftingCategoryCatalog* Catalog = PresentationCatalog.Get();
	URpgCraftingRecipeDefinition* Recipe = Station ? SelectedRecipe.Get() : nullptr;
	const TArray<URpgInventoryManagerComponent*> Storage = Station ? GetCachedConnectedStorage() : TArray<URpgInventoryManagerComponent*>();
	const TArray<FText> StorageNames = MakeStorageNames(Storage);
	const FRpgCraftingOrder* Order = Station && Station->HasCraftingOrder() ? &Station->GetCurrentOrder() : nullptr;
	const int32 MaxOrderQuantity = Station ? Station->GetMaxOrderQuantity() : 1;
	const FText StationName = MakeStationDisplayName(Station);
	const FText UnitSingular = MakeUnitNoun(Station, 1);

	CraftQuantity = FMath::Clamp(CraftQuantity, 1, MaxOrderQuantity);
	const int32 NewAffordableUnitCount = Recipe ? URpgCraftingStationComponent::CountAffordableUnits(Storage, Recipe, MaxOrderQuantity) : 0;
	const bool bRecipeUnlocked = Station && Recipe && Station->IsRecipeUnlocked(Recipe);

	// First output facts.
	const FRpgCraftingOutputItem* FirstOutput = Recipe && !Recipe->OutputItems.IsEmpty() ? &Recipe->OutputItems[0] : nullptr;
	const TSubclassOf<URpgInventoryItemDefinition> OutputDefinition = FirstOutput ? FirstOutput->ItemDefinition : nullptr;
	const FText OutputName = GetItemDisplayName(OutputDefinition);
	const int32 OutputMaxStack = OutputDefinition ? URpgInventoryManagerComponent::GetEffectiveMaxStackSizeForDefinition(OutputDefinition) : 1;
	const FRpgInventoryGridSize OutputFootprint = GetItemFootprint(OutputDefinition);
	const URpgItemizationProfile* OutputProfile = GetItemizationProfile(OutputDefinition);
	const bool bSingleItemOutput = OutputDefinition && OutputMaxStack <= 1;

	// Target chests. While an order runs they describe its recipe and remaining units; otherwise the selection.
	// A target id of None is automatic storing: each unit goes into the first ranked chest with room.
	const URpgCraftingRecipeDefinition* ContextRecipe = Order ? Order->Recipe.Get() : Recipe;
	const int32 ContextWantedUnits = Order ? FMath::Max(0, Order->QuantityTotal - Order->QuantityCompleted) : MaxOrderQuantity;
	const TSubclassOf<URpgInventoryItemDefinition> ContextOutput =
		ContextRecipe && !ContextRecipe->OutputItems.IsEmpty() ? ContextRecipe->OutputItems[0].ItemDefinition : nullptr;
	const TArray<URpgInventoryManagerComponent*> AutomaticTargets = ContextOutput
		? URpgCraftingStationComponent::RankAutomaticOutputTargets(Storage, ContextOutput)
		: TArray<URpgInventoryManagerComponent*>();
	TArray<FRpgCraftingStorageCapacity> Capacities;
	TArray<FName> StorageIds;
	for (const URpgInventoryManagerComponent* Inventory : Storage)
	{
		StorageIds.Add(URpgCraftingStationComponent::GetStorageContainerId(Inventory));
		FRpgCraftingStorageCapacity& Capacity = Capacities.AddDefaulted_GetRef();
		if (ContextRecipe)
		{
			Capacity = RpgCraftingCapacity::Evaluate(*Inventory, *ContextRecipe, ContextWantedUnits);
		}
		else
		{
			RpgCraftingCapacity::GetRootCellUsage(*Inventory, Capacity.UsedCells, Capacity.TotalCells);
		}
	}
	// Automatic room is the sum over its chests, since every unit fills whichever chest still takes it.
	int32 AutomaticUnits = 0;
	FName NextAutomaticId;
	for (const URpgInventoryManagerComponent* Inventory : AutomaticTargets)
	{
		const int32 Index = Storage.IndexOfByKey(Inventory);
		if (!Capacities.IsValidIndex(Index)) { continue; }
		AutomaticUnits += Capacities[Index].UnitsThatFit;
		if (NextAutomaticId.IsNone() && Capacities[Index].UnitsThatFit > 0) { NextAutomaticId = StorageIds[Index]; }
	}
	AutomaticUnits = FMath::Min(AutomaticUnits, ContextWantedUnits);

	FName NewTargetId;
	if (Order)
	{
		NewTargetId = Order->TargetContainerId;
	}
	else if (const FName* Pick = Recipe ? TargetPickByRecipe.Find(FObjectKey(Recipe)) : nullptr; Pick && (Pick->IsNone() || StorageIds.Contains(*Pick)))
	{
		NewTargetId = *Pick;
	}
	const bool bAutomaticTarget = NewTargetId.IsNone();
	const int32 TargetIndex = bAutomaticTarget ? INDEX_NONE : StorageIds.IndexOfByKey(NewTargetId);
	const FText AutomaticName = NSLOCTEXT("RpgCrafting", "AutomaticStorageName", "Automatic");
	const FText SelectedTargetDisplayName = bAutomaticTarget
		? (Station && !Storage.IsEmpty() ? AutomaticName : FText::GetEmpty())
		: (StorageNames.IsValidIndex(TargetIndex) ? StorageNames[TargetIndex] : FText::GetEmpty());
	const FText ContextOutputName = GetItemDisplayName(ContextOutput);
	const int32 ContextOutputCount = ContextRecipe && !ContextRecipe->OutputItems.IsEmpty() ? ContextRecipe->OutputItems[0].Count : 1;

	TArray<TObjectPtr<URpgCraftingStorageOptionViewModel>> NewTargetOptions;
	const auto FindOrCreateOption = [this](FName ContainerId)
	{
		for (URpgCraftingStorageOptionViewModel* Existing : TargetStorageOptions)
		{
			if (Existing && Existing->GetContainerId() == ContainerId) { return Existing; }
		}
		return NewObject<URpgCraftingStorageOptionViewModel>(this);
	};
	if (Station && !Storage.IsEmpty())
	{
		URpgCraftingStorageOptionViewModel* Automatic = FindOrCreateOption(NAME_None);
		const FText AutomaticRoom = !ContextRecipe
			? FText::Format(NSLOCTEXT("RpgCrafting", "AutomaticChestCount", "{0} {0}|plural(one=chest,other=chests)"), FText::AsNumber(Storage.Num()))
			: (AutomaticTargets.IsEmpty()
				? NSLOCTEXT("RpgCrafting", "AutomaticNoChest", "No suitable chest")
				: FText::Format(NSLOCTEXT("RpgCrafting", "AutomaticRoom", "{0} {0}|plural(one=chest,other=chests) · room for {1} {2}"),
					FText::AsNumber(AutomaticTargets.Num()), FText::AsNumber(AutomaticUnits * ContextOutputCount), ContextOutputName));
		Automatic->InitializeOption(NAME_None, AutomaticName, 0, 0, AutomaticUnits, AutomaticRoom,
			NSLOCTEXT("RpgCrafting", "AutomaticRule", "Assigned chests first, then unassigned ones"),
			bAutomaticTarget, false);
		NewTargetOptions.Add(Automatic);
	}
	for (int32 Index = 0; Index < Storage.Num(); ++Index)
	{
		URpgCraftingStorageOptionViewModel* Option = FindOrCreateOption(StorageIds[Index]);
		const FRpgCraftingStorageCapacity& Capacity = Capacities[Index];
		Option->InitializeOption(StorageIds[Index], StorageNames[Index], Capacity.GetFreeCells(), Capacity.TotalCells, Capacity.UnitsThatFit,
			FText::Format(NSLOCTEXT("RpgCrafting", "StorageOptionFreeCells", "{0} {0}|plural(one=cell,other=cells) free"), FText::AsNumber(Capacity.GetFreeCells())),
			MakeStorageContentsText(Storage[Index]),
			Index == TargetIndex, StorageIds[Index] == NextAutomaticId);
		NewTargetOptions.Add(Option);
	}

	const FRpgCraftingStorageCapacity* TargetCapacity = Capacities.IsValidIndex(TargetIndex) ? &Capacities[TargetIndex] : nullptr;
	const int32 UnitsThatFitTarget = bAutomaticTarget ? AutomaticUnits : (TargetCapacity ? TargetCapacity->UnitsThatFit : 0);
	const int32 NewMaxSelectedCraftQuantity = Recipe ? FMath::Clamp(FMath::Min(NewAffordableUnitCount, UnitsThatFitTarget), 1, MaxOrderQuantity) : 0;

	// Capacity texts of the chosen target.
	FText NewTargetCapacityText;
	FText NewTargetDetailText;
	bool bNewTargetHasRoom = false;
	const URpgCraftingRecipeDefinition* CapacityRecipe = ContextRecipe;
	const int32 CapacityWantedUnits = Order ? ContextWantedUnits : CraftQuantity;
	const auto MakeRoomText = [&](int32 FitUnits)
	{
		return FitUnits > 0
			? FText::Format(
				NSLOCTEXT("RpgCrafting", "TargetRoom", "Room for {0} of {1} {2}"),
				FText::AsNumber(FitUnits * ContextOutputCount),
				FText::AsNumber(CapacityWantedUnits * ContextOutputCount),
				ContextOutputName)
			: FText::Format(NSLOCTEXT("RpgCrafting", "TargetNoRoom", "No room for {0}"), ContextOutputName);
	};
	if (Station && Storage.IsEmpty())
	{
		NewTargetCapacityText = NSLOCTEXT("RpgCrafting", "NoStorageCapacity", "No connected chest");
		NewTargetDetailText = NSLOCTEXT("RpgCrafting", "NoStorageCapacityHint", "Place a chest within reach of the station.");
	}
	else if (Station && bAutomaticTarget && CapacityRecipe && ContextOutput)
	{
		if (AutomaticTargets.IsEmpty())
		{
			NewTargetCapacityText = NSLOCTEXT("RpgCrafting", "AutomaticNoChest", "No suitable chest");
			NewTargetDetailText = FText::Format(
				NSLOCTEXT("RpgCrafting", "AutomaticNoChestHint", "Every connected chest is assigned to other materials. Assign one to {0} or choose a chest."),
				ContextOutputName);
		}
		else
		{
			const int32 FitUnits = FMath::Min(AutomaticUnits, CapacityWantedUnits);
			bNewTargetHasRoom = FitUnits >= CapacityWantedUnits;
			NewTargetCapacityText = MakeRoomText(FitUnits);
			const int32 NextIndex = StorageIds.IndexOfByKey(NextAutomaticId);
			NewTargetDetailText = StorageNames.IsValidIndex(NextIndex)
				? FText::Format(NSLOCTEXT("RpgCrafting", "AutomaticNext", "Next into {0} · {1} {1}|plural(one=chest,other=chests) in use"),
					StorageNames[NextIndex], FText::AsNumber(AutomaticTargets.Num()))
				: FText::Format(NSLOCTEXT("RpgCrafting", "AutomaticFull", "All {0} {0}|plural(one=chest,other=chests) for {1} are full"),
					FText::AsNumber(AutomaticTargets.Num()), ContextOutputName);
		}
	}
	else if (Station && Order && !TargetCapacity)
	{
		NewTargetCapacityText = NSLOCTEXT("RpgCrafting", "TargetMissing", "Target storage missing");
		NewTargetDetailText = NSLOCTEXT("RpgCrafting", "TargetMissingHint", "Choose a connected chest as target.");
	}
	else if (TargetCapacity && CapacityRecipe && ContextOutput)
	{
		const int32 FitUnits = FMath::Min(TargetCapacity->UnitsThatFit, CapacityWantedUnits);
		bNewTargetHasRoom = FitUnits >= CapacityWantedUnits;
		NewTargetCapacityText = MakeRoomText(FitUnits);
		const URpgInventoryContainerComponent* TargetContainer = FindStorageContainer(Storage[TargetIndex]);
		int64 AssignmentOrder = 0;
		const int32 AssignmentRank = TargetContainer ? TargetContainer->GetAssignmentRank(ContextOutput, AssignmentOrder) : INDEX_NONE;
		if (TargetContainer && !TargetContainer->GetAssignments().IsEmpty() && (AssignmentRank == INDEX_NONE || AssignmentRank > 1))
		{
			// The chest is meant for other materials; storing here still works but mixes its contents.
			NewTargetDetailText = FText::Format(
				NSLOCTEXT("RpgCrafting", "TargetAssignedElsewhere", "Assigned to {0}, not to {1}"),
				JoinTexts(MakeAssignmentNames(TargetContainer), NSLOCTEXT("RpgCrafting", "ListSeparator", ", ")), ContextOutputName);
		}
		else
		{
			NewTargetDetailText = TargetCapacity->MaxStackSize > 1
				? FText::Format(
					NSLOCTEXT("RpgCrafting", "TargetStackDetail", "{0} present · {1} {1}|plural(one=cell,other=cells) free · {2} per stack"),
					FText::AsNumber(TargetCapacity->PresentCount),
					FText::AsNumber(TargetCapacity->GetFreeCells()),
					FText::AsNumber(TargetCapacity->MaxStackSize))
				: FText::Format(
					NSLOCTEXT("RpgCrafting", "TargetItemDetail", "{0} / {1} cells used · {2} each"),
					FText::AsNumber(TargetCapacity->UsedCells),
					FText::AsNumber(TargetCapacity->TotalCells),
					MakeCellsText(TargetCapacity->Footprint));
		}
	}

	// Materials.
	TArray<TObjectPtr<URpgCraftingIngredientViewModel>> NewIngredients;
	TArray<FText> FormulaInputs;
	if (Recipe)
	{
		for (int32 Index = 0; Index < Recipe->RequiredResources.Num(); ++Index)
		{
			const FRpgCraftingResourceCost& Cost = Recipe->RequiredResources[Index];
			URpgCraftingIngredientViewModel* Row = ReuseOrCreate(this, SelectedIngredients, Index);
			Row->InitializeIngredient(Cost.ItemDefinition, Cost.Count, CraftQuantity, URpgCraftingStationComponent::CountStorageResource(Storage, Cost.ItemDefinition));
			NewIngredients.Add(Row);
			FormulaInputs.Add(MakeCountedName(Cost.Count, GetItemDisplayName(Cost.ItemDefinition)));
		}
	}
	TArray<TObjectPtr<URpgCraftingOutputViewModel>> NewOutputs;
	TArray<FText> FormulaOutputs;
	if (Recipe)
	{
		for (int32 Index = 0; Index < Recipe->OutputItems.Num(); ++Index)
		{
			const FRpgCraftingOutputItem& Output = Recipe->OutputItems[Index];
			URpgCraftingOutputViewModel* Row = ReuseOrCreate(this, SelectedOutputs, Index);
			Row->InitializeOutput(Output.ItemDefinition, Output.Count * CraftQuantity);
			NewOutputs.Add(Row);
			FormulaOutputs.Add(MakeCountedName(Output.Count, GetItemDisplayName(Output.ItemDefinition)));
		}
	}

	// Preview key values.
	struct FPreviewRow
	{
		FText Label;
		FText Value;
		bool bEmphasized = false;
	};
	TArray<FPreviewRow> RowData;
	if (Recipe && FirstOutput)
	{
		TArray<FRpgItemStatRange> StatRanges;
		if (OutputProfile && OutputProfile->GetBaseStatRanges(FMath::Max(1, Recipe->OutputItemLevel), StatRanges))
		{
			for (const FRpgItemStatRange& Range : StatRanges)
			{
				const bool bGuaranteed = FMath::IsNearlyEqual(Range.MinValue, Range.MaxValue);
				RowData.Add({
					bGuaranteed
						? FText::Format(NSLOCTEXT("RpgCrafting", "GuaranteedStatLabel", "{0} · guaranteed"), GetRpgItemStatDisplayName(Range.StatTag))
						: GetRpgItemStatDisplayName(Range.StatTag),
					bGuaranteed
						? MakeStatValueText(Range.MinValue)
						: FText::Format(NSLOCTEXT("RpgCrafting", "StatRange", "{0}–{1}"), MakeStatValueText(Range.MinValue), MakeStatValueText(Range.MaxValue)),
					RowData.IsEmpty()});
			}
		}
		else
		{
			RowData.Add({
				FText::Format(NSLOCTEXT("RpgCrafting", "YieldPerUnit", "Yield per {0}"), UnitSingular),
				FText::Format(NSLOCTEXT("RpgCrafting", "YieldPieces", "{0} {0}|plural(one=piece,other=pieces)"), FText::AsNumber(FirstOutput->Count)),
				true});
		}
		RowData.Add({
			FText::Format(NSLOCTEXT("RpgCrafting", "TimePerUnit", "Time per {0}"), UnitSingular),
			MakeDurationText(Recipe->CraftTime),
			false});
		if (OutputMaxStack > 1)
		{
			RowData.Add({
				NSLOCTEXT("RpgCrafting", "PerStack", "Per stack"),
				FText::Format(NSLOCTEXT("RpgCrafting", "UpToStack", "up to {0}"), FText::AsNumber(OutputMaxStack)),
				false});
		}
		if (const float Weight = GetItemEquipLoadWeight(OutputDefinition); Weight > 0.0f)
		{
			RowData.Add({NSLOCTEXT("RpgCrafting", "Weight", "Weight"), MakeStatValueText(Weight), false});
		}
	}
	TArray<TObjectPtr<URpgCraftingDetailRowViewModel>> NewPreviewRows;
	for (int32 Index = 0; Index < RowData.Num(); ++Index)
	{
		URpgCraftingDetailRowViewModel* Row = ReuseOrCreate(this, PreviewRows, Index);
		Row->InitializeRow(RowData[Index].Label, RowData[Index].Value, RowData[Index].bEmphasized);
		NewPreviewRows.Add(Row);
	}

	// Identity, plan and status texts.
	FText NewBreadcrumb;
	FText NewOutputKind;
	FText NewOutputSize;
	FText NewTierText;
	FText NewPlanSummary;
	FText NewPlanDetail;
	FText NewMaterialsSummary;
	FText NewStatusText;
	FText NewStatusHint;
	bool bNewStatusWarning = false;
	const FText UnitPlural = MakeUnitNoun(Station, CraftQuantity);
	if (Recipe)
	{
		const FGameplayTag Group = URpgCraftingCategoryCatalog::ResolveCategoryGroup(Recipe->RecipeCategory);
		const FGameplayTag Subcategory = ResolveSubcategory(Recipe->RecipeCategory);
		NewTierText = MakeTierLabel(FMath::Max(1, Recipe->RecipeTier));
		if (Group.IsValid() && Subcategory.IsValid())
		{
			NewBreadcrumb = FText::Format(NSLOCTEXT("RpgCrafting", "BreadcrumbSub", "{0} / {1} · {2}"),
				GetCategoryName(Catalog, Group), GetCategoryName(Catalog, Subcategory), NewTierText);
		}
		else if (Group.IsValid())
		{
			NewBreadcrumb = FText::Format(NSLOCTEXT("RpgCrafting", "BreadcrumbGroup", "{0} · {1}"), GetCategoryName(Catalog, Group), NewTierText);
		}
		else
		{
			NewBreadcrumb = NewTierText;
		}
		NewOutputKind = OutputProfile
			? NSLOCTEXT("RpgCrafting", "KindItemized", "Single item · individual stats")
			: (OutputMaxStack > 1 ? NSLOCTEXT("RpgCrafting", "KindStackable", "Stackable material") : NSLOCTEXT("RpgCrafting", "KindSingle", "Single item"));
		if (bSingleItemOutput && OutputFootprint.IsValid())
		{
			NewOutputSize = FText::Format(NSLOCTEXT("RpgCrafting", "SpacePerItem", "Space per item {0} cells"), MakeCellsText(OutputFootprint));
		}
		NewPlanSummary = Recipe->OutputItems.Num() == 1
			? FText::Format(NSLOCTEXT("RpgCrafting", "PlanSummary", "{0} {1} → {2} {3}"),
				FText::AsNumber(CraftQuantity), UnitPlural, FText::AsNumber(CraftQuantity * FirstOutput->Count), OutputName)
			: FText::Format(NSLOCTEXT("RpgCrafting", "PlanSummaryMulti", "{0} {1}"), FText::AsNumber(CraftQuantity), UnitPlural);
		NewPlanDetail = bSingleItemOutput && OutputFootprint.IsValid()
			? FText::Format(NSLOCTEXT("RpgCrafting", "PlanDetailItems", "{0} {0}|plural(one=item,other=items) · {1} cells at {2} each · {3}"),
				FText::AsNumber(CraftQuantity * FirstOutput->Count),
				FText::AsNumber(CraftQuantity * FirstOutput->Count * OutputFootprint.Width * OutputFootprint.Height),
				MakeCellsText(OutputFootprint),
				MakeDurationText(Recipe->CraftTime * CraftQuantity))
			: FText::Format(NSLOCTEXT("RpgCrafting", "PlanDetailTime", "{0} pure time"), MakeDurationText(Recipe->CraftTime * CraftQuantity));
		NewMaterialsSummary = Storage.IsEmpty()
			? NSLOCTEXT("RpgCrafting", "MaterialsNoStorage", "No connected chest")
			: (Recipe->RequiredResources.IsEmpty()
				? NSLOCTEXT("RpgCrafting", "MaterialsFree", "Needs no materials")
				: FText::Format(NSLOCTEXT("RpgCrafting", "MaterialsSummary", "From connected chests · enough for {0} {1} now"),
					FText::AsNumber(NewAffordableUnitCount), MakeUnitNoun(Station, NewAffordableUnitCount)));

		bNewStatusWarning = true;
		if (!bRecipeUnlocked)
		{
			NewStatusText = NSLOCTEXT("RpgCrafting", "StatusLocked", "Locked");
			NewStatusHint = NSLOCTEXT("RpgCrafting", "StatusLockedHint", "This recipe is not unlocked yet.");
		}
		else if (Storage.IsEmpty())
		{
			NewStatusText = NSLOCTEXT("RpgCrafting", "StatusNoStorage", "No connected chest");
			NewStatusHint = NSLOCTEXT("RpgCrafting", "StatusNoStorageHint", "The station takes materials from nearby chests and stores the results there.");
		}
		else if (Order)
		{
			bNewStatusWarning = false;
			NewStatusText = NSLOCTEXT("RpgCrafting", "StatusOrderActive", "An order is active");
			NewStatusHint = NSLOCTEXT("RpgCrafting", "StatusOrderActiveHint", "Stop the current order to start another one.");
		}
		else if (NewAffordableUnitCount <= 0)
		{
			NewStatusText = NSLOCTEXT("RpgCrafting", "StatusMissingMaterials", "Missing materials");
			NewStatusHint = FText::Format(NSLOCTEXT("RpgCrafting", "StatusMissingMaterialsHint", "The connected chests lack the materials for one {0}."), UnitSingular);
		}
		else if (UnitsThatFitTarget <= 0 && bAutomaticTarget)
		{
			NewStatusText = AutomaticTargets.IsEmpty()
				? NSLOCTEXT("RpgCrafting", "StatusNoSuitableChest", "No suitable chest")
				: FText::Format(NSLOCTEXT("RpgCrafting", "StatusAutomaticFull", "No chest has room for {0}"), OutputName);
			NewStatusHint = FText::Format(
				NSLOCTEXT("RpgCrafting", "StatusAutomaticFullHint", "Make room in a chest assigned to {0} or in an unassigned one, or choose a chest."),
				OutputName);
		}
		else if (UnitsThatFitTarget <= 0)
		{
			NewStatusText = FText::Format(NSLOCTEXT("RpgCrafting", "StatusTargetFull", "No room in {0}"), SelectedTargetDisplayName);
			NewStatusHint = NSLOCTEXT("RpgCrafting", "StatusTargetFullHint", "Empty the chest or choose another target.");
		}
		else
		{
			bNewStatusWarning = false;
			NewStatusText = FText::Format(NSLOCTEXT("RpgCrafting", "StatusReady", "{0} ready"), StationName);
			NewStatusHint = FText::Format(NSLOCTEXT("RpgCrafting", "StatusReadyHint", "Materials are taken from the connected chests when each {0} starts."), UnitSingular);
		}
	}

	const FRpgCraftingStationPresentation Presentation = Station ? Station->GetStationPresentation() : FRpgCraftingStationPresentation();
	FText NewStartActionText;
	if (Station)
	{
		NewStartActionText = Order
			? NSLOCTEXT("RpgCrafting", "StartBlockedByOrder", "An order is active")
			: (Presentation.StartActionText.IsEmpty() ? NSLOCTEXT("RpgCrafting", "DefaultStartAction", "Start crafting") : Presentation.StartActionText);
	}
	const bool bNewCanStartOrder = Station && Recipe && !Order && UnitsThatFitTarget > 0 &&
		Station->CanStartCraftingOrder(Actor, Recipe, CraftQuantity, NewTargetId);

	FFieldChanges Changes;
	RPG_CRAFTING_SET(Changes, SelectedRecipeName, Recipe ? Recipe->DisplayName : FText::GetEmpty());
	RPG_CRAFTING_SET(Changes, SelectedRecipeDescription, Recipe ? Recipe->Description : FText::GetEmpty());
	RPG_CRAFTING_SET(Changes, PreviewIcon, GetRecipeIcon(Recipe));
	RPG_CRAFTING_SET(Changes, BreadcrumbText, NewBreadcrumb);
	RPG_CRAFTING_SET(Changes, OutputKindText, NewOutputKind);
	RPG_CRAFTING_SET(Changes, OutputSizeText, NewOutputSize);
	RPG_CRAFTING_SET(Changes, SelectedTierText, NewTierText);
	RPG_CRAFTING_SET(Changes, FormulaLabelText, Recipe ? FText::Format(NSLOCTEXT("RpgCrafting", "FormulaLabel", "One {0} uses"), UnitSingular) : FText::GetEmpty());
	RPG_CRAFTING_SET(Changes, FormulaInputsText, JoinTexts(FormulaInputs, NSLOCTEXT("RpgCrafting", "FormulaSeparator", " + ")));
	RPG_CRAFTING_SET(Changes, FormulaOutputText, JoinTexts(FormulaOutputs, NSLOCTEXT("RpgCrafting", "FormulaSeparator", " + ")));
	RPG_CRAFTING_SET(Changes, SelectedStatusText, NewStatusText);
	RPG_CRAFTING_SET(Changes, bSelectedStatusIsWarning, bNewStatusWarning);
	RPG_CRAFTING_SET(Changes, SelectedStatusHint, NewStatusHint);
	RPG_CRAFTING_SET(Changes, PreviewRows, NewPreviewRows);
	RPG_CRAFTING_SET(Changes, SelectedIngredients, NewIngredients);
	RPG_CRAFTING_SET(Changes, SelectedOutputs, NewOutputs);
	RPG_CRAFTING_SET(Changes, AffordableUnitCount, NewAffordableUnitCount);
	RPG_CRAFTING_SET(Changes, MaterialsSummaryText, NewMaterialsSummary);
	RPG_CRAFTING_SET(Changes, TargetStorageOptions, NewTargetOptions);
	RPG_CRAFTING_SET(Changes, SelectedTargetContainerId, NewTargetId);
	RPG_CRAFTING_SET(Changes, SelectedTargetName, SelectedTargetDisplayName);
	RPG_CRAFTING_SET(Changes, TargetLabelText, Station
		? (Order ? NSLOCTEXT("RpgCrafting", "TargetLabelOrder", "Target · current order") : NSLOCTEXT("RpgCrafting", "TargetLabel", "Store in"))
		: FText::GetEmpty());
	RPG_CRAFTING_SET(Changes, TargetCapacityText, NewTargetCapacityText);
	RPG_CRAFTING_SET(Changes, TargetDetailText, NewTargetDetailText);
	RPG_CRAFTING_SET(Changes, bTargetHasRoom, bNewTargetHasRoom);
	RPG_CRAFTING_SET(Changes, MaxSelectedCraftQuantity, NewMaxSelectedCraftQuantity);
	RPG_CRAFTING_SET(Changes, SelectedTotalCraftTime, Recipe ? FMath::Max(0.0f, Recipe->CraftTime) * CraftQuantity : 0.0f);
	RPG_CRAFTING_SET(Changes, PlanSummaryText, NewPlanSummary);
	RPG_CRAFTING_SET(Changes, PlanDetailText, NewPlanDetail);
	RPG_CRAFTING_SET(Changes, StartActionText, NewStartActionText);
	RPG_CRAFTING_SET(Changes, bCanStartOrder, bNewCanStartOrder);
	BroadcastFields(Changes);

	RebuildActionAvailability();
	RebuildOrderStrip();
	OnSelectedRecipeDetailsChanged.Broadcast();
}

void URpgCraftingStationViewModel::RebuildOrderStrip()
{
	URpgCraftingStationComponent* Station = ObservedStation.Get();
	const FRpgCraftingOrder* Order = Station && Station->HasCraftingOrder() ? &Station->GetCurrentOrder() : nullptr;
	const FRpgCraftingStationPresentation Presentation = Station ? Station->GetStationPresentation() : FRpgCraftingStationPresentation();
	const URpgCraftingRecipeDefinition* Recipe = Order ? Order->Recipe.Get() : nullptr;

	FText NewLabel;
	FText NewTitle;
	FText NewCounts;
	FText NewStatus;
	FText NewHint;
	float NewProgress = 0.0f;
	bool bNewWaiting = false;
	if (Station)
	{
		NewLabel = Presentation.OrderLabel.IsEmpty() ? NSLOCTEXT("RpgCrafting", "DefaultOrderLabel", "Crafting") : Presentation.OrderLabel;
	}
	if (Station && !Order)
	{
		NewTitle = FText::Format(NSLOCTEXT("RpgCrafting", "StatusReady", "{0} ready"), MakeStationDisplayName(Station));
		NewCounts = PlanSummaryText;
		NewStatus = NSLOCTEXT("RpgCrafting", "OrderIdle", "Waiting for your start");
		NewHint = NSLOCTEXT("RpgCrafting", "OrderIdleHint", "Choose a recipe, quantity and target storage.");
	}
	else if (Order && Recipe)
	{
		const float UnitDuration = FMath::Max(0.0f, Recipe->CraftTime);
		const float ServerTime = GetServerWorldTimeSeconds(Station);
		float UnitRemaining = UnitDuration;
		float UnitFraction = 0.0f;
		if (Order->bUnitPaid)
		{
			if (Order->State != ERpgCraftingOrderState::Running)
			{
				// The paid unit is produced and waits for delivery.
				UnitRemaining = 0.0f;
			}
			else if (Order->bPaused || Order->UnitFinishServerTime <= Order->UnitStartServerTime)
			{
				UnitRemaining = FMath::Min(Order->PausedRemainingTime, UnitDuration);
			}
			else
			{
				UnitRemaining = FMath::Clamp(Order->UnitFinishServerTime - ServerTime, 0.0f, UnitDuration);
			}
			UnitFraction = UnitDuration > 0.0f ? 1.0f - UnitRemaining / UnitDuration : 1.0f;
		}
		const int32 Total = FMath::Max(1, Order->QuantityTotal);
		const int32 UnitsAfterCurrent = FMath::Max(0, Order->QuantityTotal - Order->QuantityCompleted - 1);
		NewProgress = FMath::Clamp((Order->QuantityCompleted + UnitFraction) / Total, 0.0f, 1.0f);
		NewTitle = Recipe->DisplayName;
		NewCounts = FText::Format(NSLOCTEXT("RpgCrafting", "OrderCounts", "{0} / {1} {2} done"),
			FText::AsNumber(Order->QuantityCompleted), FText::AsNumber(Order->QuantityTotal), MakeUnitNoun(Station, Order->QuantityTotal));
		const FText UnitSingular = MakeUnitNoun(Station, 1);

		TArray<FText> StorageNames;
		const bool bAutomaticOrder = Order->TargetContainerId.IsNone();
		FText TargetName = NSLOCTEXT("RpgCrafting", "TheTargetChest", "the target chest");
		const TArray<URpgInventoryManagerComponent*> Storage = GetCachedConnectedStorage();
		StorageNames = MakeStorageNames(Storage);
		for (int32 Index = 0; Index < Storage.Num(); ++Index)
		{
			if (URpgCraftingStationComponent::GetStorageContainerId(Storage[Index]) == Order->TargetContainerId)
			{
				TargetName = StorageNames[Index];
			}
		}

		if (Order->bPaused)
		{
			NewStatus = NSLOCTEXT("RpgCrafting", "OrderPaused", "Paused");
			NewHint = NSLOCTEXT("RpgCrafting", "OrderPausedHint", "Resume to continue.");
		}
		else
		{
			switch (Order->State)
			{
			case ERpgCraftingOrderState::Running:
				NewStatus = FText::Format(NSLOCTEXT("RpgCrafting", "OrderRunning", "Running · {0} left"),
					MakeDurationText(UnitRemaining + UnitDuration * UnitsAfterCurrent));
				NewHint = FText::Format(NSLOCTEXT("RpgCrafting", "OrderRunningHint", "Each {0} takes its materials from the chests when it starts."), UnitSingular);
				break;
			case ERpgCraftingOrderState::WaitingForMaterials:
				{
					bNewWaiting = true;
					TArray<FText> Missing;
					for (const FRpgCraftingResourceCost& Cost : Recipe->RequiredResources)
					{
						const int32 MissingCount = Cost.Count - URpgCraftingStationComponent::CountStorageResource(Storage, Cost.ItemDefinition);
						if (MissingCount > 0)
						{
							Missing.Add(MakeCountedName(MissingCount, GetItemDisplayName(Cost.ItemDefinition)));
						}
					}
					NewStatus = NSLOCTEXT("RpgCrafting", "OrderWaitingMaterials", "Waiting for materials");
					NewHint = FText::Format(NSLOCTEXT("RpgCrafting", "OrderWaitingMaterialsHint", "Missing for the next {0}: {1}"),
						UnitSingular, JoinTexts(Missing, NSLOCTEXT("RpgCrafting", "ListSeparator", ", ")));
				}
				break;
			case ERpgCraftingOrderState::WaitingForSpace:
				{
					bNewWaiting = true;
					const TSubclassOf<URpgInventoryItemDefinition> OutputDefinition = Recipe->OutputItems.IsEmpty() ? nullptr : Recipe->OutputItems[0].ItemDefinition;
					const FRpgInventoryGridSize Footprint = GetItemFootprint(OutputDefinition);
					if (bAutomaticOrder)
					{
						NewStatus = NSLOCTEXT("RpgCrafting", "OrderWaitingSpaceAutomatic", "Waiting: no chest has room");
						NewHint = FText::Format(
							NSLOCTEXT("RpgCrafting", "OrderWaitingSpaceAutomaticHint", "Make room in a chest assigned to {0} or in an unassigned one, or choose a chest."),
							GetItemDisplayName(OutputDefinition));
					}
					else
					{
						NewStatus = FText::Format(NSLOCTEXT("RpgCrafting", "OrderWaitingSpace", "Waiting: {0} full"), TargetName);
						NewHint = OutputDefinition && URpgInventoryManagerComponent::GetEffectiveMaxStackSizeForDefinition(OutputDefinition) <= 1 && Footprint.IsValid()
							? FText::Format(NSLOCTEXT("RpgCrafting", "OrderWaitingSpaceItemHint", "The next {0} needs {1} free cells. Empty the chest or change the target."),
								UnitSingular, MakeCellsText(Footprint))
							: NSLOCTEXT("RpgCrafting", "OrderWaitingSpaceHint", "Make room in the chest or change the target.");
					}
				}
				break;
			case ERpgCraftingOrderState::WaitingForTarget:
				bNewWaiting = true;
				if (bAutomaticOrder)
				{
					NewStatus = NSLOCTEXT("RpgCrafting", "OrderWaitingNoSuitableChest", "Waiting: no suitable chest");
					NewHint = FText::Format(
						NSLOCTEXT("RpgCrafting", "OrderWaitingNoSuitableChestHint", "Assign a connected chest to {0}, leave one unassigned, or choose a chest."),
						Recipe->OutputItems.IsEmpty() ? FText::GetEmpty() : GetItemDisplayName(Recipe->OutputItems[0].ItemDefinition));
				}
				else
				{
					NewStatus = NSLOCTEXT("RpgCrafting", "OrderWaitingTarget", "Waiting: target storage missing");
					NewHint = NSLOCTEXT("RpgCrafting", "OrderWaitingTargetHint", "Choose a connected chest as target.");
				}
				break;
			}
		}
	}

	FFieldChanges Changes;
	RPG_CRAFTING_SET(Changes, OrderLabelText, NewLabel);
	RPG_CRAFTING_SET(Changes, OrderTitleText, NewTitle);
	RPG_CRAFTING_SET(Changes, OrderCountsText, NewCounts);
	RPG_CRAFTING_SET(Changes, OrderStatusText, NewStatus);
	RPG_CRAFTING_SET(Changes, OrderHintText, NewHint);
	RPG_CRAFTING_SET(Changes, OrderProgress, NewProgress);
	RPG_CRAFTING_SET(Changes, bOrderWaiting, bNewWaiting);
	BroadcastFields(Changes);
}

void URpgCraftingStationViewModel::RebuildActionAvailability()
{
	URpgCraftingStationComponent* Station = ObservedStation.Get();
	AActor* Actor = RequestingActor.Get();
	const bool bHasSelection = Station && SelectedRecipe;
	const int32 MaxOrderQuantity = Station ? Station->GetMaxOrderQuantity() : 1;
	const bool bAccess = Station && Actor && Station->CanActorAccess(Actor);

	FFieldChanges Changes;
	RPG_CRAFTING_SET(Changes, bCanDecreaseCraftQuantity, bHasSelection && CraftQuantity > 1);
	RPG_CRAFTING_SET(Changes, bCanIncreaseCraftQuantity, bHasSelection && CraftQuantity < MaxOrderQuantity);
	RPG_CRAFTING_SET(Changes, bCanSetCraftQuantityToMax, bHasSelection && MaxSelectedCraftQuantity >= 1 && CraftQuantity != MaxSelectedCraftQuantity);
	RPG_CRAFTING_SET(Changes, bCanToggleCraftingPause, bAccess && bHasActiveOrder);
	RPG_CRAFTING_SET(Changes, bCanStopOrder, bAccess && bHasActiveOrder);
	BroadcastFields(Changes);
}

void URpgCraftingStationViewModel::HandleCraftingStationChanged(FGameplayTag Channel, const FRpgCraftingStationChangeMessage& Message)
{
	if (ObservedStation.Get() == Message.Station)
	{
		RequestRefresh(CraftingRefreshDomains::All);
	}
}

void URpgCraftingStationViewModel::HandleRecipeUnlockChanged(FGameplayTag Channel, const FRpgRecipeUnlockChangeMessage& Message)
{
	RequestRefresh(CraftingRefreshDomains::RecipesAndDetails);
}

void URpgCraftingStationViewModel::HandleInventoryChanged(FGameplayTag Channel, const FRpgInventoryChangeMessage& Message)
{
	const URpgInventoryManagerComponent* ChangedInventory = Cast<URpgInventoryManagerComponent>(Message.InventoryOwner.Get());
	if (!ObservedStation || !ChangedInventory)
	{
		return;
	}
	if (ConnectedStorage.ContainsByPredicate([ChangedInventory](const TWeakObjectPtr<URpgInventoryManagerComponent>& Inventory)
	{
		return Inventory.Get() == ChangedInventory;
	}))
	{
		RequestRefresh(CraftingRefreshDomains::RecipesAndDetails | CraftingRefreshDomains::Order);
	}
}

void URpgCraftingStationViewModel::HandleWorldKnowledgeChanged(FGameplayTag KnowledgeTag, bool bIsKnown)
{
	(void)KnowledgeTag;
	(void)bIsKnown;
	RequestRefresh(CraftingRefreshDomains::RecipesAndDetails);
}

#undef RPG_CRAFTING_SET
