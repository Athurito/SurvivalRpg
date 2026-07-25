#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "RpgInventoryAutomationTestTypes.h"
#include "RpgInventoryFragment_EquippableItem.h"
#include "RpgInventoryFragment_ItemContainer.h"
#include "RpgInventoryFragment_ItemTraits.h"
#include "SurvivalRpg/Equipment/RpgEquipmentAutomationTestTypes.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Blueprint/BlueprintSupport.h"
#include "Engine/Blueprint.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/DataValidation.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"

namespace
{
	FString CollectValidationErrors(
		const FDataValidationContext& Context)
	{
		TArray<FString> ErrorStrings;
		for (const FDataValidationContext::FIssue& Issue :
			Context.GetIssues())
		{
			if (Issue.Severity == EMessageSeverity::Error)
			{
				ErrorStrings.Add(Issue.Message.ToString());
			}
		}
		return FString::Join(ErrorStrings, TEXT("\n"));
	}

	FString CollectValidationWarnings(
		const FDataValidationContext& Context)
	{
		TArray<FString> WarningStrings;
		for (const FDataValidationContext::FIssue& Issue :
			Context.GetIssues())
		{
			if (Issue.Severity == EMessageSeverity::Warning)
			{
				WarningStrings.Add(Issue.Message.ToString());
			}
		}
		return FString::Join(WarningStrings, TEXT("\n"));
	}

	int32 CountValidationWarnings(
		const FDataValidationContext& Context)
	{
		int32 WarningCount = 0;
		for (const FDataValidationContext::FIssue& Issue :
			Context.GetIssues())
		{
			WarningCount +=
				Issue.Severity == EMessageSeverity::Warning ? 1 : 0;
		}
		return WarningCount;
	}

	bool ValidateAs(
		const UObject* Object,
		EDataValidationResult ExpectedResult,
		FDataValidationContext& OutContext)
	{
		return Object &&
			Object->IsDataValid(OutContext) == ExpectedResult;
	}

