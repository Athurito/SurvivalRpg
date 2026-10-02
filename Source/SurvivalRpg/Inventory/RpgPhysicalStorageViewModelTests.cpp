#if WITH_DEV_AUTOMATION_TESTS

#include "SurvivalRpg/Mvvm/Inventory/RpgPhysicalStorageViewModel.h"
#include "RpgInventoryContainerActor.h"
#include "RpgInventoryContainerComponent.h"
#include "RpgInventoryManagerComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

namespace RpgPhysicalStorageViewModelTests
{
	struct FWorld
	{
		UGameInstance* Instance = nullptr;
		UWorld* World = nullptr;
		FWorld()
		{
			Instance = NewObject<UGameInstance>(GEngine, NAME_None, RF_Transient);
			Instance->AddToRoot();
			Instance->InitializeStandalone();
			World = Instance->GetWorld();
		}
		~FWorld()
		{
			Instance->Shutdown();
			if (World) { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); }
			Instance->RemoveFromRoot();
		}
	};

	FRpgInventoryGridSize Grid(int32 Width, int32 Height)
	{
		FRpgInventoryGridSize Result;
		Result.Width = Width;
		Result.Height = Height;
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgPhysicalStorageCapacityViewModelTest,
	"SurvivalRpg.Inventory.PhysicalStorage.ViewModelCapacityReplicationOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgPhysicalStorageCapacityViewModelTest::RunTest(const FString& Parameters)
{
	using namespace RpgPhysicalStorageViewModelTests;
	FWorld Scope;
	if (!TestNotNull(TEXT("Test world exists"), Scope.World)) { return false; }
	ARpgInventoryContainerActor* ChestA = Scope.World->SpawnActor<ARpgInventoryContainerActor>();
	ARpgInventoryContainerActor* ChestB = Scope.World->SpawnActor<ARpgInventoryContainerActor>();
	if (!TestNotNull(TEXT("Observed chest exists"), ChestA) || !TestNotNull(TEXT("Unrelated chest exists"), ChestB)) { return false; }
	URpgInventoryContainerComponent* ContainerA = ChestA->GetContainerComponent();
	URpgInventoryContainerComponent* ContainerB = ChestB->GetContainerComponent();
	URpgInventoryManagerComponent* InventoryA = ChestA->GetInventoryManager();
	URpgInventoryManagerComponent* InventoryB = ChestB->GetInventoryManager();
	ContainerA->EnsurePersistentContainerId();
	ContainerB->EnsurePersistentContainerId();
	TestTrue(TEXT("Initial observed grid is configured"), InventoryA->SetDefaultGridSize(Grid(4, 3)));
	TestTrue(TEXT("Initial unrelated grid is configured"), InventoryB->SetDefaultGridSize(Grid(2, 2)));
	TStrongObjectPtr<URpgPhysicalStorageViewModel> ViewModel(NewObject<URpgPhysicalStorageViewModel>());
	const FIntProperty* WidthProperty = FindFProperty<FIntProperty>(ViewModel->GetClass(), TEXT("GridWidth"));
	const FIntProperty* HeightProperty = FindFProperty<FIntProperty>(ViewModel->GetClass(), TEXT("GridHeight"));
	const FStructProperty* GridProperty = FindFProperty<FStructProperty>(InventoryA->GetClass(), TEXT("DefaultGridSize"));
	UFunction* CapacityOnRep = InventoryA->FindFunction(TEXT("OnRep_CapacitySettings"));
	if (!TestNotNull(TEXT("Grid width is reflected"), WidthProperty) ||
		!TestNotNull(TEXT("Grid height is reflected"), HeightProperty) ||
		!TestNotNull(TEXT("Inventory grid is reflected"), GridProperty) ||
		!TestNotNull(TEXT("Capacity replication callback exists"), CapacityOnRep)) { return false; }
	auto Width = [&]() { return WidthProperty->GetPropertyValue_InContainer(ViewModel.Get()); };
	auto Height = [&]() { return HeightProperty->GetPropertyValue_InContainer(ViewModel.Get()); };
	ViewModel->BindContainer(ContainerA);
	TestEqual(TEXT("Binding reads actual initial width"), Width(), 4);
	TestEqual(TEXT("Binding reads actual initial height"), Height(), 3);

	int32 SettingsNotifications = 0;
	const FDelegateHandle SettingsHandle = ContainerA->OnPhysicalStorageSettingsChanged.AddLambda(
		[&](URpgInventoryContainerComponent*) { ++SettingsNotifications; });
	const int32 SettingsRevision = ContainerA->GetSettingsRevision();
	// Reproduce metadata arriving first, followed by the independent inventory property and its OnRep.
	ContainerA->OnPhysicalStorageSettingsChanged.Broadcast(ContainerA);
	SettingsNotifications = 0;
	*GridProperty->ContainerPtrToValuePtr<FRpgInventoryGridSize>(InventoryA) = Grid(8, 6);
	TestTrue(TEXT("Unrelated capacity update is published"), InventoryB->SetDefaultGridSize(Grid(3, 3)));
	TestEqual(TEXT("Unrelated capacity does not refresh the observed pending grid"), Width(), 4);
	InventoryA->ProcessEvent(CapacityOnRep, nullptr);
	TestEqual(TEXT("Late capacity OnRep refreshes width without another metadata notification"), Width(), 8);
	TestEqual(TEXT("Late capacity OnRep refreshes height without another metadata notification"), Height(), 6);
	TestEqual(TEXT("Capacity does not change the settings revision"), ContainerA->GetSettingsRevision(), SettingsRevision);
	TestEqual(TEXT("Capacity refresh requires no container settings notification"), SettingsNotifications, 0);

	TestTrue(TEXT("Authority capacity-only change is accepted"), InventoryA->SetDefaultGridSize(Grid(9, 7)));
	TestEqual(TEXT("Authority capacity-only message refreshes width"), Width(), 9);
	TestEqual(TEXT("Authority capacity-only message refreshes height"), Height(), 7);
	TestEqual(TEXT("Authority capacity change emits no settings notification"), SettingsNotifications, 0);
	ContainerA->OnPhysicalStorageSettingsChanged.Remove(SettingsHandle);

	ViewModel->BindContainer(ContainerB);
	TestEqual(TEXT("Rebinding projects the second chest"), Width(), 3);
	*GridProperty->ContainerPtrToValuePtr<FRpgInventoryGridSize>(InventoryB) = Grid(5, 4);
	TestTrue(TEXT("Former observed chest can still change"), InventoryA->SetDefaultGridSize(Grid(10, 8)));
	TestEqual(TEXT("Former container capacity is ignored after rebinding"), Width(), 3);
	InventoryB->ProcessEvent(CapacityOnRep, nullptr);
	TestEqual(TEXT("Newly bound capacity notification updates the model"), Width(), 5);
	ViewModel->UnbindContainer();
	TestEqual(TEXT("Unbinding clears the projected grid"), Width(), 0);
	TestTrue(TEXT("Unbound chest remains independently mutable"), InventoryB->SetDefaultGridSize(Grid(6, 5)));
	TestEqual(TEXT("Unbound capacity notification leaves width cleared"), Width(), 0);
	TestEqual(TEXT("Unbound capacity notification leaves height cleared"), Height(), 0);
	TestFalse(TEXT("Unbound model no longer presents physical storage"), ViewModel->IsPhysicalStorage());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
