#include "RpgRuneAutomationTestTypes.h"

#include "NativeGameplayTags.h"
#include "RpgInventoryFragment_Rune.h"
#include "SurvivalRpg/Inventory/RpgInventoryAutomationTestTypes.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_EquippableItem.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemContainer.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_SkillTree.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemTraits.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgRuneAutomationTestTypes)

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_RuneTest_A, "Rune.AutomationTest.A");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_RuneTest_B, "Rune.AutomationTest.B");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_RuneTest_RankI, "Rune.AutomationTest.Rank.I");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_RuneTest_RankII, "Rune.AutomationTest.Rank.II");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_RuneTest_RankIII, "Rune.AutomationTest.Rank.III");

namespace RpgRuneAutomationTestTypes
{
	void AddSpatialAndTraits(
		URpgInventoryItemDefinition& Definition,
		const ERpgInventoryItemCategory Category,
		TArray<TObjectPtr<URpgInventoryItemFragment>>& OutFragments)
	{
		URpgInventoryFragment_SpatialItem* Spatial =
			Definition.CreateDefaultSubobject<URpgInventoryFragment_SpatialItem>(TEXT("Spatial"));
		Spatial->Footprint.Width = 1;
		Spatial->Footprint.Height = 1;
		OutFragments.Add(Spatial);

		URpgInventoryFragment_ItemTraits* Traits =
			Definition.CreateDefaultSubobject<URpgInventoryFragment_ItemTraits>(TEXT("Traits"));
		Traits->ItemCategory = Category;
		Traits->bCanStack = false;
		Traits->MaxStackSize = 1;
		OutFragments.Add(Traits);
	}

	void AddSocketGrid(URpgInventoryItemDefinition& Definition, TArray<TObjectPtr<URpgInventoryItemFragment>>& OutFragments)
	{
		URpgInventoryFragment_ItemContainer* Container =
			Definition.CreateDefaultSubobject<URpgInventoryFragment_ItemContainer>(TEXT("ItemContainer"));
		FRpgInventoryItemContainerDefinition& Sockets = Container->ProvidedContainers.AddDefaulted_GetRef();
		Sockets.ContainerId = RpgRuneAutomationTestSocketGridId;
		Sockets.DisplayName = FText::FromString(TEXT("Rune sockets"));
		Sockets.GridSize.Width = 3;
		Sockets.GridSize.Height = 1;
		Sockets.AllowedCategories.Add(ERpgInventoryItemCategory::Rune);
		Sockets.bAllowNestedContainers = false;
		Sockets.bSocketGrid = true;
		OutFragments.Add(Container);
	}

	URpgInventoryFragment_Rune* FindRune(const URpgInventoryItemDefinition* Definition)
	{
		return Definition ? const_cast<URpgInventoryFragment_Rune*>(Cast<URpgInventoryFragment_Rune>(
			Definition->FindFragmentByClass(URpgInventoryFragment_Rune::StaticClass()))) : nullptr;
	}
}

URpgRuneAutomationTestToolDefinition::URpgRuneAutomationTestToolDefinition(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = FText::FromString(TEXT("Automation Socketed Tool"));
	RpgRuneAutomationTestTypes::AddSpatialAndTraits(*this, ERpgInventoryItemCategory::Tool, Fragments);
	RpgRuneAutomationTestTypes::AddSocketGrid(*this, Fragments);
}

URpgRuneAutomationTestTreeToolDefinition::URpgRuneAutomationTestTreeToolDefinition(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = FText::FromString(TEXT("Automation Socketed Tree Tool"));
	RpgRuneAutomationTestTypes::AddSpatialAndTraits(*this, ERpgInventoryItemCategory::Tool, Fragments);
	RpgRuneAutomationTestTypes::AddSocketGrid(*this, Fragments);
	Fragments.Add(CreateDefaultSubobject<URpgInventoryFragment_SkillTree>(TEXT("SkillTree")));
}

void URpgRuneAutomationTestTreeToolDefinition::SetTestSkillTree(const URpgSkillTreeDefinition* Tree)
{
	for (URpgInventoryItemFragment* Fragment : GetMutableDefault<URpgRuneAutomationTestTreeToolDefinition>()->Fragments)
	{
		if (URpgInventoryFragment_SkillTree* SkillTreeFragment = Cast<URpgInventoryFragment_SkillTree>(Fragment))
		{
			SkillTreeFragment->SkillTree = Tree;
		}
	}
}

URpgRuneAutomationTestSocketedWeaponDefinition::URpgRuneAutomationTestSocketedWeaponDefinition(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = FText::FromString(TEXT("Automation Socketed Weapon"));
	RpgRuneAutomationTestTypes::AddSpatialAndTraits(*this, ERpgInventoryItemCategory::Weapon, Fragments);

	URpgInventoryFragment_EquippableItem* Equippable =
		CreateDefaultSubobject<URpgInventoryFragment_EquippableItem>(TEXT("Equippable"));
	Equippable->EquipmentDefinition = URpgInventoryAutomationTestWeaponEquipmentDefinition::StaticClass();
	Fragments.Add(Equippable);

	RpgRuneAutomationTestTypes::AddSocketGrid(*this, Fragments);
}

