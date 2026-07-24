#include "RpgInventoryAutomationTestTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "RpgInventoryItemInstance.h"
#include "RpgInventoryManagerComponent.h"
#include "RpgPlayerInventoryLayoutComponent.h"
#include "SurvivalRpg/Mvvm/Base/RpgBaseStorageViewModels.h"
#include "SurvivalRpg/Mvvm/Crafting/RpgCraftingViewModels.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventoryNestedContainerViewModel.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventoryViewModels.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgPlayerInventoryViewModels.h"

#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "INotifyFieldValueChanged.h"
#include "Misc/AutomationTest.h"
#include "MVVMViewModelBase.h"
#include "UObject/Package.h"
#include "UObject/TextProperty.h"
#include "UObject/UnrealType.h"

namespace RpgInventoryViewModelFieldNotifyTests
{
	class FScopedInventoryWorld
	{
	public:
		FScopedInventoryWorld()
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

		~FScopedInventoryWorld()
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
			return GameInstance != nullptr && World != nullptr;
		}

		UWorld* GetWorld() const
		{
			return World;
		}

		URpgInventoryManagerComponent* CreateInventory(
			const TCHAR* DebugName) const
		{
			if (!World)
			{
				return nullptr;
			}

			FActorSpawnParameters SpawnParameters;
			SpawnParameters.Name = MakeUniqueObjectName(
				World,
				AActor::StaticClass(),
				FName(DebugName));
			SpawnParameters.ObjectFlags = RF_Transient;
			AActor* OwnerActor = World->SpawnActor<AActor>(SpawnParameters);
			if (!OwnerActor)
			{
				return nullptr;
			}

			URpgInventoryManagerComponent* Inventory =
				NewObject<URpgInventoryManagerComponent>(
					OwnerActor,
					MakeUniqueObjectName(
						OwnerActor,
						URpgInventoryManagerComponent::StaticClass(),
						TEXT("Inventory")),
					RF_Transient);
			OwnerActor->AddInstanceComponent(Inventory);
			Inventory->RegisterComponent();
			return Inventory;
		}

