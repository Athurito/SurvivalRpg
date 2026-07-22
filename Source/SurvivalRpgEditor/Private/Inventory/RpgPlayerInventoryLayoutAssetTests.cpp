#if WITH_DEV_AUTOMATION_TESTS

#include "SurvivalRpg/Core/Character/RpgPawnData.h"
#include "SurvivalRpg/Core/Game/Experience/RpgExperienceDefinition.h"
#include "SurvivalRpg/Inventory/RpgPlayerInventoryLayoutComponent.h"
#include "SurvivalRpg/Inventory/RpgPlayerInventoryLayoutDefinition.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/Blueprint.h"
#include "Misc/AssetRegistryInterface.h"
#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"

namespace
{
	constexpr TCHAR PlayerInventoryLayoutPath[] =
		TEXT("/Game/SurvivalRpg/Inventory/Layouts/DA_PlayerInventoryLayout_Default.DA_PlayerInventoryLayout_Default");
	constexpr TCHAR PlayerPawnDataPath[] =
		TEXT("/Game/SurvivalRpg/Core/Character/DA_PawnData.DA_PawnData");
	constexpr TCHAR PrototypeExperiencePath[] =
		TEXT("/Game/SurvivalRpg/System/Experiences/RpgPrototypeExperience.RpgPrototypeExperience");