URpgRuneAutomationTestBagDefinition::URpgRuneAutomationTestBagDefinition(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = FText::FromString(TEXT("Automation Bag"));
	RpgRuneAutomationTestTypes::AddSpatialAndTraits(*this, ERpgInventoryItemCategory::Misc, Fragments);

	URpgInventoryFragment_ItemContainer* Container =
		CreateDefaultSubobject<URpgInventoryFragment_ItemContainer>(TEXT("ItemContainer"));
	FRpgInventoryItemContainerDefinition& Main = Container->ProvidedContainers.AddDefaulted_GetRef();
	Main.ContainerId = TEXT("Main");
	Main.DisplayName = FText::FromString(TEXT("Main"));
	Main.GridSize.Width = 2;
	Main.GridSize.Height = 2;
	Main.bAllowNestedContainers = false;
	Fragments.Add(Container);
}

URpgRuneAutomationTestRuneDefinition::URpgRuneAutomationTestRuneDefinition(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = FText::FromString(TEXT("Automation Rune A"));
	RpgRuneAutomationTestTypes::AddSpatialAndTraits(*this, ERpgInventoryItemCategory::Rune, Fragments);

	URpgInventoryFragment_Rune* Rune = CreateDefaultSubobject<URpgInventoryFragment_Rune>(TEXT("Rune"));
	Rune->Ranks.SetNum(3);
	Fragments.Add(Rune);
}

void URpgRuneAutomationTestRuneDefinition::SetRankAbilitySet(const int32 Rank, const URpgAbilitySet* AbilitySet)
{
	URpgInventoryFragment_Rune* Rune =
		RpgRuneAutomationTestTypes::FindRune(GetMutableDefault<URpgRuneAutomationTestRuneDefinition>());
	if (Rune && Rune->Ranks.IsValidIndex(Rank - 1))
	{
		Rune->Ranks[Rank - 1].AbilitySet = AbilitySet;
	}
}

void URpgRuneAutomationTestRuneDefinition::SetBaseRecipe(const URpgCraftingRecipeDefinition* Recipe)
{
	if (URpgInventoryFragment_Rune* Rune =
			RpgRuneAutomationTestTypes::FindRune(GetMutableDefault<URpgRuneAutomationTestRuneDefinition>()))
	{
		Rune->BaseRecipe = Recipe;
	}
}

void URpgRuneAutomationTestRuneDefinition::SetRankWork(
	const int32 Rank,
	const TArray<FRpgCraftingResourceCost>& UpgradeCosts,
	const TArray<FRpgCraftingOutputItem>& SalvageOutputs)
{
	URpgInventoryFragment_Rune* Rune =
		RpgRuneAutomationTestTypes::FindRune(GetMutableDefault<URpgRuneAutomationTestRuneDefinition>());
	if (Rune && Rune->Ranks.IsValidIndex(Rank - 1))
	{
		Rune->Ranks[Rank - 1].UpgradeCosts = UpgradeCosts;
		Rune->Ranks[Rank - 1].SalvageOutputs = SalvageOutputs;
	}
}

void URpgRuneAutomationTestRuneDefinition::SetRankDescription(const int32 Rank, const FText& Description)
{
	URpgInventoryFragment_Rune* Rune =
		RpgRuneAutomationTestTypes::FindRune(GetMutableDefault<URpgRuneAutomationTestRuneDefinition>());
	if (Rune && Rune->Ranks.IsValidIndex(Rank - 1))
	{
		Rune->Ranks[Rank - 1].Description = Description;
	}
}

URpgRuneAutomationTestOtherRuneDefinition::URpgRuneAutomationTestOtherRuneDefinition(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = FText::FromString(TEXT("Automation Rune B"));
	RpgRuneAutomationTestTypes::AddSpatialAndTraits(*this, ERpgInventoryItemCategory::Rune, Fragments);

	URpgInventoryFragment_Rune* Rune = CreateDefaultSubobject<URpgInventoryFragment_Rune>(TEXT("Rune"));
	Rune->Ranks.SetNum(1);
	Fragments.Add(Rune);
}

void ConfigureRuneAutomationTestDefinitions()
{
	if (URpgInventoryFragment_Rune* RuneA =
			RpgRuneAutomationTestTypes::FindRune(GetMutableDefault<URpgRuneAutomationTestRuneDefinition>()))
	{
		RuneA->RuneTag = TAG_RuneTest_A;
		const FGameplayTag RankTags[] = {TAG_RuneTest_RankI, TAG_RuneTest_RankII, TAG_RuneTest_RankIII};
		for (int32 Index = 0; Index < RuneA->Ranks.Num() && Index < UE_ARRAY_COUNT(RankTags); ++Index)
		{
			RuneA->Ranks[Index].GrantedTags = FGameplayTagContainer(RankTags[Index]);
		}
	}
	if (URpgInventoryFragment_Rune* RuneB =
			RpgRuneAutomationTestTypes::FindRune(GetMutableDefault<URpgRuneAutomationTestOtherRuneDefinition>()))
	{
		RuneB->RuneTag = TAG_RuneTest_B;
	}
}