	private:
		TObjectPtr<UGameInstance> GameInstance;
		TObjectPtr<UWorld> World;
	};

	struct FFieldNotifySubscription
	{
		UE::FieldNotification::FFieldId FieldId;
		FDelegateHandle Handle;
	};

	/**
	 * Counts every native FieldNotify exposed by one concrete view model.
	 *
	 * Raw callbacks are removed explicitly so a later BeginDestroy refresh can
	 * never call into a counter whose automation-test stack frame has ended.
	 */
	class FFieldNotifyCounter
	{
	public:
		explicit FFieldNotifyCounter(UMVVMViewModelBase* InViewModel)
		{
			Bind(InViewModel);
		}

		~FFieldNotifyCounter()
		{
			Unbind();
		}

		FFieldNotifyCounter(const FFieldNotifyCounter&) = delete;
		FFieldNotifyCounter& operator=(const FFieldNotifyCounter&) = delete;

		void Bind(UMVVMViewModelBase* InViewModel)
		{
			Unbind();
			ViewModel = InViewModel;
			if (!InViewModel)
			{
				++BindingFailureCount;
				return;
			}

			InViewModel->GetFieldNotificationDescriptor().ForEachField(
				InViewModel->GetClass(),
				[this, InViewModel](
					UE::FieldNotification::FFieldId FieldId)
				{
					if (!FieldId.IsValid())
					{
						++BindingFailureCount;
						return true;
					}

					const FDelegateHandle Handle =
						InViewModel->AddFieldValueChangedDelegate(
							FieldId,
							INotifyFieldValueChanged::
								FFieldValueChangedDelegate::CreateRaw(
									this,
									&FFieldNotifyCounter::
										HandleFieldValueChanged));
					if (!Handle.IsValid())
					{
						++BindingFailureCount;
						return true;
					}

					FFieldNotifySubscription& Subscription =
						Subscriptions.AddDefaulted_GetRef();
					Subscription.FieldId = FieldId;
					Subscription.Handle = Handle;
					Counts.FindOrAdd(FieldId.GetName()) = 0;
					return true;
				});
		}

		void Unbind()
		{
			if (UMVVMViewModelBase* BoundViewModel = ViewModel.Get())
			{
				for (const FFieldNotifySubscription& Subscription :
					 Subscriptions)
				{
					BoundViewModel->RemoveFieldValueChangedDelegate(
						Subscription.FieldId,
						Subscription.Handle);
				}
			}

			ViewModel.Reset();
			Subscriptions.Reset();
			Counts.Reset();
			FirstNotificationCallback.Reset();
			UnexpectedSourceCount = 0;
			BindingFailureCount = 0;
			bFirstNotificationObserved = false;
		}

		void Reset()
		{
			for (TPair<FName, int32>& Count : Counts)
			{
				Count.Value = 0;
			}
			UnexpectedSourceCount = 0;
			bFirstNotificationObserved = false;
		}

		void SetFirstNotificationCallback(
			TFunction<void(
				UObject*,
				UE::FieldNotification::FFieldId)> InCallback)
		{
			FirstNotificationCallback = MoveTemp(InCallback);
			bFirstNotificationObserved = false;
		}

		bool HasCompleteBinding() const
		{
			return ViewModel.IsValid() &&
				!Subscriptions.IsEmpty() &&
				BindingFailureCount == 0;
		}

		int32 GetFieldCount(FName FieldName) const
		{
			if (const int32* Count = Counts.Find(FieldName))
			{
				return *Count;
			}
			return 0;
		}

		int32 GetTotalCount() const
		{
			int32 Total = 0;
			for (const TPair<FName, int32>& Count : Counts)
			{
				Total += Count.Value;
			}
			return Total;
		}

		int32 GetUnexpectedSourceCount() const
		{
			return UnexpectedSourceCount;
		}

	private:
		void HandleFieldValueChanged(
			UObject* ChangedObject,
			UE::FieldNotification::FFieldId FieldId)
		{
			if (ChangedObject != ViewModel.Get())
			{
				++UnexpectedSourceCount;
			}
			++Counts.FindOrAdd(FieldId.GetName());
			if (!bFirstNotificationObserved)
			{
				bFirstNotificationObserved = true;
				if (FirstNotificationCallback)
				{
					FirstNotificationCallback(
						ChangedObject,
						FieldId);
				}
			}
		}

		TWeakObjectPtr<UMVVMViewModelBase> ViewModel;
		TArray<FFieldNotifySubscription> Subscriptions;
		TMap<FName, int32> Counts;
		TFunction<void(
			UObject*,
			UE::FieldNotification::FFieldId)>
			FirstNotificationCallback;
		int32 UnexpectedSourceCount = 0;
		int32 BindingFailureCount = 0;
		bool bFirstNotificationObserved = false;
	};

	using FExpectedFieldCount = TPair<FName, int32>;

	bool VerifyFieldCounts(
		FAutomationTestBase& Test,
		const TCHAR* Phase,
		const FFieldNotifyCounter& Counter,
		std::initializer_list<FExpectedFieldCount> ExpectedFields)
	{
		int32 ExpectedTotal = 0;
		for (const FExpectedFieldCount& Expected : ExpectedFields)
		{
			ExpectedTotal += Expected.Value;
		}

		bool bResult = true;
		const FString TotalLabel =
			FString::Printf(TEXT("%s: total notification count"), Phase);
		bResult &= Test.TestEqual(
			*TotalLabel,
			Counter.GetTotalCount(),
			ExpectedTotal);

		for (const FExpectedFieldCount& Expected : ExpectedFields)
		{
			const FString FieldLabel = FString::Printf(
				TEXT("%s: %s notification count"),
				Phase,
				*Expected.Key.ToString());
			bResult &= Test.TestEqual(
				*FieldLabel,
				Counter.GetFieldCount(Expected.Key),
				Expected.Value);
		}

		const FString SourceLabel =
			FString::Printf(TEXT("%s: every callback came from the bound VM"), Phase);
		bResult &= Test.TestEqual(
			*SourceLabel,
			Counter.GetUnexpectedSourceCount(),
			0);
		return bResult;
	}

	FRpgInventoryGridPlacement MakePlacement(
		const FRpgInventoryContainerHandle& ContainerHandle,
		int32 X,
		int32 Y)
	{
		FRpgInventoryGridPlacement Placement;
		Placement.SetContainerHandle(ContainerHandle);
		Placement.X = X;
		Placement.Y = Y;
		return Placement;
	}

	FRpgInventorySlotGroupView MakeFieldNotifyGroupView(
		const FText& DisplayName)
	{
		FRpgInventorySlotGroupView GroupView;
		GroupView.ContainerHandle =
			FRpgInventoryContainerHandle::MakeRoot(
				TEXT("FieldNotify.Content"));
		GroupView.ContainerId = TEXT("FieldNotify.Content");
		GroupView.DisplayName = DisplayName;
		GroupView.GroupKind = ERpgInventorySlotGroupKind::Content;
		GroupView.GridSize.Width = 2;
		GroupView.GridSize.Height = 1;
		return GroupView;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryEntryViewModelFieldNotifyTest,
	"SurvivalRpg.Inventory.ViewModel.FieldNotify.EntryOnlyChangedFields",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryEntryViewModelFieldNotifyTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryViewModelFieldNotifyTests;

	USceneComponent* InventoryOwner =
		NewObject<USceneComponent>(
			GetTransientPackage(),
			NAME_None,
			RF_Transient);
	URpgInventoryEntryViewModel* ViewModel =
		NewObject<URpgInventoryEntryViewModel>(
			InventoryOwner,
			NAME_None,
			RF_Transient);
	if (!TestNotNull(TEXT("The empty-entry owner exists"), InventoryOwner) ||
		!TestNotNull(TEXT("The entry VM exists"), ViewModel))
	{
		return false;
	}

	const FRpgInventoryContainerHandle ContainerHandle =
		FRpgInventoryContainerHandle::MakeRoot(
			TEXT("FieldNotify.Entry"));
	const FRpgInventoryGridPlacement InitialPlacement =
		MakePlacement(ContainerHandle, 0, 0);
	ViewModel->InitializeEmptySlot(
		InventoryOwner,
		InitialPlacement);

	FFieldNotifyCounter Counter(ViewModel);
	if (!TestTrue(
			TEXT("The entry counter subscribes to every FieldNotify field"),
			Counter.HasCompleteBinding()))
	{
		return false;
	}

	ViewModel->InitializeEmptySlot(
		InventoryOwner,
		InitialPlacement);
	VerifyFieldCounts(
		*this,
		TEXT("Identical empty-entry refresh"),
		Counter,
		{});

	Counter.Reset();
	const FRpgInventoryGridPlacement MovedPlacement =
		MakePlacement(ContainerHandle, 1, 0);
	ViewModel->InitializeEmptySlot(
		InventoryOwner,
		MovedPlacement);
	VerifyFieldCounts(
		*this,
		TEXT("Placement-only empty-entry refresh"),
		Counter,
		{
			{FName(TEXT("Placement")), 1},
		});

	Counter.Reset();
	ViewModel->InitializeEmptySlot(
		InventoryOwner,
		MovedPlacement);
	VerifyFieldCounts(
		*this,
		TEXT("Repeated moved empty-entry refresh"),
		Counter,
		{});

	Counter.Unbind();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryAddressAndGroupViewModelFieldNotifyTest,
	"SurvivalRpg.Inventory.ViewModel.FieldNotify.AddressAndGroupOnlyChangedFields",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryAddressAndGroupViewModelFieldNotifyTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryViewModelFieldNotifyTests;

	URpgInventoryAddressSlotViewModel* FirstAddress =
		NewObject<URpgInventoryAddressSlotViewModel>(
			GetTransientPackage(),
			NAME_None,
			RF_Transient);
	URpgInventorySlotGroupViewModel* GroupViewModel =
		NewObject<URpgInventorySlotGroupViewModel>(
			GetTransientPackage(),
			NAME_None,
			RF_Transient);
	if (!TestNotNull(TEXT("The first address VM exists"), FirstAddress) ||
		!TestNotNull(TEXT("The group VM exists"), GroupViewModel))
	{
		return false;
	}

	const FRpgInventorySlotGroupView InitialGroup =
		MakeFieldNotifyGroupView(
			FText::FromString(TEXT("FieldNotify Content")));
	FirstAddress->InitializeSlot(
		nullptr,
		nullptr,
		InitialGroup,
		0,
		0);
	TArray<URpgInventoryAddressSlotViewModel*> OneSlot;
	OneSlot.Add(FirstAddress);
	GroupViewModel->InitializeGroup(InitialGroup, OneSlot);

	FFieldNotifyCounter AddressCounter(FirstAddress);
	FFieldNotifyCounter GroupCounter(GroupViewModel);
	if (!TestTrue(
			TEXT("The address counter subscribes to every FieldNotify field"),
			AddressCounter.HasCompleteBinding()) ||
		!TestTrue(
			TEXT("The group counter subscribes to every FieldNotify field"),
			GroupCounter.HasCompleteBinding()))
	{
		return false;
	}

	FirstAddress->InitializeSlot(
		nullptr,
		nullptr,
		InitialGroup,
		0,
		0);
	GroupViewModel->InitializeGroup(InitialGroup, OneSlot);
	VerifyFieldCounts(
		*this,
		TEXT("Identical address refresh"),
		AddressCounter,
		{});
	VerifyFieldCounts(
		*this,
		TEXT("Identical group refresh"),
		GroupCounter,
		{});

	AddressCounter.Reset();
	GroupCounter.Reset();
	FRpgInventorySlotGroupView RenamedGroup = InitialGroup;
	RenamedGroup.DisplayName =
		FText::FromString(TEXT("Renamed FieldNotify Content"));
	FirstAddress->InitializeSlot(
		nullptr,
		nullptr,
		RenamedGroup,
		0,
		0);
	GroupViewModel->InitializeGroup(RenamedGroup, OneSlot);
	VerifyFieldCounts(
		*this,
		TEXT("Address label-only refresh"),
		AddressCounter,
		{
			{FName(TEXT("SlotLabel")), 1},
		});
	VerifyFieldCounts(
		*this,
		TEXT("Group display-name-only refresh"),
		GroupCounter,
		{
			{FName(TEXT("DisplayName")), 1},
		});

	AddressCounter.Reset();
	GroupCounter.Reset();
	FirstAddress->InitializeSlot(
		nullptr,
		nullptr,
		RenamedGroup,
		0,
		0);
	GroupViewModel->InitializeGroup(RenamedGroup, OneSlot);
	VerifyFieldCounts(
		*this,
		TEXT("Repeated renamed address refresh"),
		AddressCounter,
		{});
	VerifyFieldCounts(
		*this,
		TEXT("Repeated renamed group refresh"),
		GroupCounter,
		{});

	URpgInventoryAddressSlotViewModel* SecondAddress =
		NewObject<URpgInventoryAddressSlotViewModel>(
			GetTransientPackage(),
			NAME_None,
			RF_Transient);
	if (!TestNotNull(TEXT("The second address VM exists"), SecondAddress))
	{
		return false;
	}
	SecondAddress->InitializeSlot(
		nullptr,
		nullptr,
		RenamedGroup,
		1,
		0);

	TArray<URpgInventoryAddressSlotViewModel*> TwoSlots;
	TwoSlots.Add(FirstAddress);
	TwoSlots.Add(SecondAddress);
	GroupCounter.Reset();
	GroupViewModel->InitializeGroup(RenamedGroup, TwoSlots);
	VerifyFieldCounts(
		*this,
		TEXT("Group slot-list-only refresh"),
		GroupCounter,
		{
			{FName(TEXT("Slots")), 1},
		});

	AddressCounter.Unbind();
	GroupCounter.Unbind();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryPanelViewModelFieldNotifyTest,
	"SurvivalRpg.Inventory.ViewModel.FieldNotify.PanelOnlyChangedFields",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryPanelViewModelFieldNotifyTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryViewModelFieldNotifyTests;

	FScopedInventoryWorld TestWorld;
	if (!TestTrue(
			TEXT("The panel FieldNotify world exists"),
			TestWorld.IsValid()))
	{
		return false;
	}

	URpgInventoryManagerComponent* Inventory =
		TestWorld.CreateInventory(
			TEXT("PanelFieldNotifyInventory"));
	URpgInventoryPanelViewModel* PanelViewModel =
		NewObject<URpgInventoryPanelViewModel>(
			Inventory,
			NAME_None,
			RF_Transient);
	if (!TestNotNull(TEXT("The panel inventory exists"), Inventory) ||
		!TestNotNull(TEXT("The panel VM exists"), PanelViewModel))
	{
		return false;
	}

	PanelViewModel->BindInventory(Inventory);
	FFieldNotifyCounter Counter(PanelViewModel);
	if (!TestTrue(
			TEXT("The panel counter subscribes to every FieldNotify field"),
			Counter.HasCompleteBinding()))
	{
		PanelViewModel->UnbindInventory();
		return false;
	}

	PanelViewModel->RefreshEntries();
	VerifyFieldCounts(
		*this,
		TEXT("Unchanged empty-panel refresh"),
		Counter,
		{});

	Counter.Reset();
	const FRpgInventoryContainerHandle StorageHandle =
		FRpgInventoryContainerHandle::MakeRoot(
			TEXT("Storage"));
	URpgInventoryItemInstance* Item =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1,
			MakePlacement(StorageHandle, 0, 0));
	if (!TestNotNull(TEXT("The panel fixture item exists"), Item))
	{
		Counter.Unbind();
		PanelViewModel->UnbindInventory();
		return false;
	}

	PanelViewModel->RefreshEntries();
	VerifyFieldCounts(
		*this,
		TEXT("First panel entry refresh"),
		Counter,
		{
			{FName(TEXT("Entries")), 1},
			{FName(TEXT("UsedEntries")), 1},
		});

	const TArray<URpgInventoryEntryViewModel*> FirstEntries =
		PanelViewModel->GetEntries();
	if (!TestEqual(
			TEXT("The panel exposes exactly one entry VM"),
			FirstEntries.Num(),
			1))
	{
		Counter.Unbind();
		PanelViewModel->UnbindInventory();
		return false;
	}
	URpgInventoryEntryViewModel* StableEntry = FirstEntries[0];

	Counter.Reset();
	PanelViewModel->RefreshEntries();
	VerifyFieldCounts(
		*this,
		TEXT("Repeated populated-panel refresh"),
		Counter,
		{});
	const TArray<URpgInventoryEntryViewModel*> RepeatedEntries =
		PanelViewModel->GetEntries();
	if (TestEqual(
			TEXT("The repeated panel refresh still exposes one entry"),
			RepeatedEntries.Num(),
			1))
	{
		TestEqual(
			TEXT("The repeated panel refresh retains its child VM"),
			RepeatedEntries[0],
			StableEntry);
	}

	URpgInventoryManagerComponent* SecondInventory =
		TestWorld.CreateInventory(
			TEXT("PanelFieldNotifySecondInventory"));
	URpgInventoryItemInstance* SecondItem =
		SecondInventory
			? SecondInventory->AddItemDefinitionToPlacement(
				URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
				1,
				MakePlacement(StorageHandle, 0, 0))
			: nullptr;
	if (!TestNotNull(
			TEXT("The second panel inventory exists"),
			SecondInventory) ||
		!TestNotNull(
			TEXT("The second panel fixture item exists"),
			SecondItem) ||
		!TestEqual(
			TEXT("The second panel inventory has exactly one entry"),
			SecondInventory->GetAllEntries().Num(),
			1))
	{
		Counter.Unbind();
		PanelViewModel->UnbindInventory();
		return false;
	}

	struct FAtomicPanelEntriesSnapshot
	{
		bool bObserved = false;
		FName FirstChangedField;
		int32 EntryCount = 0;
		TObjectPtr<URpgInventoryManagerComponent> EntryInventory = nullptr;
		TObjectPtr<URpgInventoryItemInstance> ItemInstance = nullptr;
	};
	FAtomicPanelEntriesSnapshot FirstRebindSnapshot;
	Counter.Reset();
	Counter.SetFirstNotificationCallback(
		[&FirstRebindSnapshot,
		 PanelViewModel](
			UObject*,
			UE::FieldNotification::FFieldId FieldId)
		{
			FirstRebindSnapshot.bObserved = true;
			FirstRebindSnapshot.FirstChangedField =
				FieldId.GetName();
			const TArray<URpgInventoryEntryViewModel*> SnapshotEntries =
				PanelViewModel->GetEntries();
			FirstRebindSnapshot.EntryCount =
				SnapshotEntries.Num();
			if (SnapshotEntries.Num() == 1 &&
				SnapshotEntries[0])
			{
				FirstRebindSnapshot.EntryInventory =
					SnapshotEntries[0]->GetInventoryManager();
				FirstRebindSnapshot.ItemInstance =
					SnapshotEntries[0]->GetItemInstance();
			}
		});

	PanelViewModel->BindInventory(SecondInventory);
	VerifyFieldCounts(
		*this,
		TEXT("Populated panel owner rebind"),
		Counter,
		{
			{FName(TEXT("Entries")), 1},
		});
	TestTrue(
		TEXT("The panel owner rebind published a FieldNotify callback"),
		FirstRebindSnapshot.bObserved);
	TestEqual(
		TEXT("The panel owner rebind first changed field is Entries"),
		FirstRebindSnapshot.FirstChangedField,
		FName(TEXT("Entries")));
	TestEqual(
		TEXT("The first panel rebind callback sees exactly one final entry"),
		FirstRebindSnapshot.EntryCount,
		1);
	TestEqual(
		TEXT("The first panel rebind callback sees the final inventory owner"),
		FirstRebindSnapshot.EntryInventory.Get(),
		SecondInventory);
	TestEqual(
		TEXT("The first panel rebind callback sees the final item instance"),
		FirstRebindSnapshot.ItemInstance.Get(),
		SecondItem);
	TestEqual(
		TEXT("The panel observes the second inventory after rebind"),
		PanelViewModel->GetObservedInventory(),
		SecondInventory);

	Counter.Reset();
	PanelViewModel->BindInventory(SecondInventory);
	VerifyFieldCounts(
		*this,
		TEXT("Repeated populated panel owner rebind"),
		Counter,
		{});

	Counter.Unbind();
	PanelViewModel->UnbindInventory();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgPlayerInventoryAggregateFieldNotifyTest,
	"SurvivalRpg.Inventory.ViewModel.FieldNotify.PlayerAggregateOnlyChangedFields",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgPlayerInventoryAggregateFieldNotifyTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryViewModelFieldNotifyTests;

	FScopedInventoryWorld TestWorld;
	if (!TestTrue(
			TEXT("The aggregate FieldNotify world exists"),
			TestWorld.IsValid()))
	{
		return false;
	}

	UWorld* World = TestWorld.GetWorld();
	FActorSpawnParameters ControllerSpawnParameters;
	ControllerSpawnParameters.Name = MakeUniqueObjectName(
		World,
		ARpgInventoryAutomationTestPlayerController::StaticClass(),
		TEXT("FieldNotifyController"));
	ControllerSpawnParameters.ObjectFlags = RF_Transient;
	ARpgInventoryAutomationTestPlayerController* Controller =
		World->SpawnActor<ARpgInventoryAutomationTestPlayerController>(
			ControllerSpawnParameters);

	FActorSpawnParameters PlayerStateSpawnParameters;
	PlayerStateSpawnParameters.Name = MakeUniqueObjectName(
		World,
		ARpgInventoryAutomationTestPlayerState::StaticClass(),
		TEXT("FieldNotifyPlayerState"));
	PlayerStateSpawnParameters.ObjectFlags = RF_Transient;
	ARpgInventoryAutomationTestPlayerState* PlayerState =
		World->SpawnActor<ARpgInventoryAutomationTestPlayerState>(
			PlayerStateSpawnParameters);
	if (!TestNotNull(TEXT("The aggregate controller exists"), Controller) ||
		!TestNotNull(TEXT("The aggregate player state exists"), PlayerState))
	{
		return false;
	}

	Controller->SetPlayerState(PlayerState);
	PlayerState->SetOwner(Controller);

	URpgPlayerInventoryViewModel* ViewModel =
		NewObject<URpgPlayerInventoryViewModel>(
			Controller,
			NAME_None,
			RF_Transient);
	if (!TestNotNull(TEXT("The aggregate VM exists"), ViewModel))
	{
		return false;
	}
	ViewModel->BindPlayerController(Controller);

	if (!TestTrue(
			TEXT("The bound aggregate exposes carry groups"),
			!ViewModel->GetCarryGroups().IsEmpty()) ||
		!TestTrue(
			TEXT("The bound aggregate exposes inventory groups"),
			!ViewModel->GetInventoryGroups().IsEmpty()))
	{
		ViewModel->UnbindPlayerInventory();
		return false;
	}

	FFieldNotifyCounter Counter(ViewModel);
	if (!TestTrue(
			TEXT("The aggregate counter subscribes to every FieldNotify field"),
			Counter.HasCompleteBinding()))
	{
		ViewModel->UnbindPlayerInventory();
		return false;
	}

	ViewModel->BindPlayerController(Controller);
	VerifyFieldCounts(
		*this,
		TEXT("Unchanged player aggregate presenter rebind"),
		Counter,
		{});

	Counter.Reset();
	ViewModel->UnbindPlayerInventory();
	VerifyFieldCounts(
		*this,
		TEXT("First player aggregate unbind"),
		Counter,
		{
			{FName(TEXT("CarryGroups")), 1},
			{FName(TEXT("InventoryGroups")), 1},
		});

	Counter.Reset();
	ViewModel->UnbindPlayerInventory();
	VerifyFieldCounts(
		*this,
		TEXT("Repeated player aggregate unbind"),
		Counter,
		{});

	Counter.Unbind();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryNestedContainerFieldNotifyTest,
	"SurvivalRpg.Inventory.ViewModel.FieldNotify.NestedContainerOnlyChangedFields",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryNestedContainerFieldNotifyTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryViewModelFieldNotifyTests;

	FScopedInventoryWorld TestWorld;
	if (!TestTrue(
			TEXT("The nested-container FieldNotify world exists"),
			TestWorld.IsValid()))
	{
		return false;
	}

	URpgInventoryManagerComponent* Inventory =
		TestWorld.CreateInventory(
			TEXT("NestedFieldNotifyInventory"));
	if (!TestNotNull(TEXT("The nested-container inventory exists"), Inventory))
	{
		return false;
	}

	const FRpgInventoryContainerHandle StorageHandle =
		FRpgInventoryContainerHandle::MakeRoot(
			TEXT("Storage"));
	URpgInventoryItemInstance* Bag =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestBagItemDefinition::StaticClass(),
			1,
			MakePlacement(StorageHandle, 0, 0));
	if (!TestNotNull(TEXT("The nested-container bag exists"), Bag))
	{
		return false;
	}

	const FRpgInventoryContainerHandle ContentsHandle =
		FRpgInventoryContainerHandle::MakeItemOwned(
			Bag->GetItemId(),
			TEXT("Main"),
			StorageHandle.GetDirectChildDepth());
	URpgInventoryItemInstance* NestedItem =
		Inventory->AddItemDefinitionToPlacement(
			URpgInventoryAutomationTestUnitItemDefinition::StaticClass(),
			1,
			MakePlacement(ContentsHandle, 0, 0));
	if (!TestNotNull(TEXT("The nested fixture item exists"), NestedItem))
	{
		return false;
	}

	URpgInventoryNestedContainerViewModel* ViewModel =
		NewObject<URpgInventoryNestedContainerViewModel>(
			Inventory,
			NAME_None,
			RF_Transient);
	if (!TestNotNull(TEXT("The nested-container VM exists"), ViewModel) ||
		!TestTrue(
			TEXT("The nested-container VM opens its item-owned grid"),
			ViewModel->OpenContainerHandle(
				Inventory,
				ContentsHandle)))
	{
		return false;
	}

	FFieldNotifyCounter Counter(ViewModel);
	if (!TestTrue(
			TEXT("The nested counter subscribes to every FieldNotify field"),
			Counter.HasCompleteBinding()))
	{
		ViewModel->CloseContainer();
		return false;
	}

	URpgInventoryAutomationTestDynamicDelegateCounter*
		PresentationChangedCounter =
			NewObject<URpgInventoryAutomationTestDynamicDelegateCounter>(
				ViewModel,
				NAME_None,
				RF_Transient);
	if (!TestNotNull(
			TEXT("The nested presentation delegate counter exists"),
			PresentationChangedCounter))
	{
		Counter.Unbind();
		ViewModel->CloseContainer();
		return false;
	}
	ViewModel->OnPresentationChanged.AddUniqueDynamic(
		PresentationChangedCounter,
		&URpgInventoryAutomationTestDynamicDelegateCounter::RecordInvocation);
	PresentationChangedCounter->ResetInvocationCount();
	Counter.Reset();

	// A successful same-handle open refreshes the reused child panel and
	// intentionally retains one imperative presentation-complete signal even
	// when every declarative nested FieldNotify value remains unchanged.
	TestTrue(
		TEXT("The same nested container can be revalidated"),
		ViewModel->OpenContainerHandle(
			Inventory,
			ContentsHandle));
	VerifyFieldCounts(
		*this,
		TEXT("Repeated nested-container open"),
		Counter,
		{});
	TestEqual(
		TEXT("Repeated nested-container open publishes one presentation signal"),
		PresentationChangedCounter->GetInvocationCount(),
		1);
	ViewModel->OnPresentationChanged.RemoveDynamic(
		PresentationChangedCounter,
		&URpgInventoryAutomationTestDynamicDelegateCounter::RecordInvocation);

	const FText LowerQuery =
		FText::FromString(TEXT("does-not-match"));
	Counter.Reset();
	ViewModel->SetFilterQuery(LowerQuery);
	VerifyFieldCounts(
		*this,
		TEXT("Filter query and derived dimming change"),
		Counter,
		{
			{FName(TEXT("FilterQuery")), 1},
			{FName(TEXT("EntryFilterPresentation")), 1},
		});

	const FText UpperQuery =
		FText::FromString(TEXT("DOES-NOT-MATCH"));
	Counter.Reset();
	ViewModel->SetFilterQuery(UpperQuery);
	VerifyFieldCounts(
		*this,
		TEXT("Case-only filter query change"),
		Counter,
		{
			{FName(TEXT("FilterQuery")), 1},
		});

	Counter.Reset();
	ViewModel->SetFilterQuery(UpperQuery);
	VerifyFieldCounts(
		*this,
		TEXT("Identical filter query"),
		Counter,
		{});

	Counter.Reset();
	ViewModel->CloseContainer();
	VerifyFieldCounts(
		*this,
		TEXT("First nested-container close"),
		Counter,
		{
			{FName(TEXT("bIsOpen")), 1},
			{FName(TEXT("OpenOwnerItemId")), 1},
			{FName(TEXT("ActiveContainerHandle")), 1},
			{FName(TEXT("Title")), 1},
			{FName(TEXT("Breadcrumbs")), 1},
			{FName(TEXT("FilterQuery")), 1},
			{FName(TEXT("EntryFilterPresentation")), 1},
		});

	Counter.Reset();
	ViewModel->CloseContainer();
	VerifyFieldCounts(
		*this,
		TEXT("Repeated nested-container close"),
		Counter,
		{});

	Counter.Unbind();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgBaseStorageResourceEntryFieldNotifyTest,
	"SurvivalRpg.BaseStorage.ViewModel.FieldNotify.ResourceEntryOnlyChangedFields",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgBaseStorageResourceEntryFieldNotifyTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryViewModelFieldNotifyTests;

	FRpgBaseResourceEntryView InitialEntry;
	InitialEntry.ItemDefinition =
		URpgInventoryAutomationTestMaterialDefinition::StaticClass();
	InitialEntry.Count = 2;
	InitialEntry.Capacity = 10;
	InitialEntry.SortIndex = 3;

	URpgBaseResourceEntryViewModel* ViewModel =
		NewObject<URpgBaseResourceEntryViewModel>(
			GetTransientPackage(),
			NAME_None,
			RF_Transient);
	if (!TestNotNull(TEXT("The base-resource entry VM exists"), ViewModel))
	{
		return false;
	}
	ViewModel->InitializeFromResourceEntry(InitialEntry);

	FFieldNotifyCounter Counter(ViewModel);
	if (!TestTrue(
			TEXT("The base-resource counter subscribes to every FieldNotify field"),
			Counter.HasCompleteBinding()))
	{
		return false;
	}

	ViewModel->InitializeFromResourceEntry(InitialEntry);
	VerifyFieldCounts(
		*this,
		TEXT("Identical base-resource refresh"),
		Counter,
		{});

	FRpgBaseResourceEntryView SortedEntry = InitialEntry;
	SortedEntry.SortIndex = 4;
	Counter.Reset();
	ViewModel->InitializeFromResourceEntry(SortedEntry);
	VerifyFieldCounts(
		*this,
		TEXT("Base-resource sort-index-only refresh"),
		Counter,
		{
			{FName(TEXT("SortIndex")), 1},
		});

	Counter.Reset();
	ViewModel->InitializeFromResourceEntry(SortedEntry);
	VerifyFieldCounts(
		*this,
		TEXT("Repeated sorted base-resource refresh"),
		Counter,
		{});

	const FTextProperty* DisplayNameProperty =
		FindFProperty<FTextProperty>(
			ViewModel->GetClass(),
			TEXT("DisplayName"));
	const FSoftObjectProperty* IconProperty =
		FindFProperty<FSoftObjectProperty>(
			ViewModel->GetClass(),
			TEXT("Icon"));
	const FIntProperty* CountProperty =
		FindFProperty<FIntProperty>(
			ViewModel->GetClass(),
			TEXT("Count"));
	const FIntProperty* CapacityProperty =
		FindFProperty<FIntProperty>(
			ViewModel->GetClass(),
			TEXT("Capacity"));
	const FIntProperty* FreeCapacityProperty =
		FindFProperty<FIntProperty>(
			ViewModel->GetClass(),
			TEXT("FreeCapacity"));
	const FFloatProperty* FillRatioProperty =
		FindFProperty<FFloatProperty>(
			ViewModel->GetClass(),
			TEXT("FillRatio"));
	const FIntProperty* SortIndexProperty =
		FindFProperty<FIntProperty>(
			ViewModel->GetClass(),
			TEXT("SortIndex"));
	const FBoolProperty* IsEmptyProperty =
		FindFProperty<FBoolProperty>(
			ViewModel->GetClass(),
			TEXT("bIsEmpty"));
	const FBoolProperty* IsFullProperty =
		FindFProperty<FBoolProperty>(
			ViewModel->GetClass(),
			TEXT("bIsFull"));
	if (!TestNotNull(
			TEXT("The DisplayName snapshot property exists"),
			DisplayNameProperty) ||
		!TestNotNull(
			TEXT("The Icon snapshot property exists"),
			IconProperty) ||
		!TestNotNull(
			TEXT("The Count snapshot property exists"),
			CountProperty) ||
		!TestNotNull(
			TEXT("The Capacity snapshot property exists"),
			CapacityProperty) ||
		!TestNotNull(
			TEXT("The FreeCapacity snapshot property exists"),
			FreeCapacityProperty) ||
		!TestNotNull(
			TEXT("The FillRatio snapshot property exists"),
			FillRatioProperty) ||
		!TestNotNull(
			TEXT("The SortIndex snapshot property exists"),
			SortIndexProperty) ||
		!TestNotNull(
			TEXT("The bIsEmpty snapshot property exists"),
			IsEmptyProperty) ||
		!TestNotNull(
			TEXT("The bIsFull snapshot property exists"),
			IsFullProperty))
	{
		return false;
	}

	struct FAtomicBaseResourceSnapshot
	{
		bool bObserved = false;
		FName FirstChangedField;
		TSubclassOf<URpgInventoryItemDefinition> ItemDefinition;
		FText DisplayName;
		bool bIconIsNull = false;
		int32 Count = 0;
		int32 Capacity = 0;
		int32 FreeCapacity = 0;
		float FillRatio = 0.0f;
		int32 SortIndex = 0;
		bool bIsEmpty = true;
		bool bIsFull = false;
	};
	FAtomicBaseResourceSnapshot FirstCallbackSnapshot;
	Counter.SetFirstNotificationCallback(
		[&FirstCallbackSnapshot,
		 ViewModel,
		 DisplayNameProperty,
		 IconProperty,
		 CountProperty,
		 CapacityProperty,
		 FreeCapacityProperty,
		 FillRatioProperty,
		 SortIndexProperty,
		 IsEmptyProperty,
		 IsFullProperty](
			UObject* ChangedObject,
			UE::FieldNotification::FFieldId FieldId)
		{
			FirstCallbackSnapshot.bObserved = true;
			FirstCallbackSnapshot.FirstChangedField =
				FieldId.GetName();
			FirstCallbackSnapshot.ItemDefinition =
				ViewModel->GetItemDefinition();
			FirstCallbackSnapshot.DisplayName =
				DisplayNameProperty->GetPropertyValue_InContainer(
					ChangedObject);
			FirstCallbackSnapshot.bIconIsNull =
				IconProperty->GetPropertyValue_InContainer(
					ChangedObject).IsNull();
			FirstCallbackSnapshot.Count =
				CountProperty->GetPropertyValue_InContainer(
					ChangedObject);
			FirstCallbackSnapshot.Capacity =
				CapacityProperty->GetPropertyValue_InContainer(
					ChangedObject);
			FirstCallbackSnapshot.FreeCapacity =
				FreeCapacityProperty->GetPropertyValue_InContainer(
					ChangedObject);
			FirstCallbackSnapshot.FillRatio =
				FillRatioProperty->GetPropertyValue_InContainer(
					ChangedObject);
			FirstCallbackSnapshot.SortIndex =
				SortIndexProperty->GetPropertyValue_InContainer(
					ChangedObject);
			FirstCallbackSnapshot.bIsEmpty =
				IsEmptyProperty->GetPropertyValue_InContainer(
					ChangedObject);
			FirstCallbackSnapshot.bIsFull =
				IsFullProperty->GetPropertyValue_InContainer(
					ChangedObject);
		});

	FRpgBaseResourceEntryView FullEntry = SortedEntry;
	FullEntry.Count = FullEntry.Capacity;
	Counter.Reset();
	ViewModel->InitializeFromResourceEntry(FullEntry);
	VerifyFieldCounts(
		*this,
		TEXT("Base-resource count reaches capacity"),
		Counter,
		{
			{FName(TEXT("Count")), 1},
			{FName(TEXT("FreeCapacity")), 1},
			{FName(TEXT("FillRatio")), 1},
			{FName(TEXT("bIsFull")), 1},
		});
	TestTrue(
		TEXT("The first changed FieldNotify callback captured a snapshot"),
		FirstCallbackSnapshot.bObserved);
	TestTrue(
		TEXT("The first changed FieldNotify callback identifies a valid field"),
		!FirstCallbackSnapshot.FirstChangedField.IsNone());
	TestEqual(
		TEXT("The first callback sees the final item definition"),
		FirstCallbackSnapshot.ItemDefinition.Get(),
		URpgInventoryAutomationTestMaterialDefinition::StaticClass());
	TestEqual(
		TEXT("The first callback sees the final display name"),
		FirstCallbackSnapshot.DisplayName.ToString(),
		FString(TEXT("Automation Material")));
	TestTrue(
		TEXT("The first callback sees the final empty icon"),
		FirstCallbackSnapshot.bIconIsNull);
	TestEqual(
		TEXT("The first callback sees the final count"),
		FirstCallbackSnapshot.Count,
		10);
	TestEqual(
		TEXT("The first callback sees the final capacity"),
		FirstCallbackSnapshot.Capacity,
		10);
	TestEqual(
		TEXT("The first callback sees the final free capacity"),
		FirstCallbackSnapshot.FreeCapacity,
		0);
	TestTrue(
		TEXT("The first callback sees the final fill ratio"),
		FMath::IsNearlyEqual(
			FirstCallbackSnapshot.FillRatio,
			1.0f));
	TestEqual(
		TEXT("The first callback sees the final sort index"),
		FirstCallbackSnapshot.SortIndex,
		4);
	TestFalse(
		TEXT("The first callback sees the final non-empty state"),
		FirstCallbackSnapshot.bIsEmpty);
	TestTrue(
		TEXT("The first callback sees the final full state"),
		FirstCallbackSnapshot.bIsFull);

	Counter.Reset();
	ViewModel->InitializeFromResourceEntry(FullEntry);
	VerifyFieldCounts(
		*this,
		TEXT("Repeated full base-resource refresh"),
		Counter,
		{});

	Counter.Unbind();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCraftingOutputFieldNotifyTest,
	"SurvivalRpg.Crafting.ViewModel.FieldNotify.OutputOnlyChangedFields",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgCraftingOutputFieldNotifyTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryViewModelFieldNotifyTests;

	URpgCraftingOutputViewModel* ViewModel =
		NewObject<URpgCraftingOutputViewModel>(
			GetTransientPackage(),
			NAME_None,
			RF_Transient);
	if (!TestNotNull(TEXT("The crafting-output VM exists"), ViewModel))
	{
		return false;
	}

	const TSubclassOf<URpgInventoryItemDefinition> ItemDefinition =
		URpgInventoryAutomationTestUnitItemDefinition::StaticClass();
	ViewModel->InitializeOutput(ItemDefinition, 1);

	FFieldNotifyCounter Counter(ViewModel);
	if (!TestTrue(
			TEXT("The crafting-output counter subscribes to every FieldNotify field"),
			Counter.HasCompleteBinding()))
	{
		return false;
	}

	ViewModel->InitializeOutput(ItemDefinition, 1);
	VerifyFieldCounts(
		*this,
		TEXT("Identical crafting-output refresh"),
		Counter,
		{});

	Counter.Reset();
	ViewModel->InitializeOutput(ItemDefinition, 2);
	VerifyFieldCounts(
		*this,
		TEXT("Crafting-output count-only refresh"),
		Counter,
		{
			{FName(TEXT("OutputCount")), 1},
		});

	Counter.Reset();
	ViewModel->InitializeOutput(ItemDefinition, 2);
	VerifyFieldCounts(
		*this,
		TEXT("Repeated crafting-output refresh"),
		Counter,
		{});

	Counter.Unbind();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCraftingStationRebindFieldNotifyTest,
	"SurvivalRpg.Crafting.ViewModel.FieldNotify.StationRebindPublishesFinalPointers",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgCraftingStationRebindFieldNotifyTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryViewModelFieldNotifyTests;

	FScopedInventoryWorld TestWorld;
	if (!TestTrue(
			TEXT("The crafting-station rebind world exists"),
			TestWorld.IsValid()))
	{
		return false;
	}

	UWorld* World = TestWorld.GetWorld();
	FActorSpawnParameters StationOwnerSpawnParameters;
	StationOwnerSpawnParameters.Name = MakeUniqueObjectName(
		World,
		AActor::StaticClass(),
		TEXT("FieldNotifyCraftingStationOwner"));
	StationOwnerSpawnParameters.ObjectFlags = RF_Transient;
	AActor* StationOwner =
		World->SpawnActor<AActor>(StationOwnerSpawnParameters);

	FActorSpawnParameters RequesterASpawnParameters;
	RequesterASpawnParameters.Name = MakeUniqueObjectName(
		World,
		AActor::StaticClass(),
		TEXT("FieldNotifyCraftingRequesterA"));
	RequesterASpawnParameters.ObjectFlags = RF_Transient;
	AActor* RequesterA =
		World->SpawnActor<AActor>(RequesterASpawnParameters);

	FActorSpawnParameters RequesterBSpawnParameters;
	RequesterBSpawnParameters.Name = MakeUniqueObjectName(
		World,
		AActor::StaticClass(),
		TEXT("FieldNotifyCraftingRequesterB"));
	RequesterBSpawnParameters.ObjectFlags = RF_Transient;
	AActor* RequesterB =
		World->SpawnActor<AActor>(RequesterBSpawnParameters);
	if (!TestNotNull(
			TEXT("The crafting-station owner exists"),
			StationOwner) ||
		!TestNotNull(
			TEXT("Crafting requester A exists"),
			RequesterA) ||
		!TestNotNull(
			TEXT("Crafting requester B exists"),
			RequesterB))
	{
		return false;
	}

	URpgCraftingStationComponent* Station =
		NewObject<URpgCraftingStationComponent>(
			StationOwner,
			MakeUniqueObjectName(
				StationOwner,
				URpgCraftingStationComponent::StaticClass(),
				TEXT("FieldNotifyCraftingStation")),
			RF_Transient);
	if (!TestNotNull(
			TEXT("The transient crafting-station component exists"),
			Station))
	{
		return false;
	}
	StationOwner->AddInstanceComponent(Station);
	Station->RegisterComponent();

	URpgCraftingStationViewModel* ViewModel =
		NewObject<URpgCraftingStationViewModel>(
			Station,
			NAME_None,
			RF_Transient);
	if (!TestNotNull(
			TEXT("The crafting-station VM exists"),
			ViewModel))
	{
		return false;
	}
	ViewModel->BindCraftingStation(Station, RequesterA);

	const FObjectPropertyBase* ObservedStationProperty =
		FindFProperty<FObjectPropertyBase>(
			ViewModel->GetClass(),
			TEXT("ObservedStation"));
	const FObjectPropertyBase* RequestingActorProperty =
		FindFProperty<FObjectPropertyBase>(
			ViewModel->GetClass(),
			TEXT("RequestingActor"));
	if (!TestNotNull(
			TEXT("The ObservedStation snapshot property exists"),
			ObservedStationProperty) ||
		!TestNotNull(
			TEXT("The RequestingActor snapshot property exists"),
			RequestingActorProperty))
	{
		ViewModel->UnbindCraftingStation();
		return false;
	}

	TestEqual(
		TEXT("The baseline observes the transient station"),
		ObservedStationProperty->GetObjectPropertyValue_InContainer(
			ViewModel),
		static_cast<UObject*>(Station));
	TestEqual(
		TEXT("The baseline observes requester A"),
		RequestingActorProperty->GetObjectPropertyValue_InContainer(
			ViewModel),
		static_cast<UObject*>(RequesterA));

	FFieldNotifyCounter Counter(ViewModel);
	if (!TestTrue(
			TEXT("The crafting-station counter subscribes to every FieldNotify field"),
			Counter.HasCompleteBinding()))
	{
		ViewModel->UnbindCraftingStation();
		return false;
	}

	struct FAtomicCraftingStationSnapshot
	{
		bool bObserved = false;
		FName FirstChangedField;
		TObjectPtr<UObject> ObservedStation = nullptr;
		TObjectPtr<UObject> RequestingActor = nullptr;
	};
	FAtomicCraftingStationSnapshot FirstCallbackSnapshot;
	Counter.SetFirstNotificationCallback(
		[&FirstCallbackSnapshot,
		 ObservedStationProperty,
		 RequestingActorProperty](
			UObject* ChangedObject,
			UE::FieldNotification::FFieldId FieldId)
		{
			FirstCallbackSnapshot.bObserved = true;
			FirstCallbackSnapshot.FirstChangedField =
				FieldId.GetName();
			FirstCallbackSnapshot.ObservedStation =
				ObservedStationProperty->
					GetObjectPropertyValue_InContainer(
						ChangedObject);
			FirstCallbackSnapshot.RequestingActor =
				RequestingActorProperty->
					GetObjectPropertyValue_InContainer(
						ChangedObject);
		});

	ViewModel->BindCraftingStation(Station, RequesterB);
	VerifyFieldCounts(
		*this,
		TEXT("Same-station requester rebind"),
		Counter,
		{
			{FName(TEXT("RequestingActor")), 1},
		});
	TestTrue(
		TEXT("The requester rebind published a FieldNotify callback"),
		FirstCallbackSnapshot.bObserved);
	TestEqual(
		TEXT("The requester rebind first changed field is RequestingActor"),
		FirstCallbackSnapshot.FirstChangedField,
		FName(TEXT("RequestingActor")));
	TestEqual(
		TEXT("The first callback already sees the final station"),
		FirstCallbackSnapshot.ObservedStation.Get(),
		static_cast<UObject*>(Station));
	TestEqual(
		TEXT("The first callback already sees requester B"),
		FirstCallbackSnapshot.RequestingActor.Get(),
		static_cast<UObject*>(RequesterB));

	Counter.Reset();
	ViewModel->BindCraftingStation(Station, RequesterB);
	VerifyFieldCounts(
		*this,
		TEXT("Repeated same-station requester rebind"),
		Counter,
		{});

	Counter.Unbind();
	ViewModel->UnbindCraftingStation();
	return true;
}

#endif