	constexpr TCHAR PlayerInventoryLayoutPackage[] =
		TEXT("/Game/SurvivalRpg/Inventory/Layouts/DA_PlayerInventoryLayout_Default");
	constexpr TCHAR PlayerPawnDataPackage[] =
		TEXT("/Game/SurvivalRpg/Core/Character/DA_PawnData");
	constexpr TCHAR PrototypeExperiencePackage[] =
		TEXT("/Game/SurvivalRpg/System/Experiences/RpgPrototypeExperience");
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgPlayerInventoryLayoutAssetCompositionTest,
	"SurvivalRpg.Inventory.Layout.AssetComposition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgPlayerInventoryLayoutAssetCompositionTest::RunTest(const FString& Parameters)
{
	const URpgPlayerInventoryLayoutDefinition* Layout =
		LoadObject<URpgPlayerInventoryLayoutDefinition>(
			nullptr,
			PlayerInventoryLayoutPath);
	const URpgPawnData* PlayerPawnData =
		LoadObject<URpgPawnData>(nullptr, PlayerPawnDataPath);
	const UBlueprint* PrototypeExperienceBlueprint =
		LoadObject<UBlueprint>(nullptr, PrototypeExperiencePath);
	if (!TestNotNull(TEXT("The default player inventory layout loads"), Layout) ||
		!TestNotNull(TEXT("DA_PawnData loads"), PlayerPawnData) ||
		!TestNotNull(
			TEXT("RpgPrototypeExperience Blueprint loads"),
			PrototypeExperienceBlueprint))
	{
		return false;
	}

	TestTrue(
		TEXT("The default player inventory layout has the exact native definition class"),
		Layout->GetClass() ==
			URpgPlayerInventoryLayoutDefinition::StaticClass());

	const URpgExperienceDefinition* PrototypeExperience =
		PrototypeExperienceBlueprint->GeneratedClass
			? Cast<URpgExperienceDefinition>(
				PrototypeExperienceBlueprint->GeneratedClass->GetDefaultObject())
			: nullptr;
	if (!TestNotNull(
		TEXT("RpgPrototypeExperience generated defaults load"),
		PrototypeExperience))
	{
		return false;
	}

	TestTrue(
		TEXT("DA_PawnData owns the default player inventory layout definition"),
		PlayerPawnData->InventoryLayoutDefinition.Get() == Layout);
	TestTrue(
		TEXT("RpgPrototypeExperience owns DA_PawnData"),
		PrototypeExperience->DefaultPawnData.Get() == PlayerPawnData);

	const TArray<FName> ExpectedContainerIds =
	{
		TEXT("Gear.Head"),
		TEXT("Gear.Chest"),
		TEXT("Gear.Hands"),
		TEXT("Gear.Legs"),
		TEXT("Gear.Feet"),
		TEXT("Gear.Backpack"),
		TEXT("Gear.Belt"),
		TEXT("Gear.Pouch"),
		TEXT("Gear.ResourceBag"),
		TEXT("WeaponSlot1"),
		TEXT("WeaponSlot2"),
		TEXT("ShieldSlot"),
		TEXT("Pockets")
	};
	const TArray<FRpgInventorySlotGroupDefinition>& Groups =
		Layout->StaticSlotGroups;
	if (!TestEqual(
		TEXT("The layout has exactly thirteen static groups"),
		Groups.Num(),
		ExpectedContainerIds.Num()))
	{
		return false;
	}

	int32 TotalCellCount = 0;
	for (int32 Index = 0; Index < Groups.Num(); ++Index)
	{
		const FRpgInventorySlotGroupDefinition& Group = Groups[Index];
		TestEqual(
			FString::Printf(TEXT("Group %d keeps its authored container id"), Index),
			Group.ContainerId,
			ExpectedContainerIds[Index]);
		TotalCellCount += Group.GridSize.Width * Group.GridSize.Height;
	}
	TestEqual(
		TEXT("The static player layout exposes exactly 48 cells"),
		TotalCellCount,
		48);

	for (int32 Index = 0; Index < 12; ++Index)
	{
		TestEqual(
			FString::Printf(TEXT("Group %d remains one cell wide"), Index),
			Groups[Index].GridSize.Width,
			1);
		TestEqual(
			FString::Printf(TEXT("Group %d remains one cell high"), Index),
			Groups[Index].GridSize.Height,
			1);
	}
	const FRpgInventorySlotGroupDefinition& Pockets = Groups[12];
	TestEqual(TEXT("Pockets remains six cells wide"), Pockets.GridSize.Width, 6);
	TestEqual(TEXT("Pockets remains six cells high"), Pockets.GridSize.Height, 6);

	for (int32 Index = 0; Index < 9; ++Index)
	{
		const FRpgInventorySlotGroupDefinition& GearGroup = Groups[Index];
		TestTrue(
			FString::Printf(TEXT("%s remains a gear group"), *GearGroup.ContainerId.ToString()),
			GearGroup.GroupKind == ERpgInventorySlotGroupKind::Gear);
		TestFalse(
			FString::Printf(TEXT("%s is not actionbar bindable"), *GearGroup.ContainerId.ToString()),
			GearGroup.Rule.bActionbarBindable);
		TestFalse(
			FString::Printf(TEXT("%s is not a carry slot"), *GearGroup.ContainerId.ToString()),
			GearGroup.Rule.bCarrySlot);
		TestFalse(
			FString::Printf(TEXT("%s has no carry activation role"), *GearGroup.ContainerId.ToString()),
			GearGroup.Rule.CarryActivationRole.IsValid());
	}

	for (int32 Index = 0; Index < 5; ++Index)
	{
		const TArray<ERpgInventoryItemCategory>& Categories =
			Groups[Index].Rule.AllowedCategories;
		TestEqual(
			FString::Printf(TEXT("%s accepts exactly one broad category"), *Groups[Index].ContainerId.ToString()),
			Categories.Num(),
			1);
		if (Categories.Num() == 1)
		{
			TestTrue(
				FString::Printf(TEXT("%s accepts armor"), *Groups[Index].ContainerId.ToString()),
				Categories[0] == ERpgInventoryItemCategory::Armor);
		}
	}
	for (int32 Index = 5; Index < 9; ++Index)
	{
		TestTrue(
			FString::Printf(TEXT("%s keeps its unrestricted broad category rule"), *Groups[Index].ContainerId.ToString()),
			Groups[Index].Rule.AllowedCategories.IsEmpty());
	}

	auto TestCarryGroup = [this](
		const TCHAR* Description,
		const FRpgInventorySlotGroupDefinition& Group,
		ERpgInventoryItemCategory ExpectedCategory,
		const FGameplayTag& ExpectedRole)
	{
		TestTrue(
			FString::Printf(TEXT("%s remains a carry group"), Description),
			Group.GroupKind == ERpgInventorySlotGroupKind::Carry);
		TestTrue(
			FString::Printf(TEXT("%s remains actionbar bindable"), Description),
			Group.Rule.bActionbarBindable);
		TestTrue(
			FString::Printf(TEXT("%s remains a carry slot"), Description),
			Group.Rule.bCarrySlot);
		TestEqual(
			FString::Printf(TEXT("%s accepts exactly one broad category"), Description),
			Group.Rule.AllowedCategories.Num(),
			1);
		if (Group.Rule.AllowedCategories.Num() == 1)
		{
			TestTrue(
				FString::Printf(TEXT("%s keeps its authored item category"), Description),
				Group.Rule.AllowedCategories[0] == ExpectedCategory);
		}
		TestTrue(
			FString::Printf(TEXT("%s keeps its authored carry activation role"), Description),
			Group.Rule.CarryActivationRole == ExpectedRole);
	};
	TestCarryGroup(
		TEXT("WeaponSlot1"),
		Groups[9],
		ERpgInventoryItemCategory::Weapon,
		FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.MainHand")));
	TestCarryGroup(
		TEXT("WeaponSlot2"),
		Groups[10],
		ERpgInventoryItemCategory::Weapon,
		FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.MainHand")));
	TestCarryGroup(
		TEXT("ShieldSlot"),
		Groups[11],
		ERpgInventoryItemCategory::Shield,
		FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.OffHand")));

	TestTrue(
		TEXT("Pockets remains a content group"),
		Pockets.GroupKind == ERpgInventorySlotGroupKind::Content);
	TestTrue(
		TEXT("Pockets remains actionbar bindable"),
		Pockets.Rule.bActionbarBindable);
	TestFalse(TEXT("Pockets is not a carry slot"), Pockets.Rule.bCarrySlot);
	TestFalse(
		TEXT("Pockets has no carry activation role"),
		Pockets.Rule.CarryActivationRole.IsValid());
	TestTrue(
		TEXT("Pockets keeps its unrestricted broad category rule"),
		Pockets.Rule.AllowedCategories.IsEmpty());

	for (const FRpgInventorySlotGroupDefinition& Group : Groups)
	{
		TestTrue(
			FString::Printf(TEXT("%s has no required item-tag override"), *Group.ContainerId.ToString()),
			Group.Rule.RequiredItemTags.IsEmpty());
		TestTrue(
			FString::Printf(TEXT("%s has no blocked item-tag override"), *Group.ContainerId.ToString()),
			Group.Rule.BlockedItemTags.IsEmpty());
	}

	TestNull(
		TEXT("The runtime layout component no longer owns designer-authored StaticSlotGroups"),
		URpgPlayerInventoryLayoutComponent::StaticClass()->FindPropertyByName(
			TEXT("StaticSlotGroups")));

	IAssetRegistry& AssetRegistry =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
			TEXT("AssetRegistry")).Get();
	AssetRegistry.WaitForCompletion();
	UE::AssetRegistry::FDependencyQuery HardPackageQuery;
	HardPackageQuery.Required =
		UE::AssetRegistry::EDependencyProperty::Hard;
	TestTrue(
		TEXT("DA_PawnData has a hard package dependency on the player inventory layout"),
		AssetRegistry.ContainsDependency(
			FName(PlayerPawnDataPackage),
			FName(PlayerInventoryLayoutPackage),
			UE::AssetRegistry::EDependencyCategory::Package,
			HardPackageQuery));
	TestTrue(
		TEXT("RpgPrototypeExperience has a hard package dependency on DA_PawnData"),
		AssetRegistry.ContainsDependency(
			FName(PrototypeExperiencePackage),
			FName(PlayerPawnDataPackage),
			UE::AssetRegistry::EDependencyCategory::Package,
			HardPackageQuery));

	return true;
}

#endif
