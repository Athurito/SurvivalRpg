#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "RpgBaseCampActor.h"
#include "RpgBaseStorageComponent.h"
#include "RpgBaseStorageDomainAnchorComponent.h"
#include "RpgBaseStorageUpgradeDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryAutomationTestTypes.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_ContainmentProfile.h"
#include "SurvivalRpg/Inventory/RpgInventoryFragment_StorageProfile.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "UObject/UnrealType.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace RpgStorageFoundationTests
{
	class FScopedStorageWorld
	{
	public:
		FScopedStorageWorld()
		{
			GameInstance = NewObject<UGameInstance>(
				GEngine,
				NAME_None,
				RF_Transient);
			if (!GameInstance)
			{
				return;
			}

			GameInstance->AddToRoot();
			GameInstance->InitializeStandalone();
			World = GameInstance->GetWorld();
		}

		~FScopedStorageWorld()
		{
			UWorld* WorldToDestroy = World;
			if (GameInstance)
			{
				GameInstance->Shutdown();
			}
			if (WorldToDestroy)
			{
				GEngine->DestroyWorldContext(WorldToDestroy);
				WorldToDestroy->DestroyWorld(false);
			}
			if (GameInstance)
			{
				GameInstance->RemoveFromRoot();
			}
		}

		bool IsValid() const
		{
			return World != nullptr;
		}

		ARpgBaseCampActor* SpawnBase(FName BaseId)
		{
			if (!World || BaseId.IsNone())
			{
				return nullptr;
			}

			FActorSpawnParameters SpawnParameters;
			SpawnParameters.Name = MakeUniqueObjectName(
				World,
				ARpgBaseCampActor::StaticClass(),
				TEXT("StorageInvariantBase"));
			SpawnParameters.ObjectFlags = RF_Transient;
			ARpgBaseCampActor* Base =
				World->SpawnActor<ARpgBaseCampActor>(SpawnParameters);
			FNameProperty* BaseIdProperty = FindFProperty<FNameProperty>(
				ARpgBaseCampActor::StaticClass(),
				TEXT("BaseId"));
			if (!Base || !BaseIdProperty)
			{
				return nullptr;
			}
			BaseIdProperty->SetPropertyValue_InContainer(Base, BaseId);
			return Base;
		}

		APlayerController* SpawnController(const TCHAR* DebugName)
		{
			if (!World)
			{
				return nullptr;
			}

			FActorSpawnParameters SpawnParameters;
			SpawnParameters.Name = MakeUniqueObjectName(
				World,
				APlayerController::StaticClass(),
				FName(DebugName));
			SpawnParameters.ObjectFlags = RF_Transient;
			return World->SpawnActor<APlayerController>(SpawnParameters);
		}

	private:
		TObjectPtr<UGameInstance> GameInstance;
		TObjectPtr<UWorld> World;
	};

	bool MakeStorageFixture(
		FAutomationTestBase& Test,
		FScopedStorageWorld& TestWorld,
		FName BaseId,
		ARpgBaseCampActor*& OutBase,
		URpgBaseStorageComponent*& OutStorage)
	{
		if (!Test.TestTrue(TEXT("Storage test world is valid"), TestWorld.IsValid()))
		{
			return false;
		}
		OutBase = TestWorld.SpawnBase(BaseId);
		if (!Test.TestNotNull(TEXT("Authoritative base fixture exists"), OutBase))
		{
			return false;
		}
		OutStorage = OutBase->GetBaseStorageComponent();
		return Test.TestNotNull(
			TEXT("Base fixture owns a storage coordinator"),
			OutStorage) &&
			Test.TestTrue(TEXT("Storage fixture has authority"), OutBase->HasAuthority());
	}

#if WITH_EDITOR
	bool PassesDataValidation(const UObject* Object)
	{
		if (!Object)
		{
			return false;
		}

		FDataValidationContext Context;
		return Object->IsDataValid(Context) != EDataValidationResult::Invalid;
	}
#endif
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgStorageProfileStaticContractTest,
	"SurvivalRpg.Inventory.StorageProfile.StaticContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgStorageProfileStaticContractTest::RunTest(const FString& Parameters)
{
	URpgInventoryFragment_StorageProfile* Profile =
		NewObject<URpgInventoryFragment_StorageProfile>();
	if (!TestNotNull(TEXT("Storage profile can be instantiated"), Profile))
	{
		return false;
	}

	Profile->StorageMode = ERpgInventoryStorageMode::BulkResource;
	Profile->StorageDomainTag = RpgGameplayTags::Storage_Domain_Materials;
	Profile->BulkCapacityCost = 2;
	Profile->bCanAutoDeposit = true;
	Profile->bCanCraftFromNetwork = true;
	TestTrue(TEXT("Authored fungible bulk profile is structurally valid"), Profile->IsStructurallyValid());
	TestTrue(TEXT("Bulk mode is exposed through IsBulkResource"), Profile->IsBulkResource());
	TestTrue(TEXT("Manual bulk deposit is independent of auto-deposit capability"), Profile->CanDepositAsBulk());

	FGameplayTagContainer NetworkCapabilities;
	TestTrue(TEXT("Manual bulk deposit does not require the auto-deposit capability"), Profile->CanDepositAsBulkWithCapabilities(NetworkCapabilities));
	Profile->RequiredStorageCapabilityTags.AddTag(
		RpgGameplayTags::Storage_Capability_Reservations);
	TestFalse(TEXT("Manual bulk deposit still enforces profile-specific capability requirements"), Profile->CanDepositAsBulkWithCapabilities(NetworkCapabilities));
	NetworkCapabilities.AddTag(
		RpgGameplayTags::Storage_Capability_Reservations);
	TestTrue(TEXT("Manual bulk deposit accepts satisfied profile-specific capabilities without auto-deposit"), Profile->CanDepositAsBulkWithCapabilities(NetworkCapabilities));
	TestFalse(TEXT("Auto-deposit fails closed without the mandatory network capability"), Profile->CanAutoDeposit(NetworkCapabilities));
	NetworkCapabilities.AddTag(
		RpgGameplayTags::Storage_Capability_AutoDepositBulk);
	TestTrue(TEXT("Auto-deposit accepts explicit item opt-in plus network capability"), Profile->CanAutoDeposit(NetworkCapabilities));
	Profile->bCanAutoDeposit = false;
	TestFalse(TEXT("Network capability cannot override item-definition auto-deposit opt-out"), Profile->CanAutoDeposit(NetworkCapabilities));
	TestTrue(TEXT("Item remains manually bulk-depositable after auto-deposit opt-out"), Profile->CanDepositAsBulk());
	Profile->bCanAutoDeposit = true;

	Profile->BulkCapacityCost = 0;
	TestFalse(TEXT("Bulk profile rejects zero capacity cost"), Profile->IsStructurallyValid());

	Profile->BulkCapacityCost = 1;
	Profile->StorageMode = ERpgInventoryStorageMode::GridItem;
	TestFalse(TEXT("Instance-preserving mode rejects bulk convenience flags"), Profile->IsStructurallyValid());

	Profile->bCanAutoDeposit = false;
	Profile->bCanCraftFromNetwork = false;
	Profile->StorageDomainTag = RpgGameplayTags::Storage_Domain_Armory;
	TestTrue(TEXT("Grid item becomes valid when bulk conveniences are disabled"), Profile->IsStructurallyValid());

	const URpgInventoryItemDefinition* InvalidContainedContainer =
		GetDefault<URpgInventoryAutomationTestContainedContainerDefinition>();
	const URpgInventoryFragment_StorageProfile* InvalidStorageProfile =
		InvalidContainedContainer
			? Cast<URpgInventoryFragment_StorageProfile>(
				InvalidContainedContainer->FindFragmentByClass(
					URpgInventoryFragment_StorageProfile::StaticClass()))
			: nullptr;
	const URpgInventoryFragment_ContainmentProfile* InvalidContainmentProfile =
		InvalidContainedContainer
			? Cast<URpgInventoryFragment_ContainmentProfile>(
				InvalidContainedContainer->FindFragmentByClass(
					URpgInventoryFragment_ContainmentProfile::StaticClass()))
			: nullptr;
	if (!TestNotNull(
			TEXT("Invalid contained-container fixture exposes a storage profile"),
			InvalidStorageProfile) ||
		!TestNotNull(
			TEXT("Invalid contained-container fixture exposes a containment profile"),
			InvalidContainmentProfile))
	{
		return false;
	}
	TestFalse(
		TEXT("A SpecialContainedItem provider fails runtime structural validation"),
		InvalidStorageProfile->IsStructurallyValid());
	TestFalse(
		TEXT("Containment rejects a provider before extraction could consume its subtree"),
		InvalidContainmentProfile->IsStructurallyValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgContainmentProfileStaticContractTest,
	"SurvivalRpg.Inventory.ContainmentProfile.StaticContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgContainmentProfileStaticContractTest::RunTest(const FString& Parameters)
{
	URpgInventoryFragment_ContainmentProfile* Profile =
		NewObject<URpgInventoryFragment_ContainmentProfile>();
	if (!TestNotNull(TEXT("Containment profile can be instantiated"), Profile))
	{
		return false;
	}

	Profile->RequiredSealedSlots = 1;
	Profile->RequiredContainmentStrength = 25.0f;
	Profile->RequiredCorruptionProtection = 10.0f;
	Profile->InstabilityValue = 8.0f;
	Profile->ContainmentStrain = 5.0f;
	Profile->RequiredContainmentCapabilityTags.AddTag(
		RpgGameplayTags::Storage_Capability_RiftContainment);
	Profile->AllowedStabilizedDestinationDomains.AddTag(
		RpgGameplayTags::Storage_Domain_Armory);
	TestTrue(TEXT("Non-negative contained-item contract is structurally valid"), Profile->IsStructurallyValid());

	Profile->RequiredSealedSlots = 2;
	TestFalse(TEXT("V1 contained items must occupy exactly one sealed slot"), Profile->IsStructurallyValid());
	Profile->RequiredSealedSlots = 1;
	Profile->ExtractionOutputCount = 1;
	TestFalse(TEXT("Extraction count without an output definition is rejected"), Profile->IsStructurallyValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgStorageFoundationNeutralDefaultsTest,
	"SurvivalRpg.Base.Storage.Foundation.NeutralDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgStorageFoundationNeutralDefaultsTest::RunTest(const FString& Parameters)
{
	URpgBaseStorageUpgradeDefinition* Upgrade =
		NewObject<URpgBaseStorageUpgradeDefinition>();
	if (!TestNotNull(TEXT("Storage upgrade definition can be instantiated"), Upgrade))
	{
		return false;
	}

	TestFalse(TEXT("New network effects are neutral for legacy upgrade assets"), Upgrade->HasNetworkEffects());
	TestTrue(TEXT("Decommission refunds are explicit and empty by default"), Upgrade->DecommissionRefunds.IsEmpty());

	FRpgBaseStorageDomainAnchorVisualState VisualState;
	TestTrue(TEXT("Default anchor visual state is normalized"), VisualState.IsValid());
	VisualState.FillRatio = 1.01f;
	TestFalse(TEXT("Anchor visual state rejects out-of-range fill"), VisualState.IsValid());
	return true;
}

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgBaseStorageUpgradeDefinitionDataValidationTest,
	"SurvivalRpg.Base.Storage.DataValidation.UpgradeEffectsAndStationTags",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgBaseStorageUpgradeDefinitionDataValidationTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgStorageFoundationTests;
	URpgBaseStorageUpgradeDefinition* Upgrade =
		NewObject<URpgBaseStorageUpgradeDefinition>();
	if (!TestNotNull(TEXT("Upgrade validation fixture exists"), Upgrade))
	{
		return false;
	}

	TestFalse(TEXT("Untargeted upgrade definition is rejected"), PassesDataValidation(Upgrade));
	Upgrade->TargetDomainTag = RpgGameplayTags::Storage_Domain_Materials;
	Upgrade->TargetAnchorId = TEXT("MaterialDepot");
	TestTrue(TEXT("Explicitly targeted neutral upgrade definition is valid"), PassesDataValidation(Upgrade));
	Upgrade->CapacityEffect.AdditionalCapacity = 1;
	TestTrue(TEXT("CapacityEffect accepts Materials"), PassesDataValidation(Upgrade));
	Upgrade->TargetDomainTag = RpgGameplayTags::Storage_Domain_Armory;
	TestFalse(TEXT("CapacityEffect rejects non-Materials domains"), PassesDataValidation(Upgrade));
	Upgrade->CapacityEffect.AdditionalCapacity = 0;

	Upgrade->GridEffect.AdditionalColumns = 1;
	TestTrue(TEXT("GridEffect accepts Armory"), PassesDataValidation(Upgrade));
	Upgrade->TargetDomainTag = RpgGameplayTags::Storage_Domain_Materials;
	TestFalse(TEXT("GridEffect rejects non-Armory domains"), PassesDataValidation(Upgrade));
	Upgrade->GridEffect.AdditionalColumns = 0;

	Upgrade->TargetDomainTag = RpgGameplayTags::Storage_Domain_RiftContainment;
	Upgrade->ContainmentEffect.AdditionalSealedSlots = 1;
	TestTrue(TEXT("ContainmentEffect accepts RiftContainment"), PassesDataValidation(Upgrade));
	Upgrade->TargetDomainTag = RpgGameplayTags::Storage_Domain_Armory;
	TestFalse(TEXT("ContainmentEffect rejects non-Rift domains"), PassesDataValidation(Upgrade));
	Upgrade->ContainmentEffect.AdditionalSealedSlots = 0;

	Upgrade->TargetDomainTag = RpgGameplayTags::Storage_Domain_RiftContainment;
	Upgrade->StrainEffect.StrainToleranceDelta = 1.0f;
	TestTrue(TEXT("StrainEffect accepts RiftContainment"), PassesDataValidation(Upgrade));
	Upgrade->TargetDomainTag = RpgGameplayTags::Storage_Domain_Materials;
	TestFalse(TEXT("StrainEffect rejects non-Rift domains"), PassesDataValidation(Upgrade));
	Upgrade->StrainEffect.StrainToleranceDelta = 0.0f;
	Upgrade->TargetDomainTag = RpgGameplayTags::Storage_Domain_Materials;

	Upgrade->AllowedStationTags.AddTag(
		RpgGameplayTags::Base_Storage_Station_Terminal);
	TestTrue(TEXT("AllowedStationTags accepts a strict station child"), PassesDataValidation(Upgrade));
	Upgrade->AllowedStationTags.Reset();
	Upgrade->AllowedStationTags.AddTag(RpgGameplayTags::Base_Storage_Station);
	TestFalse(TEXT("AllowedStationTags rejects the station root"), PassesDataValidation(Upgrade));
	Upgrade->AllowedStationTags.Reset();
	Upgrade->AllowedStationTags.AddTag(
		RpgGameplayTags::Storage_Domain_Materials);
	TestFalse(TEXT("AllowedStationTags rejects unrelated hierarchies"), PassesDataValidation(Upgrade));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgBaseStorageComponentDataValidationTest,
	"SurvivalRpg.Base.Storage.DataValidation.CoordinatorConfiguration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgBaseStorageComponentDataValidationTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgStorageFoundationTests;
	URpgBaseStorageComponent* Storage = NewObject<URpgBaseStorageComponent>();
	if (!TestNotNull(TEXT("Storage validation fixture exists"), Storage))
	{
		return false;
	}

	FIntProperty* MaterialCapacityProperty = FindFProperty<FIntProperty>(
		URpgBaseStorageComponent::StaticClass(),
		TEXT("BaseMaterialCapacityPoints"));
	FIntProperty* ContainmentSlotsProperty = FindFProperty<FIntProperty>(
		URpgBaseStorageComponent::StaticClass(),
		TEXT("BaseContainmentSlots"));
	FIntProperty* ArmoryColumnsProperty = FindFProperty<FIntProperty>(
		URpgBaseStorageComponent::StaticClass(),
		TEXT("BaseArmoryGridColumns"));
	FIntProperty* ArmoryRowsProperty = FindFProperty<FIntProperty>(
		URpgBaseStorageComponent::StaticClass(),
		TEXT("BaseArmoryGridRows"));
	FIntProperty* CleanseAmountProperty = FindFProperty<FIntProperty>(
		URpgBaseStorageComponent::StaticClass(),
		TEXT("RiftCleanseAmount"));
	FBoolProperty* SharedCapacityProperty = FindFProperty<FBoolProperty>(
		URpgBaseStorageComponent::StaticClass(),
		TEXT("bUseSharedMaterialCapacity"));
	FStructProperty* BaselineCapabilitiesProperty =
		FindFProperty<FStructProperty>(
			URpgBaseStorageComponent::StaticClass(),
			TEXT("BaselineCapabilities"));
	FArrayProperty* CleanseCostsProperty = FindFProperty<FArrayProperty>(
		URpgBaseStorageComponent::StaticClass(),
		TEXT("RiftCleanseCosts"));
	if (!TestNotNull(TEXT("Material capacity property is reflected"), MaterialCapacityProperty) ||
		!TestNotNull(TEXT("Containment slots property is reflected"), ContainmentSlotsProperty) ||
		!TestNotNull(TEXT("Armory columns property is reflected"), ArmoryColumnsProperty) ||
		!TestNotNull(TEXT("Armory rows property is reflected"), ArmoryRowsProperty) ||
		!TestNotNull(TEXT("Cleanse amount property is reflected"), CleanseAmountProperty) ||
		!TestNotNull(TEXT("Shared-capacity property is reflected"), SharedCapacityProperty) ||
		!TestNotNull(TEXT("Baseline capability property is reflected"), BaselineCapabilitiesProperty) ||
		!TestNotNull(TEXT("Cleanse costs property is reflected"), CleanseCostsProperty))
	{
		return false;
	}

	FGameplayTagContainer* BaselineCapabilities =
		BaselineCapabilitiesProperty->ContainerPtrToValuePtr<
			FGameplayTagContainer>(Storage);
	TArray<FRpgBaseStorageOperationCost>* CleanseCosts =
		CleanseCostsProperty->ContainerPtrToValuePtr<
			TArray<FRpgBaseStorageOperationCost>>(Storage);
	if (!TestNotNull(TEXT("Baseline capability value is addressable"), BaselineCapabilities) ||
		!TestNotNull(TEXT("Cleanse cost value is addressable"), CleanseCosts))
	{
		return false;
	}

	TestFalse(TEXT("Native coordinator requires an authored cleanse cost"), PassesDataValidation(Storage));

	MaterialCapacityProperty->SetPropertyValue_InContainer(Storage, -1);
	TestFalse(TEXT("Negative material capacity is rejected"), PassesDataValidation(Storage));
	MaterialCapacityProperty->SetPropertyValue_InContainer(Storage, 300);
	ContainmentSlotsProperty->SetPropertyValue_InContainer(Storage, -1);
	TestFalse(TEXT("Negative containment slots are rejected"), PassesDataValidation(Storage));
	ContainmentSlotsProperty->SetPropertyValue_InContainer(Storage, 0);
	ArmoryColumnsProperty->SetPropertyValue_InContainer(Storage, 0);
	TestFalse(TEXT("Zero Armory columns are rejected"), PassesDataValidation(Storage));
	ArmoryColumnsProperty->SetPropertyValue_InContainer(Storage, 8);
	ArmoryRowsProperty->SetPropertyValue_InContainer(Storage, 0);
	TestFalse(TEXT("Zero Armory rows are rejected"), PassesDataValidation(Storage));
	ArmoryRowsProperty->SetPropertyValue_InContainer(Storage, 6);

	FRpgBaseStorageOperationCost ValidCost;
	ValidCost.ItemDefinition =
		URpgInventoryAutomationTestMaterialDefinition::StaticClass();
	ValidCost.Count = 2;
	CleanseCosts->Add(ValidCost);
	TestTrue(TEXT("Explicit Materials BulkResource cleanse cost is accepted"), PassesDataValidation(Storage));
	SharedCapacityProperty->SetPropertyValue_InContainer(Storage, false);
	TestFalse(TEXT("Legacy per-definition capacity mode is rejected"), PassesDataValidation(Storage));
	SharedCapacityProperty->SetPropertyValue_InContainer(Storage, true);

	BaselineCapabilities->AddTag(
		RpgGameplayTags::Storage_Capability_CraftFromNetwork);
	TestTrue(TEXT("Strict baseline capability children are accepted"), PassesDataValidation(Storage));
	BaselineCapabilities->Reset();
	BaselineCapabilities->AddTag(RpgGameplayTags::Storage_Capability);
	TestFalse(TEXT("Baseline capability root is rejected"), PassesDataValidation(Storage));
	BaselineCapabilities->Reset();
	BaselineCapabilities->AddTag(RpgGameplayTags::Storage_Domain_Materials);
	TestFalse(TEXT("Unrelated baseline capability tags are rejected"), PassesDataValidation(Storage));
	BaselineCapabilities->Reset();

	CleanseCosts->Add(ValidCost);
	TestFalse(TEXT("Duplicate cleanse definitions are rejected"), PassesDataValidation(Storage));
	CleanseCosts->SetNum(1);
	(*CleanseCosts)[0].ItemDefinition =
		URpgInventoryAutomationTestWideItemDefinition::StaticClass();
	TestFalse(TEXT("Cleanse costs without an explicit BulkResource profile are rejected"), PassesDataValidation(Storage));
	(*CleanseCosts)[0] = ValidCost;
	(*CleanseCosts)[0].Count = 0;
	TestFalse(TEXT("Non-positive cleanse cost counts are rejected"), PassesDataValidation(Storage));
	CleanseCosts->Reset();
	TestFalse(TEXT("A free Rift cleanse is rejected"), PassesDataValidation(Storage));
	CleanseCosts->Add(ValidCost);

	CleanseAmountProperty->SetPropertyValue_InContainer(Storage, 0);
	TestFalse(TEXT("Zero cleanse amount is rejected"), PassesDataValidation(Storage));
	CleanseAmountProperty->SetPropertyValue_InContainer(Storage, 101);
	TestFalse(TEXT("Cleanse amount above 100 is rejected"), PassesDataValidation(Storage));
	CleanseAmountProperty->SetPropertyValue_InContainer(Storage, 25);
	TestTrue(TEXT("Restored coordinator configuration is valid"), PassesDataValidation(Storage));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgBaseStorageAnchorDataValidationTest,
	"SurvivalRpg.Base.Storage.DataValidation.BaseCampAnchorTopology",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgBaseStorageAnchorDataValidationTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgStorageFoundationTests;
	FScopedStorageWorld TestWorld;
	ARpgBaseCampActor* Base = nullptr;
	URpgBaseStorageComponent* Storage = nullptr;
	if (!MakeStorageFixture(
			*this,
			TestWorld,
			FName(TEXT("Automation.AnchorValidation")),
			Base,
			Storage))
	{
		return false;
	}

	FArrayProperty* CleanseCostsProperty = FindFProperty<FArrayProperty>(
		URpgBaseStorageComponent::StaticClass(),
		TEXT("RiftCleanseCosts"));
	if (!TestNotNull(
			TEXT("Base fixture cleanse costs are reflected"),
			CleanseCostsProperty))
	{
		return false;
	}
	TArray<FRpgBaseStorageOperationCost>* CleanseCosts =
		CleanseCostsProperty->ContainerPtrToValuePtr<
			TArray<FRpgBaseStorageOperationCost>>(Storage);
	if (!TestNotNull(
			TEXT("Base fixture cleanse costs are addressable"),
			CleanseCosts))
	{
		return false;
	}
	FRpgBaseStorageOperationCost& ValidCleanseCost =
		CleanseCosts->AddDefaulted_GetRef();
	ValidCleanseCost.ItemDefinition =
		URpgInventoryAutomationTestMaterialDefinition::StaticClass();
	ValidCleanseCost.Count = 1;

	TInlineComponentArray<URpgBaseStorageDomainAnchorComponent*> Anchors(Base);
	URpgBaseStorageDomainAnchorComponent* MaterialsAnchor = nullptr;
	URpgBaseStorageDomainAnchorComponent* ArmoryAnchor = nullptr;
	for (URpgBaseStorageDomainAnchorComponent* Anchor : Anchors)
	{
		if (!Anchor)
		{
			continue;
		}
		if (Anchor->GetDomainTag() == RpgGameplayTags::Storage_Domain_Materials)
		{
			MaterialsAnchor = Anchor;
		}
		else if (Anchor->GetDomainTag() == RpgGameplayTags::Storage_Domain_Armory)
		{
			ArmoryAnchor = Anchor;
		}
	}
	if (!TestNotNull(TEXT("Materials anchor exists"), MaterialsAnchor) ||
		!TestNotNull(TEXT("Armory anchor exists"), ArmoryAnchor))
	{
		return false;
	}

	TestTrue(TEXT("Native BaseCamp anchor topology is valid"), PassesDataValidation(Base));
	TestTrue(TEXT("Different domains on one owner remain valid"), PassesDataValidation(MaterialsAnchor));

	const FName MaterialsId = MaterialsAnchor->GetAnchorId();
	const FName ArmoryId = ArmoryAnchor->GetAnchorId();
	FNameProperty* AnchorIdProperty = FindFProperty<FNameProperty>(
		URpgBaseStorageDomainAnchorComponent::StaticClass(),
		TEXT("AnchorId"));
	if (!TestNotNull(TEXT("Anchor id property is reflected"), AnchorIdProperty))
	{
		return false;
	}
	AnchorIdProperty->SetPropertyValue_InContainer(MaterialsAnchor, NAME_None);
	TestFalse(TEXT("BaseCamp rejects an empty required anchor id"), PassesDataValidation(Base));
	AnchorIdProperty->SetPropertyValue_InContainer(MaterialsAnchor, MaterialsId);

	TestTrue(
		TEXT("Materials anchor can be temporarily reconfigured for validation"),
		MaterialsAnchor->ConfigureAnchor(
			MaterialsId,
			RpgGameplayTags::Storage_Domain_Personal));
	TestFalse(TEXT("BaseCamp rejects a missing required Materials anchor"), PassesDataValidation(Base));
	TestTrue(
		TEXT("Materials anchor fixture restores its canonical domain"),
		MaterialsAnchor->ConfigureAnchor(
			MaterialsId,
			RpgGameplayTags::Storage_Domain_Materials));

	TestTrue(
		TEXT("Armory anchor can be temporarily assigned a duplicate id"),
		ArmoryAnchor->ConfigureAnchor(
			MaterialsId,
			RpgGameplayTags::Storage_Domain_Armory));
	TestFalse(TEXT("BaseCamp rejects repeated required anchor ids"), PassesDataValidation(Base));
	TestTrue(
		TEXT("Armory anchor fixture restores its stable id"),
		ArmoryAnchor->ConfigureAnchor(
			ArmoryId,
			RpgGameplayTags::Storage_Domain_Armory));
	TestTrue(TEXT("Restored required anchors validate again"), PassesDataValidation(Base));

	URpgBaseStorageDomainAnchorComponent* DuplicateMaterials =
		NewObject<URpgBaseStorageDomainAnchorComponent>(
			Base,
			TEXT("DuplicateMaterialsAnchor"),
			RF_Transient);
	if (!TestNotNull(TEXT("Duplicate-domain fixture exists"), DuplicateMaterials))
	{
		return false;
	}
	TestTrue(
		TEXT("Duplicate-domain fixture accepts a distinct stable id"),
		DuplicateMaterials->ConfigureAnchor(
			TEXT("SecondMaterialDepot"),
			RpgGameplayTags::Storage_Domain_Materials));
	Base->AddInstanceComponent(DuplicateMaterials);
	TestFalse(
		TEXT("Generic anchor validation rejects duplicate domains on one owner"),
		PassesDataValidation(DuplicateMaterials));
	TestFalse(
		TEXT("BaseCamp rejects more than one Materials anchor"),
		PassesDataValidation(Base));
	return true;
}
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgBaseStorageCommandAdmissionInvariantTest,
	"SurvivalRpg.Base.Storage.Transaction.CommandAdmissionReplayAndRequesterBinding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgBaseStorageCommandAdmissionInvariantTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgStorageFoundationTests;
	FScopedStorageWorld TestWorld;
	const FName BaseId(TEXT("Automation.CommandAdmission"));
	ARpgBaseCampActor* Base = nullptr;
	URpgBaseStorageComponent* Storage = nullptr;
	if (!MakeStorageFixture(*this, TestWorld, BaseId, Base, Storage))
	{
		return false;
	}

	APlayerController* Requester =
		TestWorld.SpawnController(TEXT("StorageRequester"));
	APlayerController* ForeignRequester =
		TestWorld.SpawnController(TEXT("ForeignStorageRequester"));
	if (!TestNotNull(TEXT("Primary requester exists"), Requester) ||
		!TestNotNull(TEXT("Foreign requester exists"), ForeignRequester))
	{
		return false;
	}

	const int64 InitialRevision = Storage->GetNetworkRevision();
	FRpgBaseStorageRequestContext Context;
	Context.RequestId = FGuid::NewGuid();
	Context.BaseId = BaseId;
	Context.ExpectedNetworkRevision = InitialRevision;
	constexpr uint32 PayloadHash = 0xA117D00Du;
	FRpgBaseStorageCommandResult Admission;
	if (!TestTrue(
		TEXT("A fresh exact-revision command is admitted"),
		Storage->AdmitCommand(
			Context,
			PayloadHash,
			Requester,
			Admission)))
	{
		return false;
	}

	TestTrue(
		TEXT("An admitted command may mutate strain"),
		Storage->TryAddRiftStrain(15));
	TestEqual(
		TEXT("Mutations remain coalesced until command completion"),
		Storage->GetNetworkRevision(),
		InitialRevision);
	const FRpgBaseStorageCommandResult Completed = Storage->CompleteCommand(
		Context,
		PayloadHash,
		ERpgBaseStorageResultCode::Success);
	TestEqual(
		TEXT("One committed command advances the network revision exactly once"),
		Storage->GetNetworkRevision(),
		InitialRevision + 1);
	TestEqual(
		TEXT("The result exposes the committed network revision"),
		Completed.NetworkRevision,
		InitialRevision + 1);
	TestEqual(
		TEXT("The result preserves the caller RequestId"),
		Completed.RequestId,
		Context.RequestId);

	FRpgBaseStorageCommandResult Replay;
	TestFalse(
		TEXT("An exact replay is served without re-admission"),
		Storage->AdmitCommand(
			Context,
			PayloadHash,
			Requester,
			Replay));
	TestEqual(
		TEXT("The exact replay returns the cached success"),
		Replay.Code,
		ERpgBaseStorageResultCode::Success);
	TestEqual(
		TEXT("Replay cannot advance the monotone network revision"),
		Storage->GetNetworkRevision(),
		InitialRevision + 1);
	TestEqual(
		TEXT("Replay cannot apply strain a second time"),
		Storage->GetCleanseableRiftStrain(),
		15);

	FRpgBaseStorageCommandResult PayloadCollision;
	TestFalse(
		TEXT("Reusing a RequestId with a different payload is rejected"),
		Storage->AdmitCommand(
			Context,
			PayloadHash + 1,
			Requester,
			PayloadCollision));
	TestEqual(
		TEXT("A RequestId/payload collision reports Conflict"),
		PayloadCollision.Code,
		ERpgBaseStorageResultCode::Conflict);

	FRpgBaseStorageCommandResult RequesterCollision;
	TestFalse(
		TEXT("A second requester cannot replay another player's RequestId"),
		Storage->AdmitCommand(
			Context,
			PayloadHash,
			ForeignRequester,
			RequesterCollision));
	TestEqual(
		TEXT("A RequestId/requester collision reports Conflict"),
		RequesterCollision.Code,
		ERpgBaseStorageResultCode::Conflict);

	FRpgBaseStorageRequestContext StaleContext;
	StaleContext.RequestId = FGuid::NewGuid();
	StaleContext.BaseId = BaseId;
	StaleContext.ExpectedNetworkRevision = InitialRevision;
	FRpgBaseStorageCommandResult StaleResult;
	TestFalse(
		TEXT("A fresh request using a spent network revision is rejected"),
		Storage->AdmitCommand(
			StaleContext,
			0x51A1Eu,
			Requester,
			StaleResult));
	TestEqual(
		TEXT("A spent optimistic revision reports Stale"),
		StaleResult.Code,
		ERpgBaseStorageResultCode::Stale);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgBaseStorageInternalRollbackTaintTest,
	"SurvivalRpg.Base.Storage.Transaction.InternalRollbackTaintsUntilRestore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgBaseStorageInternalRollbackTaintTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgStorageFoundationTests;
	FScopedStorageWorld TestWorld;
	const FName BaseId(TEXT("Automation.RollbackTaint"));
	ARpgBaseCampActor* Base = nullptr;
	URpgBaseStorageComponent* Storage = nullptr;
	if (!MakeStorageFixture(*this, TestWorld, BaseId, Base, Storage))
	{
		return false;
	}

	APlayerController* Requester =
		TestWorld.SpawnController(TEXT("RollbackRequester"));
	if (!TestNotNull(TEXT("Rollback requester exists"), Requester))
	{
		return false;
	}

	const int64 InitialRevision = Storage->GetNetworkRevision();
	FRpgBaseStorageRequestContext Context;
	Context.RequestId = FGuid::NewGuid();
	Context.BaseId = BaseId;
	Context.ExpectedNetworkRevision = InitialRevision;
	constexpr uint32 PayloadHash = 0xFA11BACCu;
	FRpgBaseStorageCommandResult Admission;
	if (!TestTrue(
		TEXT("Rollback test command is admitted"),
		Storage->AdmitCommand(
			Context,
			PayloadHash,
			Requester,
			Admission)))
	{
		return false;
	}

	const FRpgBaseStorageCommandResult Failed = Storage->CompleteCommand(
		Context,
		PayloadHash,
		ERpgBaseStorageResultCode::InternalRollback);
	TestEqual(
		TEXT("Rollback failure remains explicit in the result"),
		Failed.Code,
		ERpgBaseStorageResultCode::InternalRollback);
	TestTrue(
		TEXT("InternalRollback fail-closes the storage network"),
		Storage->IsMutationTainted());
	TestEqual(
		TEXT("Taint completion advances the revision exactly once"),
		Storage->GetNetworkRevision(),
		InitialRevision + 1);
	TestFalse(
		TEXT("Taint blocks direct Rift mutation"),
		Storage->TryAddRiftStrain(10));
	TestFalse(
		TEXT("Taint blocks direct capacity mutation"),
		Storage->AddMaterialCapacityPoints(1));
	TestEqual(
		TEXT("Blocked direct mutations cannot advance revision"),
		Storage->GetNetworkRevision(),
		InitialRevision + 1);

	FRpgBaseStorageRequestContext BlockedContext;
	BlockedContext.RequestId = FGuid::NewGuid();
	BlockedContext.BaseId = BaseId;
	BlockedContext.ExpectedNetworkRevision = Storage->GetNetworkRevision();
	FRpgBaseStorageCommandResult Blocked;
	TestFalse(
		TEXT("Taint blocks fresh command admission"),
		Storage->AdmitCommand(
			BlockedContext,
			0xB10CCEDu,
			Requester,
			Blocked));
	TestEqual(
		TEXT("Taint admission failure reports InternalRollback"),
		Blocked.Code,
		ERpgBaseStorageResultCode::InternalRollback);

	FRpgBaseStorageSaveData CleanSave;
	FString RestoreError;
	TestTrue(
		TEXT("A fully validated storage restore clears fatal taint"),
		Storage->RestoreStorageState(CleanSave, RestoreError));
	TestFalse(
		TEXT("Successful restore reopens the mutation epoch"),
		Storage->IsMutationTainted());
	TestTrue(
		TEXT("Direct mutation succeeds again after restore"),
		Storage->TryAddRiftStrain(10));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgBaseStorageOverfullResourceCheckpointTest,
	"SurvivalRpg.Base.Storage.Transaction.ResourceCheckpointRestoresOverfullLedgerExactly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgBaseStorageOverfullResourceCheckpointTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgStorageFoundationTests;
	FScopedStorageWorld TestWorld;
	ARpgBaseCampActor* Base = nullptr;
	URpgBaseStorageComponent* Storage = nullptr;
	if (!MakeStorageFixture(
			*this,
			TestWorld,
			FName(TEXT("Automation.OverfullCheckpoint")),
			Base,
			Storage))
	{
		return false;
	}

	FRpgBaseStorageSaveData OverfullSave;
	FRpgBaseStorageBulkSaveEntry& SavedMaterial =
		OverfullSave.BulkEntries.AddDefaulted_GetRef();
	SavedMaterial.ItemDefinition =
		URpgInventoryAutomationTestMaterialDefinition::StaticClass();
	SavedMaterial.Count = 350;
	SavedMaterial.SortIndex = 7;
	FString RestoreError;
	if (!TestTrue(
		TEXT("A rebalanced overfull bulk row restores without item loss"),
		Storage->RestoreStorageState(OverfullSave, RestoreError)))
	{
		AddError(RestoreError);
		return false;
	}

	const TSubclassOf<URpgInventoryItemDefinition> MaterialDefinition =
		URpgInventoryAutomationTestMaterialDefinition::StaticClass();
	TestEqual(
		TEXT("The overfull restore preserves every material unit"),
		Storage->GetResourceCount(MaterialDefinition),
		350);
	TestTrue(
		TEXT("The preserved row is visibly over shared capacity"),
		Storage->IsMaterialDomainOverCapacity());
	TestFalse(
		TEXT("Overfull storage rejects additional deposits"),
		Storage->StoreDefinitionResource(MaterialDefinition, 1));

	FRpgBaseResourceMutationCheckpoint Checkpoint;
	if (!TestTrue(
		TEXT("The exact overfull resource row can be checkpointed"),
		Storage->CaptureResourceMutationCheckpoint(
			MaterialDefinition,
			Checkpoint)))
	{
		return false;
	}
	const TArray<FRpgBaseResourceEntryView> BeforeRows =
		Storage->GetAllResources();
	const int64 RevisionBeforeDebit = Storage->GetNetworkRevision();
	TestTrue(
		TEXT("A transaction may debit preserved overfull resources"),
		Storage->WithdrawResource(MaterialDefinition, 75));
	TestEqual(
		TEXT("The debit mutates the exact row"),
		Storage->GetResourceCount(MaterialDefinition),
		275);
	TestTrue(
		TEXT("Rollback restores the exact checkpoint without capacity admission"),
		Storage->RestoreResourceMutationCheckpoint(Checkpoint));
	TestEqual(
		TEXT("Rollback restores the original overfull count"),
		Storage->GetResourceCount(MaterialDefinition),
		350);
	TestTrue(
		TEXT("Rollback preserves the intentional overfull marker"),
		Storage->IsMaterialDomainOverCapacity());
	TestEqual(
		TEXT("Debit and rollback each advance the monotone revision"),
		Storage->GetNetworkRevision(),
		RevisionBeforeDebit + 2);
	const TArray<FRpgBaseResourceEntryView> AfterRows =
		Storage->GetAllResources();
	if (TestEqual(
			TEXT("Rollback preserves the resource-row cardinality"),
			AfterRows.Num(),
			BeforeRows.Num()) &&
		BeforeRows.Num() == 1)
	{
		TestEqual(
			TEXT("Rollback restores the exact legacy sort key"),
			AfterRows[0].SortIndex,
			BeforeRows[0].SortIndex);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgBaseStorageMultiplePackagesPerAnchorTest,
	"SurvivalRpg.Base.Storage.Upgrades.MultiplePackagesShareFixedAnchor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgBaseStorageMultiplePackagesPerAnchorTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgStorageFoundationTests;
	FScopedStorageWorld TestWorld;
	ARpgBaseCampActor* Base = nullptr;
	URpgBaseStorageComponent* Storage = nullptr;
	if (!MakeStorageFixture(
			*this,
			TestWorld,
			FName(TEXT("Automation.StackedAnchorPackages")),
			Base,
			Storage))
	{
		return false;
	}

	URpgBaseStorageUpgradeDefinition* QuartermasterOne =
		NewObject<URpgBaseStorageUpgradeDefinition>(Storage);
	URpgBaseStorageUpgradeDefinition* QuartermasterTwo =
		NewObject<URpgBaseStorageUpgradeDefinition>(Storage);
	if (!TestNotNull(TEXT("Quartermaster I fixture exists"), QuartermasterOne) ||
		!TestNotNull(TEXT("Quartermaster II fixture exists"), QuartermasterTwo))
	{
		return false;
	}

	for (URpgBaseStorageUpgradeDefinition* Upgrade :
		{QuartermasterOne, QuartermasterTwo})
	{
		Upgrade->TargetDomainTag =
			RpgGameplayTags::Storage_Domain_Materials;
		Upgrade->TargetAnchorId = TEXT("MaterialDepot");
	}
	QuartermasterOne->CapacityEffect.AdditionalCapacity = 300;
	QuartermasterTwo->CapacityEffect.AdditionalCapacity = 300;
	QuartermasterOne->GrantedCapabilityTags.AddTag(
		RpgGameplayTags::Storage_Capability_ExpeditionPreset);
	QuartermasterTwo->RequiredInstalledCapabilityTags.AddTag(
		RpgGameplayTags::Storage_Capability_ExpeditionPreset);
	QuartermasterTwo->GrantedCapabilityTags.AddTag(
		RpgGameplayTags::Storage_Capability_ProjectReservations);

	TestTrue(TEXT("Quartermaster I installs on MaterialDepot"),
		Storage->InstallUpgrade(QuartermasterOne));
	TestTrue(TEXT("Quartermaster II can target the same fixed anchor"),
		Storage->InstallUpgrade(QuartermasterTwo));
	TestEqual(TEXT("Both capacity packages contribute cumulatively"),
		Storage->GetMaterialCapacityPoints(), 900);
	TestTrue(TEXT("Both packages remain independently installed"),
		Storage->HasInstalledUpgrade(QuartermasterOne) &&
		Storage->HasInstalledUpgrade(QuartermasterTwo));
	TestFalse(TEXT("Quartermaster I cannot be removed while II depends on it"),
		Storage->DecommissionUpgrade(QuartermasterOne));
	TestTrue(TEXT("Quartermaster II can be removed first"),
		Storage->DecommissionUpgrade(QuartermasterTwo));
	TestTrue(TEXT("Quartermaster I can then be removed"),
		Storage->DecommissionUpgrade(QuartermasterOne));
	TestEqual(TEXT("Removing both packages restores base capacity"),
		Storage->GetMaterialCapacityPoints(), 300);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgBaseStorageRawAndPassiveStrainTest,
	"SurvivalRpg.Base.Storage.Rift.CleanseOnlyRawStrainAndCheckpointRestore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgBaseStorageRawAndPassiveStrainTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgStorageFoundationTests;
	FScopedStorageWorld TestWorld;
	ARpgBaseCampActor* Base = nullptr;
	URpgBaseStorageComponent* Storage = nullptr;
	if (!MakeStorageFixture(
			*this,
			TestWorld,
			FName(TEXT("Automation.RiftStrain")),
			Base,
			Storage))
	{
		return false;
	}

	URpgBaseStorageUpgradeDefinition* PassiveUpgrade =
		NewObject<URpgBaseStorageUpgradeDefinition>(
			Storage,
			TEXT("AutomationPassiveRiftUpgrade"),
			RF_Transient);
	if (!TestNotNull(TEXT("Passive Rift upgrade fixture exists"), PassiveUpgrade))
	{
		return false;
	}
	PassiveUpgrade->TargetDomainTag =
		RpgGameplayTags::Storage_Domain_RiftContainment;
	PassiveUpgrade->TargetAnchorId = TEXT("RiftVault");
	PassiveUpgrade->StrainEffect.AddedStrain = 20.0f;
	if (!TestTrue(
		TEXT("The passive Rift upgrade installs"),
		Storage->InstallUpgrade(PassiveUpgrade)))
	{
		return false;
	}

	TestEqual(
		TEXT("Passive burden contributes to effective UI strain"),
		Storage->GetRiftStrain(),
		20);
	TestEqual(
		TEXT("Passive burden is not cleanseable raw strain"),
		Storage->GetCleanseableRiftStrain(),
		0);
	TestFalse(
		TEXT("Cleanse cannot erase passive-only strain"),
		Storage->CleanseRiftStrain(25));
	TestEqual(
		TEXT("Rejected passive-only cleanse preserves effective strain"),
		Storage->GetRiftStrain(),
		20);

	TestTrue(
		TEXT("Authored strain adds to the cleanseable raw component"),
		Storage->TryAddRiftStrain(30));
	TestEqual(
		TEXT("Raw strain is tracked independently"),
		Storage->GetCleanseableRiftStrain(),
		30);
	TestEqual(
		TEXT("Effective strain combines raw and passive components"),
		Storage->GetRiftStrain(),
		50);
	const int32 RawCheckpoint = Storage->GetCleanseableRiftStrain();
	TestTrue(
		TEXT("Cleanse debits only the raw component"),
		Storage->CleanseRiftStrain(25));
	TestEqual(
		TEXT("Cleanse leaves five raw points"),
		Storage->GetCleanseableRiftStrain(),
		5);
	TestEqual(
		TEXT("Passive burden remains after raw cleanse"),
		Storage->GetRiftStrain(),
		25);
	TestTrue(
		TEXT("Rollback restores the captured raw strain exactly"),
		Storage->RestoreRiftStrainCheckpoint(RawCheckpoint));
	TestEqual(
		TEXT("Raw rollback restores the pre-cleanse value"),
		Storage->GetCleanseableRiftStrain(),
		30);
	TestEqual(
		TEXT("Raw rollback recomposes the same effective strain"),
		Storage->GetRiftStrain(),
		50);
	TestFalse(
		TEXT("Out-of-range rollback checkpoints fail closed"),
		Storage->RestoreRiftStrainCheckpoint(101));
	TestEqual(
		TEXT("A rejected rollback checkpoint cannot mutate raw strain"),
		Storage->GetCleanseableRiftStrain(),
		30);
	return true;
}

#endif