	TArray<FName> GetDefinitionContentRoots()
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
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryItemDefinitionSpatialDataValidationTest,
	"SurvivalRpg.Inventory.ItemDefinitions.DataValidation.SpatialContract",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryItemDefinitionSpatialDataValidationTest::RunTest(
	const FString& Parameters)
{
	URpgInventoryAutomationTestUnitItemDefinition* ValidDefinition =
		NewObject<URpgInventoryAutomationTestUnitItemDefinition>();
	FDataValidationContext ValidContext;
	TestTrue(
		TEXT("A definition with exactly one positive SpatialItem fragment validates"),
		ValidateAs(
			ValidDefinition,
			EDataValidationResult::Valid,
			ValidContext));
	TestEqual(
		TEXT("A valid SpatialItem contract emits no errors"),
		ValidContext.GetNumErrors(),
		0u);
	TestNotNull(
		TEXT("Editor validation accepts the same SpatialItem contract as runtime placement"),
		ValidDefinition->FindValidSpatialItemFragment());

	URpgInventoryAutomationTestMissingSpatialItemDefinition*
		MissingDefinition =
			NewObject<
				URpgInventoryAutomationTestMissingSpatialItemDefinition>();
	FDataValidationContext MissingContext;
	TestTrue(
		TEXT("A missing SpatialItem fragment is invalid"),
		ValidateAs(
			MissingDefinition,
			EDataValidationResult::Invalid,
			MissingContext));
	const FString MissingErrors =
		CollectValidationErrors(MissingContext);
	TestTrue(
		TEXT("The missing-fragment error names the affected object path"),
		MissingErrors.Contains(MissingDefinition->GetPathName()));
	TestTrue(
		TEXT("The missing-fragment error tells the designer to add exactly one SpatialItem fragment"),
		MissingErrors.Contains(TEXT("no SpatialItem fragment")) &&
			MissingErrors.Contains(TEXT("exactly one")));
	TestNull(
		TEXT("Runtime placement rejects the same missing SpatialItem contract"),
		MissingDefinition->FindValidSpatialItemFragment());

	URpgInventoryAutomationTestUnitItemDefinition* DuplicateDefinition =
		NewObject<URpgInventoryAutomationTestUnitItemDefinition>();
	URpgInventoryFragment_SpatialItem* DuplicateSpatial =
		NewObject<URpgInventoryFragment_SpatialItem>(
			DuplicateDefinition);
	DuplicateSpatial->Footprint.Width = 2;
	DuplicateSpatial->Footprint.Height = 1;
	DuplicateDefinition->Fragments.Add(DuplicateSpatial);
	FDataValidationContext DuplicateContext;
	TestTrue(
		TEXT("Duplicate SpatialItem fragments are invalid"),
		ValidateAs(
			DuplicateDefinition,
			EDataValidationResult::Invalid,
			DuplicateContext));
	const FString DuplicateErrors =
		CollectValidationErrors(DuplicateContext);
	TestTrue(
		TEXT("The duplicate-fragment error names the affected object path"),
		DuplicateErrors.Contains(
			DuplicateDefinition->GetPathName()));
	TestTrue(
		TEXT("The duplicate-fragment error reports count and fragment indices"),
		DuplicateErrors.Contains(TEXT("2 SpatialItem fragments")) &&
			DuplicateErrors.Contains(TEXT("[0, 2]")));
	TestNull(
		TEXT("Runtime placement rejects the same duplicate SpatialItem contract"),
		DuplicateDefinition->FindValidSpatialItemFragment());

	URpgInventoryAutomationTestUnitItemDefinition* InvalidDefinition =
		NewObject<URpgInventoryAutomationTestUnitItemDefinition>();
	URpgInventoryFragment_SpatialItem* InvalidSpatial =
		const_cast<URpgInventoryFragment_SpatialItem*>(
			InvalidDefinition->FindValidSpatialItemFragment());
	if (!TestNotNull(
		TEXT("The invalid-footprint fixture starts with a SpatialItem fragment"),
		InvalidSpatial))
	{
		return false;
	}
	InvalidSpatial->Footprint.Width = 0;
	InvalidSpatial->Footprint.Height = -2;
	FDataValidationContext InvalidContext;
	TestTrue(
		TEXT("A non-positive SpatialItem footprint is invalid"),
		ValidateAs(
			InvalidDefinition,
			EDataValidationResult::Invalid,
			InvalidContext));
	const FString InvalidErrors =
		CollectValidationErrors(InvalidContext);
	TestTrue(
		TEXT("The invalid-footprint error names the affected object path"),
		InvalidErrors.Contains(
			InvalidDefinition->GetPathName()));
	TestTrue(
		TEXT("The invalid-footprint error reports dimensions and fragment index"),
		InvalidErrors.Contains(TEXT("0 x -2")) &&
			InvalidErrors.Contains(TEXT("Fragments[0]")));
	TestNull(
		TEXT("Runtime placement rejects the same non-positive footprint"),
		InvalidDefinition->FindValidSpatialItemFragment());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryItemDefinitionEquippableDataValidationTest,
	"SurvivalRpg.Inventory.ItemDefinitions.DataValidation.EquippableReference",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryItemDefinitionEquippableDataValidationTest::RunTest(
	const FString& Parameters)
{
	URpgInventoryAutomationTestUnitItemDefinition* Definition =
		NewObject<URpgInventoryAutomationTestUnitItemDefinition>();
	URpgInventoryFragment_EquippableItem* EquippableFragment =
		NewObject<URpgInventoryFragment_EquippableItem>(Definition);
	Definition->Fragments.Add(EquippableFragment);

	FDataValidationContext MissingReferenceContext;
	TestTrue(
		TEXT("An EquippableItem fragment without EquipmentDefinition is invalid"),
		ValidateAs(
			Definition,
			EDataValidationResult::Invalid,
			MissingReferenceContext));
	const FString MissingReferenceErrors =
		CollectValidationErrors(MissingReferenceContext);
	TestTrue(
		TEXT("The missing EquipmentDefinition error names the item object path"),
		MissingReferenceErrors.Contains(Definition->GetPathName()));
	TestTrue(
		TEXT("The missing EquipmentDefinition error names the exact fragment entry"),
		MissingReferenceErrors.Contains(
			TEXT("Fragments[2]")) &&
			MissingReferenceErrors.Contains(
				TEXT("without an EquipmentDefinition")));

	EquippableFragment->EquipmentDefinition =
		URpgInventoryAutomationTestWeaponEquipmentDefinition::
			StaticClass();
	FDataValidationContext ValidReferenceContext;
	TestTrue(
		TEXT("Assigning a concrete EquipmentDefinition repairs the item contract"),
		ValidateAs(
			Definition,
			EDataValidationResult::Valid,
			ValidReferenceContext));
	TestEqual(
		TEXT("A valid EquippableItem reference emits no errors"),
		ValidReferenceContext.GetNumErrors(),
		0u);

	EquippableFragment->EquipmentDefinition =
		URpgInventoryAutomationTestDisabledEquipmentDefinition::
			StaticClass();
	FDataValidationContext EmptyAllowedSlotsContext;
	TestTrue(
		TEXT("An equippable item cannot reference an EquipmentDefinition with empty AllowedSlots"),
		ValidateAs(
			Definition,
			EDataValidationResult::Invalid,
			EmptyAllowedSlotsContext));
	const FString EmptyAllowedSlotsErrors =
		CollectValidationErrors(EmptyAllowedSlotsContext);
	TestTrue(
		TEXT("The empty AllowedSlots error identifies the item, fragment, and referenced equipment class"),
		EmptyAllowedSlotsErrors.Contains(Definition->GetPathName()) &&
			EmptyAllowedSlotsErrors.Contains(TEXT("Fragments[2]")) &&
			EmptyAllowedSlotsErrors.Contains(
				URpgInventoryAutomationTestDisabledEquipmentDefinition::
					StaticClass()->GetPathName()) &&
			EmptyAllowedSlotsErrors.Contains(
				TEXT("empty AllowedSlots")));

	EquippableFragment->EquipmentDefinition =
		URpgInventoryAutomationTestWeaponEquipmentDefinition::
			StaticClass();
	URpgInventoryFragment_EquippableItem* IgnoredDuplicate =
		NewObject<URpgInventoryFragment_EquippableItem>(Definition);
	Definition->Fragments.Add(IgnoredDuplicate);
	Definition->Fragments.Add(nullptr);
	FDataValidationContext RuntimeOrderingContext;
	TestTrue(
		TEXT("A later EquippableItem duplicate and null fragment follow the existing first-match runtime contract"),
		ValidateAs(
			Definition,
			EDataValidationResult::Valid,
			RuntimeOrderingContext));
	TestTrue(
		TEXT("FindFragmentByClass keeps the first effective EquippableItem fragment"),
		Definition->FindFragmentByClass(
			URpgInventoryFragment_EquippableItem::StaticClass()) ==
			EquippableFragment);

	URpgInventoryAutomationTestUnitItemDefinition*
		MissingFirstDefinition =
			NewObject<URpgInventoryAutomationTestUnitItemDefinition>();
	URpgInventoryFragment_EquippableItem* MissingFirstFragment =
		NewObject<URpgInventoryFragment_EquippableItem>(
			MissingFirstDefinition);
	MissingFirstDefinition->Fragments.Add(MissingFirstFragment);
	URpgInventoryFragment_EquippableItem* ValidSecondFragment =
		NewObject<URpgInventoryFragment_EquippableItem>(
			MissingFirstDefinition);
	ValidSecondFragment->EquipmentDefinition =
		URpgInventoryAutomationTestWeaponEquipmentDefinition::
			StaticClass();
	MissingFirstDefinition->Fragments.Add(ValidSecondFragment);
	FDataValidationContext MissingFirstContext;
	TestTrue(
		TEXT("A valid later duplicate does not hide a malformed effective EquippableItem fragment"),
		ValidateAs(
			MissingFirstDefinition,
			EDataValidationResult::Invalid,
			MissingFirstContext));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryItemDefinitionContainerDataValidationTest,
	"SurvivalRpg.Inventory.ItemDefinitions.DataValidation.ItemContainers",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryItemDefinitionContainerDataValidationTest::RunTest(
	const FString& Parameters)
{
	URpgInventoryAutomationTestUnitItemDefinition* Definition =
		NewObject<URpgInventoryAutomationTestUnitItemDefinition>();
	URpgInventoryFragment_ItemContainer* ContainerFragment =
		NewObject<URpgInventoryFragment_ItemContainer>(Definition);
	Definition->Fragments.Add(ContainerFragment);

	FRpgInventoryItemContainerDefinition& MainContainer =
		ContainerFragment->ProvidedContainers.AddDefaulted_GetRef();
	MainContainer.ContainerId = TEXT("Main");
	MainContainer.GridSize.Width = 4;
	MainContainer.GridSize.Height = 3;

	FDataValidationContext ValidContext;
	TestTrue(
		TEXT("A positive uniquely named native item-container row validates"),
		ContainerFragment->HasStructurallyValidProvidedContainers() &&
			ValidateAs(
				Definition,
				EDataValidationResult::Valid,
				ValidContext));
	TestEqual(
		TEXT("A valid native item-container contract emits no errors"),
		ValidContext.GetNumErrors(),
		0u);
	const FString DefinitionlessProviderWarnings =
		CollectValidationWarnings(ValidContext);
	TestEqual(
		TEXT("A definitionless ItemContainer emits exactly one migration warning"),
		CountValidationWarnings(ValidContext),
		1);
	TestTrue(
		TEXT("A definitionless ItemContainer remains valid but receives a Gear migration warning"),
		DefinitionlessProviderWarnings.Contains(
			Definition->GetPathName()) &&
			DefinitionlessProviderWarnings.Contains(
				TEXT("Fragments[2]")) &&
			DefinitionlessProviderWarnings.Contains(
				TEXT("no effective EquippableItem fragment")) &&
			DefinitionlessProviderWarnings.Contains(
				TEXT("no longer eligible for a Gear provider slot")));

	URpgInventoryFragment_EquippableItem* ProviderEquippableFragment =
		NewObject<URpgInventoryFragment_EquippableItem>(Definition);
	ProviderEquippableFragment->EquipmentDefinition =
		URpgInventoryAutomationTestBagEquipmentDefinition::
			StaticClass();
	Definition->Fragments.Add(ProviderEquippableFragment);
	FDataValidationContext ExplicitProviderContext;
	TestTrue(
		TEXT("An explicit bag EquipmentDefinition resolves the Gear provider migration warning"),
		ValidateAs(
			Definition,
			EDataValidationResult::Valid,
			ExplicitProviderContext));
	TestTrue(
		TEXT("An explicit provider contract emits no migration warnings"),
		CountValidationWarnings(ExplicitProviderContext) == 0 &&
			CollectValidationWarnings(
				ExplicitProviderContext).IsEmpty());

	URpgInventoryAutomationTestBagItemDefinition*
		AuthoredProviderFixture =
			NewObject<
				URpgInventoryAutomationTestBagItemDefinition>();
	FDataValidationContext AuthoredProviderContext;
	TestTrue(
		TEXT("The runtime bag-provider fixture satisfies the explicit editor contract"),
		ValidateAs(
			AuthoredProviderFixture,
			EDataValidationResult::Valid,
			AuthoredProviderContext));
	TestEqual(
		TEXT("The explicit runtime bag-provider fixture emits no migration warnings"),
		CountValidationWarnings(AuthoredProviderContext),
		0);

	FRpgInventoryItemContainerDefinition& MissingIdContainer =
		ContainerFragment->ProvidedContainers.AddDefaulted_GetRef();
	MissingIdContainer.GridSize.Width = 0;
	MissingIdContainer.GridSize.Height = -2;
	FRpgInventoryItemContainerDefinition& DuplicateContainer =
		ContainerFragment->ProvidedContainers.AddDefaulted_GetRef();
	DuplicateContainer.ContainerId = TEXT("Main");
	DuplicateContainer.GridSize.Width = 2;
	DuplicateContainer.GridSize.Height = 1;

	FDataValidationContext InvalidContext;
	TestFalse(
		TEXT("The shared runtime helper rejects missing ids, duplicate ids, and non-positive grids"),
		ContainerFragment->HasStructurallyValidProvidedContainers());
	TestTrue(
		TEXT("Malformed native item-container rows are invalid editor data"),
		ValidateAs(
			Definition,
			EDataValidationResult::Invalid,
			InvalidContext));
	const FString InvalidErrors =
		CollectValidationErrors(InvalidContext);
	TestTrue(
		TEXT("The missing-id error identifies its fragment and container indices"),
		InvalidErrors.Contains(Definition->GetPathName()) &&
			InvalidErrors.Contains(
				TEXT("Fragments[2].ProvidedContainers[1]")) &&
			InvalidErrors.Contains(TEXT("no ContainerId")));
	TestTrue(
		TEXT("The invalid-grid error reports dimensions and its exact row"),
		InvalidErrors.Contains(TEXT("0 x -2")) &&
			InvalidErrors.Contains(
				TEXT("Fragments[2].ProvidedContainers[1]")));
	TestTrue(
		TEXT("The duplicate-id error identifies both declarations"),
		InvalidErrors.Contains(TEXT("ContainerId 'Main'")) &&
			InvalidErrors.Contains(
				TEXT("Fragments[2].ProvidedContainers[2]")) &&
			InvalidErrors.Contains(
				TEXT("first declared at ProvidedContainers[0]")));

	ContainerFragment->ProvidedContainers.RemoveAt(1, 2);
	URpgInventoryFragment_ItemContainer* IgnoredLaterFragment =
		NewObject<URpgInventoryFragment_ItemContainer>(Definition);
	FRpgInventoryItemContainerDefinition&
		IgnoredMalformedContainer =
			IgnoredLaterFragment->ProvidedContainers.AddDefaulted_GetRef();
	IgnoredMalformedContainer.GridSize.Width = 0;
	IgnoredMalformedContainer.GridSize.Height = 0;
	Definition->Fragments.Add(IgnoredLaterFragment);

	FDataValidationContext FirstMatchContext;
	TestTrue(
		TEXT("A malformed later ItemContainer fragment is ignored by the existing first-match runtime contract"),
		ValidateAs(
			Definition,
			EDataValidationResult::Valid,
			FirstMatchContext));
	TestTrue(
		TEXT("FindFragmentByClass keeps the first effective ItemContainer fragment"),
		Definition->FindFragmentByClass(
			URpgInventoryFragment_ItemContainer::StaticClass()) ==
			ContainerFragment);

	IgnoredMalformedContainer.ContainerId = TEXT("Later");
	IgnoredMalformedContainer.GridSize.Width = 1;
	IgnoredMalformedContainer.GridSize.Height = 1;
	ContainerFragment->ProvidedContainers[0].ContainerId = NAME_None;
	FDataValidationContext MalformedFirstContext;
	TestTrue(
		TEXT("A valid later duplicate cannot hide a malformed effective ItemContainer fragment"),
		ValidateAs(
			Definition,
			EDataValidationResult::Invalid,
			MalformedFirstContext));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryAuthoredDefinitionDataValidationTest,
	"SurvivalRpg.Inventory.ItemDefinitions.DataValidation.AuthoredAssets",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryAuthoredDefinitionDataValidationTest::RunTest(
	const FString& Parameters)
{
	IAssetRegistry& AssetRegistry =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
			TEXT("AssetRegistry")).Get();
	AssetRegistry.WaitForCompletion();

	TSet<FTopLevelAssetPath> DerivedDefinitionClassPaths;
	AssetRegistry.GetDerivedClassNames(
		{
			URpgInventoryItemDefinition::StaticClass()->
				GetClassPathName(),
			URpgEquipmentDefinition::StaticClass()->
				GetClassPathName()
		},
		TSet<FTopLevelAssetPath>(),
		DerivedDefinitionClassPaths);

	FARFilter BlueprintFilter;
	BlueprintFilter.ClassPaths.Add(
		UBlueprint::StaticClass()->GetClassPathName());
	BlueprintFilter.bRecursiveClasses = true;
	BlueprintFilter.bRecursivePaths = true;
	for (const FName ContentRoot : GetDefinitionContentRoots())
	{
		BlueprintFilter.PackagePaths.Add(ContentRoot);
	}

	TArray<FAssetData> BlueprintAssets;
	AssetRegistry.GetAssets(BlueprintFilter, BlueprintAssets);
	BlueprintAssets.Sort(
		[](const FAssetData& Left, const FAssetData& Right)
		{
			return Left.PackageName.LexicalLess(
				Right.PackageName);
		});

	int32 ItemDefinitionCount = 0;
	int32 EquipmentDefinitionCount = 0;
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
		if (!DerivedDefinitionClassPaths.Contains(
			GeneratedClassPath))
		{
			continue;
		}

		UClass* GeneratedClass = LoadObject<UClass>(
			nullptr,
			*GeneratedClassPath.ToString());
		if (!GeneratedClass)
		{
			AddError(
				FString::Printf(
					TEXT("%s generated class '%s' did not load"),
					*BlueprintAsset.GetObjectPathString(),
					*GeneratedClassPath.ToString()));
			continue;
		}
		if (GeneratedClass->HasAnyClassFlags(
			CLASS_Abstract |
			CLASS_Deprecated |
			CLASS_NewerVersionExists))
		{
			continue;
		}

		const bool bIsItemDefinition =
			GeneratedClass->IsChildOf(
				URpgInventoryItemDefinition::StaticClass());
		const bool bIsEquipmentDefinition =
			GeneratedClass->IsChildOf(
				URpgEquipmentDefinition::StaticClass());
		if (!bIsItemDefinition && !bIsEquipmentDefinition)
		{
			continue;
		}

		const UObject* Definition =
			GeneratedClass->GetDefaultObject();
		if (!Definition)
		{
			AddError(
				FString::Printf(
					TEXT("%s has no generated class default object"),
					*BlueprintAsset.GetObjectPathString()));
			continue;
		}

		ItemDefinitionCount += bIsItemDefinition ? 1 : 0;
		EquipmentDefinitionCount +=
			bIsEquipmentDefinition ? 1 : 0;

		FDataValidationContext ValidationContext;
		const EDataValidationResult ValidationResult =
			Definition->IsDataValid(ValidationContext);
		if (ValidationResult != EDataValidationResult::Valid ||
			ValidationContext.GetNumErrors() > 0)
		{
			AddError(
				FString::Printf(
					TEXT(
						"%s failed native definition validation: %s"),
					*BlueprintAsset.GetObjectPathString(),
					*CollectValidationErrors(
						ValidationContext)));
		}
	}

	TestTrue(
		TEXT("At least one concrete authored ItemDefinition CDO was validated"),
		ItemDefinitionCount > 0);
	TestTrue(
		TEXT("At least one concrete authored EquipmentDefinition CDO was validated"),
		EquipmentDefinitionCount > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgEquipmentDefinitionSlotReferenceDataValidationTest,
	"SurvivalRpg.Equipment.Definition.DataValidation.SlotReferences",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgEquipmentDefinitionSlotReferenceDataValidationTest::RunTest(
	const FString& Parameters)
{
	URpgInventoryAutomationTestWeaponEquipmentDefinition* Definition =
		NewObject<
			URpgInventoryAutomationTestWeaponEquipmentDefinition>();
	FDataValidationContext BaselineContext;
	TestTrue(
		TEXT("The baseline MainHand equipment definition validates"),
		ValidateAs(
			Definition,
			EDataValidationResult::Valid,
			BaselineContext));
	TestTrue(
		TEXT("The shared runtime helper accepts the baseline slot references"),
		Definition->HasStructurallyValidSlotReferences());

	Definition->AllowedSlots = { ERpgEquipmentSlot::None };
	FDataValidationContext NoneSlotContext;
	TestFalse(
		TEXT("The shared runtime helper rejects None in AllowedSlots"),
		Definition->HasStructurallyValidSlotReferences());
	TestTrue(
		TEXT("None in AllowedSlots is invalid editor data"),
		ValidateAs(
			Definition,
			EDataValidationResult::Invalid,
			NoneSlotContext));
	FString ErrorText = CollectValidationErrors(NoneSlotContext);
	TestTrue(
		TEXT("The None-slot error is path-aware and identifies AllowedSlots[0]"),
		ErrorText.Contains(Definition->GetPathName()) &&
			ErrorText.Contains(TEXT("AllowedSlots[0]")));

	Definition->AllowedSlots =
	{
		static_cast<ERpgEquipmentSlot>(255)
	};
	FDataValidationContext UnknownSlotContext;
	TestFalse(
		TEXT("The shared runtime helper rejects unknown AllowedSlots values"),
		Definition->HasStructurallyValidSlotReferences());
	TestTrue(
		TEXT("An unknown AllowedSlots value is invalid editor data"),
		ValidateAs(
			Definition,
			EDataValidationResult::Invalid,
			UnknownSlotContext));
	ErrorText = CollectValidationErrors(UnknownSlotContext);
	TestTrue(
		TEXT("The unknown-slot error identifies the exact AllowedSlots row"),
		ErrorText.Contains(TEXT("invalid or None slot")) &&
			ErrorText.Contains(TEXT("AllowedSlots[0]")));

	Definition->AllowedSlots =
	{
		ERpgEquipmentSlot::MainHand,
		ERpgEquipmentSlot::MainHand
	};
	FDataValidationContext DuplicateSlotContext;
	TestTrue(
		TEXT("A duplicate AllowedSlots value is invalid editor data"),
		ValidateAs(
			Definition,
			EDataValidationResult::Invalid,
			DuplicateSlotContext));
	ErrorText = CollectValidationErrors(DuplicateSlotContext);
	TestTrue(
		TEXT("The duplicate-slot error reports its value and second index"),
		ErrorText.Contains(TEXT("MainHand")) &&
			ErrorText.Contains(TEXT("AllowedSlots[1]")));

	Definition->AllowedSlots = { ERpgEquipmentSlot::MainHand };
	Definition->SlotAbilitySetsToGrant.SetNum(1);
	Definition->SlotAbilitySetsToGrant[0].EquippedSlot =
		ERpgEquipmentSlot::MainHand;
	Definition->SlotAbilitySetsToGrant[0].AbilitySet = nullptr;
	FDataValidationContext MissingAbilitySetContext;
	TestTrue(
		TEXT("A slot-grant row without AbilitySet is invalid editor data"),
		ValidateAs(
			Definition,
			EDataValidationResult::Invalid,
			MissingAbilitySetContext));
	ErrorText = CollectValidationErrors(MissingAbilitySetContext);
	TestTrue(
		TEXT("The missing-AbilitySet error identifies the exact grant row"),
		ErrorText.Contains(
			TEXT("SlotAbilitySetsToGrant[0]")) &&
			ErrorText.Contains(TEXT("no AbilitySet")));

	Definition->SlotAbilitySetsToGrant[0].AbilitySet =
		GetDefault<
			URpgEquipmentAutomationTestHealthAbilitySet>();
	Definition->SlotAbilitySetsToGrant[0].EquippedSlot =
		ERpgEquipmentSlot::Head;
	FDataValidationContext UnreachableGrantContext;
	TestTrue(
		TEXT("A slot-grant target outside AllowedSlots is invalid editor data"),
		ValidateAs(
			Definition,
			EDataValidationResult::Invalid,
			UnreachableGrantContext));
	ErrorText = CollectValidationErrors(UnreachableGrantContext);
	TestTrue(
		TEXT("The unreachable-grant error reports its slot and row"),
		ErrorText.Contains(TEXT("Head")) &&
			ErrorText.Contains(
				TEXT("SlotAbilitySetsToGrant[0]")) &&
			ErrorText.Contains(
				TEXT("not present exactly once in AllowedSlots")));

	Definition->SlotAbilitySetsToGrant[0].EquippedSlot =
		ERpgEquipmentSlot::None;
	FDataValidationContext NoneGrantSlotContext;
	TestTrue(
		TEXT("A None slot-grant target is invalid editor data"),
		ValidateAs(
			Definition,
			EDataValidationResult::Invalid,
			NoneGrantSlotContext));
	ErrorText = CollectValidationErrors(NoneGrantSlotContext);
	TestTrue(
		TEXT("The None grant-target error identifies EquippedSlot and its row"),
		ErrorText.Contains(TEXT("invalid or None EquippedSlot")) &&
			ErrorText.Contains(
				TEXT("SlotAbilitySetsToGrant[0]")));

	Definition->SlotAbilitySetsToGrant[0].EquippedSlot =
		ERpgEquipmentSlot::MainHand;
	FDataValidationContext ValidGrantContext;
	TestTrue(
		TEXT("A complete slot-grant row targeting an allowed slot validates"),
		Definition->HasStructurallyValidSlotReferences() &&
			ValidateAs(
				Definition,
				EDataValidationResult::Valid,
				ValidGrantContext));

	Definition->SlotAbilitySetsToGrant.Reset();
	Definition->AllowedSlots.Reset();
	FDataValidationContext EmptyAllowedSlotsContext;
	TestTrue(
		TEXT("An entirely empty AllowedSlots array remains a structurally valid disabled standalone definition"),
		Definition->HasStructurallyValidSlotReferences() &&
			Definition->HasValidHandOccupancyContract());
	TestTrue(
		TEXT("Empty AllowedSlots remains valid until an equippable item references the definition"),
		ValidateAs(
			Definition,
			EDataValidationResult::Valid,
			EmptyAllowedSlotsContext));

	Definition->AllowedSlots = { ERpgEquipmentSlot::OffHand };
	Definition->HandOccupancy =
		ERpgEquipmentHandOccupancy::BothHands;
	FDataValidationContext InvalidHandPolicyContext;
	TestTrue(
		TEXT("BothHands plus OffHand remains structurally well-formed"),
		Definition->HasStructurallyValidSlotReferences());
	TestFalse(
		TEXT("The semantic hand contract rejects BothHands plus OffHand"),
		Definition->HasValidHandOccupancyContract());
	TestTrue(
		TEXT("BothHands plus OffHand is invalid editor data"),
		ValidateAs(
			Definition,
			EDataValidationResult::Invalid,
			InvalidHandPolicyContext));
	ErrorText = CollectValidationErrors(InvalidHandPolicyContext);
	TestTrue(
		TEXT("The hand-contract error names the definition and conflicting values"),
		ErrorText.Contains(Definition->GetPathName()) &&
			ErrorText.Contains(TEXT("BothHands")) &&
			ErrorText.Contains(TEXT("OffHand")));

	Definition->AllowedSlots = { ERpgEquipmentSlot::MainHand };
	Definition->HandOccupancy =
		static_cast<ERpgEquipmentHandOccupancy>(255);
	FDataValidationContext UnknownHandPolicyContext;
	TestFalse(
		TEXT("The semantic hand helper rejects unknown HandOccupancy values"),
		Definition->HasValidHandOccupancyContract());
	TestTrue(
		TEXT("An unknown HandOccupancy value is invalid editor data"),
		ValidateAs(
			Definition,
			EDataValidationResult::Invalid,
			UnknownHandPolicyContext));
	ErrorText = CollectValidationErrors(UnknownHandPolicyContext);
	TestTrue(
		TEXT("The unknown hand-contract error names the field and raw value"),
		ErrorText.Contains(TEXT("HandOccupancy")) &&
			ErrorText.Contains(TEXT("255")));

	return true;
}

#endif
