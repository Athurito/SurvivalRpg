#if WITH_DEV_AUTOMATION_TESTS

#include "SurvivalRpg/Core/Character/RpgPawnData.h"
#include "SurvivalRpg/Core/Game/Experience/RpgExperienceDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ItemTraits.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Inventory/RpgPlayerInventoryLayoutComponent.h"
#include "SurvivalRpg/Inventory/RpgPlayerInventoryLayoutDefinition.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Blueprint/BlueprintSupport.h"
#include "Engine/Blueprint.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AssetRegistryInterface.h"
#include "Misc/AutomationTest.h"
#include "Misc/DataValidation.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

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

	TArray<FName> GetProjectContentRoots()
	{
		TArray<FName> ContentRoots = { FName(TEXT("/Game")) };
		for (const TSharedRef<IPlugin>& Plugin :
			IPluginManager::Get().GetEnabledPluginsWithContent())
		{
			if (!Plugin->IsMounted() ||
				Plugin->GetType() != EPluginType::Project)
			{
				continue;
			}

			FString MountedAssetPath = Plugin->GetMountedAssetPath();
			MountedAssetPath.RemoveFromEnd(TEXT("/"));
			if (!MountedAssetPath.IsEmpty())
			{
				ContentRoots.AddUnique(FName(*MountedAssetPath));
			}
		}
		return ContentRoots;
	}

	FRpgInventorySlotGroupDefinition MakeValidationGroup(
		FName ContainerId,
		ERpgInventorySlotGroupKind GroupKind,
		ERpgEquipmentSlot EquipmentSlotRole,
		FGameplayTag SemanticRole = FGameplayTag(),
		bool bActionbarBindable = false)
	{
		FRpgInventorySlotGroupDefinition Group;
		Group.ContainerId = ContainerId;
		Group.SemanticRole = SemanticRole;
		Group.DisplayName = FText::FromName(ContainerId);
		Group.GroupKind = GroupKind;
		Group.EquipmentSlotRole = EquipmentSlotRole;
		Group.GridSize.Width = 1;
		Group.GridSize.Height = 1;
		Group.Rule.bActionbarBindable = bActionbarBindable;
		return Group;
	}

	URpgPlayerInventoryLayoutDefinition* MakeValidTransientLayout()
	{
		URpgPlayerInventoryLayoutDefinition* Layout =
			NewObject<URpgPlayerInventoryLayoutDefinition>(
				GetTransientPackage(),
				NAME_None,
				RF_Transient);
		if (!Layout)
		{
			return nullptr;
		}

		FRpgInventorySlotGroupDefinition Content = MakeValidationGroup(
			TEXT("Content.Root"),
			ERpgInventorySlotGroupKind::Content,
			ERpgEquipmentSlot::None,
			FGameplayTag::RequestGameplayTag(
				TEXT("Rpg.Inventory.Layout.Role.Content.Primary")));
		Content.GridSize.Width = 4;
		Content.GridSize.Height = 3;

		Layout->StaticSlotGroups =
		{
			Content,
			MakeValidationGroup(
				TEXT("Carry.Primary"),
				ERpgInventorySlotGroupKind::Carry,
				ERpgEquipmentSlot::MainHand,
				FGameplayTag::RequestGameplayTag(
					TEXT("Rpg.Inventory.Layout.Role.Carry.Primary")),
				true),
			MakeValidationGroup(
				TEXT("Carry.Secondary"),
				ERpgInventorySlotGroupKind::Carry,
				ERpgEquipmentSlot::MainHand,
				FGameplayTag::RequestGameplayTag(
					TEXT("Rpg.Inventory.Layout.Role.Carry.Secondary")),
				true),
			MakeValidationGroup(
				TEXT("Gear.Head"),
				ERpgInventorySlotGroupKind::Gear,
				ERpgEquipmentSlot::Head)
		};
		return Layout;
	}

	struct FLayoutDataValidationSnapshot
	{
		EDataValidationResult Result = EDataValidationResult::NotValidated;
		TArray<FString> Errors;
	};

	FLayoutDataValidationSnapshot ValidateLayoutForTest(
		const URpgPlayerInventoryLayoutDefinition& Layout)
	{
		FLayoutDataValidationSnapshot Snapshot;
		FDataValidationContext Context;
		Snapshot.Result = Layout.IsDataValid(Context);
		for (const FDataValidationContext::FIssue& Issue :
			Context.GetIssues())
		{
			if (Issue.Severity == EMessageSeverity::Error)
			{
				Snapshot.Errors.Add(Issue.Message.ToString());
			}
		}
		return Snapshot;
	}

	bool ContainsValidationError(
		const FLayoutDataValidationSnapshot& Snapshot,
		const TCHAR* ExpectedFragment)
	{
		return Snapshot.Errors.ContainsByPredicate(
			[ExpectedFragment](const FString& Error)
			{
				return Error.Contains(ExpectedFragment);
			});
	}
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
	const FLayoutDataValidationSnapshot LayoutValidation =
		ValidateLayoutForTest(*Layout);
	TestEqual(
		TEXT("The real default player inventory layout passes native data validation"),
		LayoutValidation.Result,
		EDataValidationResult::Valid);
	TestEqual(
		TEXT("The real default player inventory layout produces no validation errors"),
		LayoutValidation.Errors.Num(),
		0);

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
	const TArray<FGameplayTag> ExpectedSemanticRoles =
	{
		FGameplayTag(),
		FGameplayTag(),
		FGameplayTag(),
		FGameplayTag(),
		FGameplayTag(),
		FGameplayTag(),
		FGameplayTag(),
		FGameplayTag(),
		FGameplayTag(),
		FGameplayTag::RequestGameplayTag(TEXT("Rpg.Inventory.Layout.Role.Carry.Primary")),
		FGameplayTag::RequestGameplayTag(TEXT("Rpg.Inventory.Layout.Role.Carry.Secondary")),
		FGameplayTag::RequestGameplayTag(TEXT("Rpg.Inventory.Layout.Role.Carry.OffHand")),
		FGameplayTag::RequestGameplayTag(TEXT("Rpg.Inventory.Layout.Role.Content.Primary"))
	};
	const TArray<ERpgEquipmentSlot> ExpectedEquipmentSlotRoles =
	{
		ERpgEquipmentSlot::Head,
		ERpgEquipmentSlot::Chest,
		ERpgEquipmentSlot::Hands,
		ERpgEquipmentSlot::Legs,
		ERpgEquipmentSlot::Feet,
		ERpgEquipmentSlot::Backpack,
		ERpgEquipmentSlot::Belt,
		ERpgEquipmentSlot::Pouch,
		ERpgEquipmentSlot::ResourceBag,
		ERpgEquipmentSlot::MainHand,
		ERpgEquipmentSlot::MainHand,
		ERpgEquipmentSlot::OffHand,
		ERpgEquipmentSlot::None
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
	TSet<FGameplayTag> AuthoredSemanticRoles;
	for (int32 Index = 0; Index < Groups.Num(); ++Index)
	{
		const FRpgInventorySlotGroupDefinition& Group = Groups[Index];
		TestEqual(
			FString::Printf(TEXT("Group %d keeps its authored container id"), Index),
			Group.ContainerId,
			ExpectedContainerIds[Index]);
		TestEqual(
			FString::Printf(TEXT("Group %d keeps its explicit semantic role"), Index),
			Group.SemanticRole,
			ExpectedSemanticRoles[Index]);
		TestEqual(
			FString::Printf(TEXT("Group %d keeps its explicit equipment-slot role"), Index),
			Group.EquipmentSlotRole,
			ExpectedEquipmentSlotRoles[Index]);
		if (Group.SemanticRole.IsValid())
		{
			TestFalse(
				FString::Printf(TEXT("Semantic role %s is unique"), *Group.SemanticRole.ToString()),
				AuthoredSemanticRoles.Contains(Group.SemanticRole));
			AuthoredSemanticRoles.Add(Group.SemanticRole);
		}
		TotalCellCount += Group.GridSize.Width * Group.GridSize.Height;
	}
	TestEqual(
		TEXT("The default layout exposes exactly four authored singleton roles"),
		AuthoredSemanticRoles.Num(),
		4);
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
		ERpgEquipmentSlot ExpectedEquipmentSlotRole)
	{
		TestTrue(
			FString::Printf(TEXT("%s remains a carry group"), Description),
			Group.GroupKind == ERpgInventorySlotGroupKind::Carry);
		TestTrue(
			FString::Printf(TEXT("%s remains actionbar bindable"), Description),
			Group.Rule.bActionbarBindable);
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
		TestEqual(
			FString::Printf(TEXT("%s keeps its authored equipment-slot role"), Description),
			Group.EquipmentSlotRole,
			ExpectedEquipmentSlotRole);
	};
	TestCarryGroup(
		TEXT("WeaponSlot1"),
		Groups[9],
		ERpgInventoryItemCategory::Weapon,
		ERpgEquipmentSlot::MainHand);
	TestCarryGroup(
		TEXT("WeaponSlot2"),
		Groups[10],
		ERpgInventoryItemCategory::Weapon,
		ERpgEquipmentSlot::MainHand);
	TestCarryGroup(
		TEXT("ShieldSlot"),
		Groups[11],
		ERpgInventoryItemCategory::Shield,
		ERpgEquipmentSlot::OffHand);

	TestTrue(
		TEXT("Pockets remains a content group"),
		Pockets.GroupKind == ERpgInventorySlotGroupKind::Content);
	TestTrue(
		TEXT("Pockets remains actionbar bindable"),
		Pockets.Rule.bActionbarBindable);
	TestEqual(
		TEXT("Pockets has no equipment-slot role"),
		Pockets.EquipmentSlotRole,
		ERpgEquipmentSlot::None);
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
		TEXT("The slot rule no longer exposes the redundant bCarrySlot property"),
		FRpgInventorySlotRule::StaticStruct()->FindPropertyByName(
			TEXT("bCarrySlot")));
	TestNull(
		TEXT("The slot rule no longer exposes the gameplay-tag CarryActivationRole property"),
		FRpgInventorySlotRule::StaticStruct()->FindPropertyByName(
			TEXT("CarryActivationRole")));

	const FEnumProperty* DefinitionEquipmentSlotRoleProperty =
		FindFProperty<FEnumProperty>(
			FRpgInventorySlotGroupDefinition::StaticStruct(),
			GET_MEMBER_NAME_CHECKED(
				FRpgInventorySlotGroupDefinition,
				EquipmentSlotRole));
	if (TestNotNull(
			TEXT("Static group definitions expose EquipmentSlotRole as an enum property"),
			DefinitionEquipmentSlotRoleProperty))
	{
		TestEqual(
			TEXT("Definition EquipmentSlotRole uses ERpgEquipmentSlot"),
			DefinitionEquipmentSlotRoleProperty->GetEnum(),
			StaticEnum<ERpgEquipmentSlot>());
	}

	const FEnumProperty* ViewEquipmentSlotRoleProperty =
		FindFProperty<FEnumProperty>(
			FRpgInventorySlotGroupView::StaticStruct(),
			GET_MEMBER_NAME_CHECKED(
				FRpgInventorySlotGroupView,
				EquipmentSlotRole));
	if (TestNotNull(
			TEXT("Runtime group views expose EquipmentSlotRole as an enum property"),
			ViewEquipmentSlotRoleProperty))
	{
		TestEqual(
			TEXT("View EquipmentSlotRole uses ERpgEquipmentSlot"),
			ViewEquipmentSlotRoleProperty->GetEnum(),
			StaticEnum<ERpgEquipmentSlot>());
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgPlayerInventoryLayoutDataValidationTest,
	"SurvivalRpg.Inventory.Layout.DataValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgPlayerInventoryLayoutDataValidationTest::RunTest(
	const FString& Parameters)
{
	URpgPlayerInventoryLayoutDefinition* ValidLayout =
		MakeValidTransientLayout();
	if (!TestNotNull(
			TEXT("A valid transient player inventory layout can be created"),
			ValidLayout))
	{
		return false;
	}

	const FRpgInventoryStaticLayoutValidationResult ValidContract =
		ValidLayout->ValidateStaticSlotGroups();
	TestTrue(
		TEXT("Two distinct Carry roots may share the typed MainHand role"),
		ValidContract.IsValid());
	TestTrue(
		TEXT("The valid layout also passes the physical equipment preflight"),
		ValidContract.PassesPhysicalEquipmentPreflight());
	const FLayoutDataValidationSnapshot ValidSnapshot =
		ValidateLayoutForTest(*ValidLayout);
	TestEqual(
		TEXT("The shared valid contract also passes UObject data validation"),
		ValidSnapshot.Result,
		EDataValidationResult::Valid);
	TestEqual(
		TEXT("The valid transient layout has no editor diagnostics"),
		ValidSnapshot.Errors.Num(),
		0);

	auto TestSingleGroupFailure =
		[this](
			const TCHAR* Description,
			TFunctionRef<void(URpgPlayerInventoryLayoutDefinition&)> Mutate,
			ERpgInventoryStaticLayoutValidationIssue ExpectedIssue,
			int32 ExpectedGroupIndex,
			bool bExpectedToMaterialize,
			const TCHAR* ExpectedMessageFragment)
		{
			URpgPlayerInventoryLayoutDefinition* Layout =
				MakeValidTransientLayout();
			if (!TestNotNull(
				*FString::Printf(
					TEXT("%s creates a transient layout"),
					Description),
				Layout))
			{
				return;
			}

			Mutate(*Layout);
			const FRpgInventoryStaticLayoutValidationResult Contract =
				Layout->ValidateStaticSlotGroups();
			TestFalse(
				*FString::Printf(
					TEXT("%s invalidates the shared layout contract"),
					Description),
				Contract.IsValid());
			const bool bIsSemanticOnlyFailure =
				ExpectedIssue ==
				ERpgInventoryStaticLayoutValidationIssue::
					MissingActionbarCarrySemanticRole ||
				ExpectedIssue ==
					ERpgInventoryStaticLayoutValidationIssue::
						SemanticRoleOutsideLayoutNamespace;
			TestEqual(
				*FString::Printf(
					TEXT("%s keeps the physical equipment preflight appropriately scoped"),
					Description),
				Contract.PassesPhysicalEquipmentPreflight(),
				bIsSemanticOnlyFailure);
			TestTrue(
				*FString::Printf(
					TEXT("%s reports its exact rule"),
					Description),
				Contract.HasIssue(ExpectedIssue, ExpectedGroupIndex));
			TestEqual(
				*FString::Printf(
					TEXT("%s exposes the expected runtime materialization state"),
					Description),
				Contract.CanMaterializeGroup(ExpectedGroupIndex),
				bExpectedToMaterialize);

			const FLayoutDataValidationSnapshot Snapshot =
				ValidateLayoutForTest(*Layout);
			TestEqual(
				*FString::Printf(
					TEXT("%s is rejected by IsDataValid"),
					Description),
				Snapshot.Result,
				EDataValidationResult::Invalid);
			TestTrue(
				*FString::Printf(
					TEXT("%s emits an actionable editor diagnostic"),
					Description),
				ContainsValidationError(
					Snapshot,
					ExpectedMessageFragment));
		};

	TestSingleGroupFailure(
		TEXT("A missing ContainerId"),
		[](URpgPlayerInventoryLayoutDefinition& Layout)
		{
			Layout.StaticSlotGroups[0].ContainerId = NAME_None;
		},
		ERpgInventoryStaticLayoutValidationIssue::MissingContainerId,
		0,
		false,
		TEXT("has no ContainerId"));

	TestSingleGroupFailure(
		TEXT("A non-positive grid footprint"),
		[](URpgPlayerInventoryLayoutDefinition& Layout)
		{
			Layout.StaticSlotGroups[0].GridSize.Width = 0;
		},
		ERpgInventoryStaticLayoutValidationIssue::InvalidGridSize,
		0,
		false,
		TEXT("Width and height must both be at least one cell"));

	TestSingleGroupFailure(
		TEXT("An unknown GroupKind"),
		[](URpgPlayerInventoryLayoutDefinition& Layout)
		{
			Layout.StaticSlotGroups[0].GroupKind =
				static_cast<ERpgInventorySlotGroupKind>(255);
		},
		ERpgInventoryStaticLayoutValidationIssue::InvalidGroupKind,
		0,
		false,
		TEXT("has an unknown GroupKind"));

	TestSingleGroupFailure(
		TEXT("A Content group with an equipment role"),
		[](URpgPlayerInventoryLayoutDefinition& Layout)
		{
			Layout.StaticSlotGroups[0].EquipmentSlotRole =
				ERpgEquipmentSlot::MainHand;
		},
		ERpgInventoryStaticLayoutValidationIssue::
			ContentHasEquipmentSlotRole,
		0,
		false,
		TEXT("Content groups must use None"));

	TestSingleGroupFailure(
		TEXT("A Carry group with a non-hand equipment role"),
		[](URpgPlayerInventoryLayoutDefinition& Layout)
		{
			Layout.StaticSlotGroups[1].EquipmentSlotRole =
				ERpgEquipmentSlot::Chest;
		},
		ERpgInventoryStaticLayoutValidationIssue::
			CarryHasInvalidEquipmentSlotRole,
		1,
		false,
		TEXT("Carry groups must use MainHand or OffHand"));

	TestSingleGroupFailure(
		TEXT("A Gear group with a hand equipment role"),
		[](URpgPlayerInventoryLayoutDefinition& Layout)
		{
			Layout.StaticSlotGroups[3].EquipmentSlotRole =
				ERpgEquipmentSlot::MainHand;
		},
		ERpgInventoryStaticLayoutValidationIssue::
			GearHasInvalidEquipmentSlotRole,
		3,
		false,
		TEXT("Gear groups require one managed non-hand slot"));

	TestSingleGroupFailure(
		TEXT("A multi-cell Carry group"),
		[](URpgPlayerInventoryLayoutDefinition& Layout)
		{
			Layout.StaticSlotGroups[1].GridSize.Width = 2;
		},
		ERpgInventoryStaticLayoutValidationIssue::
			EquipmentGroupIsNotSingleCell,
		1,
		false,
		TEXT("Equipment groups must be exactly 1 x 1"));

	TestSingleGroupFailure(
		TEXT("An actionbar Carry group without a semantic role"),
		[](URpgPlayerInventoryLayoutDefinition& Layout)
		{
			Layout.StaticSlotGroups[1].SemanticRole = FGameplayTag();
		},
		ERpgInventoryStaticLayoutValidationIssue::
			MissingActionbarCarrySemanticRole,
		1,
		true,
		TEXT("without a SemanticRole"));

	TestSingleGroupFailure(
		TEXT("A valid semantic role outside the layout namespace"),
		[](URpgPlayerInventoryLayoutDefinition& Layout)
		{
			Layout.StaticSlotGroups[0].SemanticRole =
				FGameplayTag::RequestGameplayTag(
					TEXT("UI.Screen.Inventory"));
		},
		ERpgInventoryStaticLayoutValidationIssue::
			SemanticRoleOutsideLayoutNamespace,
		0,
		true,
		TEXT("outside the Rpg.Inventory.Layout.Role namespace"));

	{
		URpgPlayerInventoryLayoutDefinition* Layout =
			MakeValidTransientLayout();
		Layout->StaticSlotGroups[3].ContainerId =
			Layout->StaticSlotGroups[0].ContainerId;
		const FRpgInventoryStaticLayoutValidationResult Contract =
			Layout->ValidateStaticSlotGroups();
		TestFalse(
			TEXT("Duplicate ContainerIds invalidate the shared contract"),
			Contract.IsValid());
		TestFalse(
			TEXT("Duplicate ContainerIds fail the physical equipment preflight"),
			Contract.PassesPhysicalEquipmentPreflight());
		TestTrue(
			TEXT("The duplicate ContainerId is diagnosed"),
			Contract.HasIssue(
				ERpgInventoryStaticLayoutValidationIssue::
					DuplicateContainerId,
				3));
		TestFalse(
			TEXT("The first colliding physical root is filtered"),
			Contract.CanMaterializeGroup(0));
		TestFalse(
			TEXT("The second colliding physical root is filtered"),
			Contract.CanMaterializeGroup(3));
		const FLayoutDataValidationSnapshot Snapshot =
			ValidateLayoutForTest(*Layout);
		TestTrue(
			TEXT("The duplicate ContainerId diagnostic identifies its earlier entry"),
			ContainsValidationError(
				Snapshot,
				TEXT("from StaticSlotGroups[0]")));
	}

	{
		URpgPlayerInventoryLayoutDefinition* Layout =
			MakeValidTransientLayout();
		Layout->StaticSlotGroups[3].SemanticRole =
			Layout->StaticSlotGroups[0].SemanticRole;
		const FRpgInventoryStaticLayoutValidationResult Contract =
			Layout->ValidateStaticSlotGroups();
		TestFalse(
			TEXT("Duplicate semantic roles invalidate the shared contract"),
			Contract.IsValid());
		TestTrue(
			TEXT("Duplicate semantic roles do not widen the physical equipment preflight"),
			Contract.PassesPhysicalEquipmentPreflight());
		TestTrue(
			TEXT("The duplicate semantic role is diagnosed"),
			Contract.HasIssue(
				ERpgInventoryStaticLayoutValidationIssue::
					DuplicateSemanticRole,
				3));
		TestTrue(
			TEXT("The first semantic collision remains visible to the unique runtime resolver"),
			Contract.CanMaterializeGroup(0));
		TestTrue(
			TEXT("The second semantic collision remains visible to the unique runtime resolver"),
			Contract.CanMaterializeGroup(3));
		const FLayoutDataValidationSnapshot Snapshot =
			ValidateLayoutForTest(*Layout);
		TestTrue(
			TEXT("The semantic-role diagnostic explains singleton scope"),
			ContainsValidationError(
				Snapshot,
				TEXT("layout-wide singleton keys")));
	}

	{
		URpgPlayerInventoryLayoutDefinition* Layout =
			MakeValidTransientLayout();
		Layout->StaticSlotGroups.Add(
			MakeValidationGroup(
				TEXT("Gear.Head.Alternate"),
				ERpgInventorySlotGroupKind::Gear,
				ERpgEquipmentSlot::Head));
		const int32 DuplicateGearIndex =
			Layout->StaticSlotGroups.Num() - 1;
		const FRpgInventoryStaticLayoutValidationResult Contract =
			Layout->ValidateStaticSlotGroups();
		TestFalse(
			TEXT("Duplicate Gear roles invalidate the shared contract"),
			Contract.IsValid());
		TestFalse(
			TEXT("Duplicate Gear roles fail the physical equipment preflight"),
			Contract.PassesPhysicalEquipmentPreflight());
		TestTrue(
			TEXT("The duplicate Gear role is diagnosed"),
			Contract.HasIssue(
				ERpgInventoryStaticLayoutValidationIssue::
					DuplicateGearEquipmentSlotRole,
				DuplicateGearIndex));
		TestTrue(
			TEXT("Both Gear-role candidates remain visible to the unique runtime resolver"),
			Contract.CanMaterializeGroup(DuplicateGearIndex));
		const FLayoutDataValidationSnapshot Snapshot =
			ValidateLayoutForTest(*Layout);
		TestTrue(
			TEXT("The duplicate Gear-role diagnostic explains the unique-root requirement"),
			ContainsValidationError(
				Snapshot,
				TEXT("exactly one static root")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgPlayerInventoryLayoutAuthoredAssetDataValidationTest,
	"SurvivalRpg.Inventory.Layout.AuthoredAssetsDataValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgPlayerInventoryLayoutAuthoredAssetDataValidationTest::RunTest(
	const FString& Parameters)
{
	IAssetRegistry& AssetRegistry =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
			TEXT("AssetRegistry")).Get();
	AssetRegistry.WaitForCompletion();

	FARFilter LayoutFilter;
	LayoutFilter.ClassPaths.Add(
		URpgPlayerInventoryLayoutDefinition::StaticClass()->GetClassPathName());
	LayoutFilter.bRecursiveClasses = true;
	LayoutFilter.bRecursivePaths = true;
	for (const FName ContentRoot : GetProjectContentRoots())
	{
		LayoutFilter.PackagePaths.Add(ContentRoot);
	}

	TArray<FAssetData> LayoutAssets;
	AssetRegistry.GetAssets(LayoutFilter, LayoutAssets);
	LayoutAssets.Sort(
		[](const FAssetData& Left, const FAssetData& Right)
		{
			return Left.PackageName.LexicalLess(Right.PackageName);
		});

	for (const FAssetData& LayoutAsset : LayoutAssets)
	{
		const URpgPlayerInventoryLayoutDefinition* Layout =
			Cast<URpgPlayerInventoryLayoutDefinition>(
				LayoutAsset.GetAsset());
		if (!Layout)
		{
			AddError(
				FString::Printf(
					TEXT("%s did not load as a player inventory layout definition"),
					*LayoutAsset.GetObjectPathString()));
			continue;
		}

		const FLayoutDataValidationSnapshot Snapshot =
			ValidateLayoutForTest(*Layout);
		if (Snapshot.Result != EDataValidationResult::Valid ||
			!Snapshot.Errors.IsEmpty())
		{
			AddError(
				FString::Printf(
					TEXT("%s failed native layout validation: %s"),
					*LayoutAsset.GetObjectPathString(),
					*FString::Join(Snapshot.Errors, TEXT("\n"))));
		}
	}

	TestTrue(
		TEXT("AssetRegistry found at least one authored player inventory layout definition"),
		!LayoutAssets.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryItemDefinitionExplicitSpatialFragmentAssetTest,
	"SurvivalRpg.Inventory.ItemDefinitions.ExplicitSpatialFragments",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgInventoryItemDefinitionExplicitSpatialFragmentAssetTest::RunTest(
	const FString& Parameters)
{
	IAssetRegistry& AssetRegistry =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
			TEXT("AssetRegistry")).Get();
	AssetRegistry.WaitForCompletion();

	TSet<FTopLevelAssetPath> DerivedItemDefinitionClassPaths;
	AssetRegistry.GetDerivedClassNames(
		{ URpgInventoryItemDefinition::StaticClass()->GetClassPathName() },
		TSet<FTopLevelAssetPath>(),
		DerivedItemDefinitionClassPaths);

	FARFilter BlueprintFilter;
	BlueprintFilter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
	BlueprintFilter.bRecursiveClasses = true;
	BlueprintFilter.bRecursivePaths = true;
	for (const FName ContentRoot : GetProjectContentRoots())
	{
		BlueprintFilter.PackagePaths.Add(ContentRoot);
	}

	TArray<FAssetData> BlueprintAssets;
	AssetRegistry.GetAssets(BlueprintFilter, BlueprintAssets);
	BlueprintAssets.Sort(
		[](const FAssetData& Left, const FAssetData& Right)
		{
			return Left.PackageName.LexicalLess(Right.PackageName);
		});

	int32 ConcreteItemDefinitionCount = 0;
	for (const FAssetData& BlueprintAsset : BlueprintAssets)
	{
		const FString GeneratedClassExportPath =
			BlueprintAsset.GetTagValueRef<FString>(
				FBlueprintTags::GeneratedClassPath);
		if (GeneratedClassExportPath.IsEmpty())
		{
			continue;
		}

		const FTopLevelAssetPath GeneratedClassPath(
			FPackageName::ExportTextPathToObjectPath(
				GeneratedClassExportPath));
		if (!DerivedItemDefinitionClassPaths.Contains(GeneratedClassPath))
		{
			continue;
		}

		UClass* GeneratedClass = LoadObject<UClass>(
			nullptr,
			*GeneratedClassPath.ToString());
		if (!TestNotNull(
			*FString::Printf(
				TEXT("%s generated class loads"),
				*BlueprintAsset.GetObjectPathString()),
			GeneratedClass))
		{
			continue;
		}
		if (GeneratedClass->HasAnyClassFlags(
			CLASS_Abstract |
			CLASS_Deprecated |
			CLASS_NewerVersionExists))
		{
			continue;
		}

		const URpgInventoryItemDefinition* ItemDefinition =
			Cast<URpgInventoryItemDefinition>(
				GeneratedClass->GetDefaultObject());
		if (!TestNotNull(
			*FString::Printf(
				TEXT("%s generated defaults are an item definition"),
				*BlueprintAsset.GetObjectPathString()),
			ItemDefinition))
		{
			continue;
		}

		++ConcreteItemDefinitionCount;
		TArray<const URpgInventoryFragment_SpatialItem*> SpatialFragments;
		for (const URpgInventoryItemFragment* Fragment :
			ItemDefinition->Fragments)
		{
			if (const URpgInventoryFragment_SpatialItem* SpatialFragment =
				Cast<URpgInventoryFragment_SpatialItem>(Fragment))
			{
				SpatialFragments.Add(SpatialFragment);
			}
		}

		if (!TestEqual(
			*FString::Printf(
				TEXT("%s has exactly one explicit Spatial fragment"),
				*BlueprintAsset.GetObjectPathString()),
			SpatialFragments.Num(),
			1))
		{
			continue;
		}

		TestTrue(
			*FString::Printf(
				TEXT("%s has a valid positive Spatial footprint"),
				*BlueprintAsset.GetObjectPathString()),
			SpatialFragments[0]->Footprint.IsValid());
	}

	TestTrue(
		TEXT("AssetRegistry found at least one concrete Blueprint item definition"),
		ConcreteItemDefinitionCount > 0);
	return true;
}

#endif
