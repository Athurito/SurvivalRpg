#include "RpgInventoryAutomationTestTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "IPickupable.h"
#include "RpgInventoryManagerComponent.h"
#include "RpgPlayerInventoryLayoutComponent.h"
#include "SurvivalRpg/ActionBar/RpgActionBarComponent.h"
#include "SurvivalRpg/Base/RpgBaseStorageComponent.h"
#include "SurvivalRpg/Crafting/RpgCraftingStationComponent.h"
#include "SurvivalRpg/Equipment/RpgEquipmentLoadoutComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Mvvm/Base/RpgBaseStorageViewModels.h"
#include "SurvivalRpg/Mvvm/Crafting/RpgCraftingViewModels.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgActionBarViewModels.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgInventoryViewModels.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgPlayerInventoryViewModels.h"

#include "CoreGlobals.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Misc/AutomationTest.h"
#include "TimerManager.h"

namespace RpgInventoryViewModelInvalidationTests
{
	constexpr float TimerFrameDeltaSeconds = 1.0f / 60.0f;

	FGameplayTag GetInventoryChangedChannel()
	{
		return FGameplayTag::RequestGameplayTag(
			TEXT("Rpg.Inventory.Message.StackChanged"));
	}

	FGameplayTag GetBaseStorageChangedChannel()
	{
		return FGameplayTag::RequestGameplayTag(
			TEXT("Rpg.BaseStorage.Message.Changed"));
	}

	class FScopedInvalidationWorld
	{
	public:
		FScopedInvalidationWorld()
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

		~FScopedInvalidationWorld()
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

			// The explicit timer ticks below must not advance the editor's
			// engine-global frame state beyond this isolated fixture.
			GFrameCounter = CachedFrameCounter;
		}

		bool IsValid() const
		{
			return GameInstance != nullptr && World != nullptr;
		}

		UWorld* GetWorld() const
		{
			return World;
		}

		template <typename ActorType = AActor>
		ActorType* SpawnActor(const TCHAR* DebugName) const
		{
			if (!World)
			{
				return nullptr;
			}

			FActorSpawnParameters SpawnParameters;
			SpawnParameters.Name = MakeUniqueObjectName(
				World,
				ActorType::StaticClass(),
				FName(DebugName));
			SpawnParameters.ObjectFlags = RF_Transient;
			return World->SpawnActor<ActorType>(SpawnParameters);
		}

		template <typename ComponentType>
		ComponentType* CreateComponent(
			AActor* Owner,
			const TCHAR* DebugName) const
		{
			if (!Owner)
			{
				return nullptr;
			}

			ComponentType* Component = NewObject<ComponentType>(
				Owner,
				MakeUniqueObjectName(
					Owner,
					ComponentType::StaticClass(),
					FName(DebugName)),
				RF_Transient);
			if (!Component)
			{
				return nullptr;
			}

			Owner->AddInstanceComponent(Component);
			Component->RegisterComponent();
			return Component;
		}

		URpgInventoryManagerComponent* CreateInventory(
			const TCHAR* DebugName) const
		{
			AActor* Owner = SpawnActor(DebugName);
			return CreateComponent<URpgInventoryManagerComponent>(
				Owner,
				TEXT("Inventory"));
		}

		/**
		 * Marks the standalone timer manager as already ticked in this frame.
		 *
		 * Scheduling after this call guarantees that SetTimerForNextTick is
		 * eligible on the following explicit frame even when
		 * TimerManager.GuaranteeEngineTickDelay is enabled.
		 */
		void PrimeTimerManager() const
		{
			if (!World)
			{
				return;
			}

			++GFrameCounter;
			World->GetTimerManager().Tick(0.0f);
		}

		/** Advances only the timer manager, avoiding unrelated actor ticks. */
		void AdvanceTimerFrame() const
		{
			if (!World)
			{
				return;
			}

			++GFrameCounter;
			World->GetTimerManager().Tick(
				TimerFrameDeltaSeconds);
		}

	private:
		const uint64 CachedFrameCounter = GFrameCounter;
		TObjectPtr<UGameInstance> GameInstance;
		TObjectPtr<UWorld> World;
	};

	struct FPlayerFixture
	{
		TObjectPtr<ARpgInventoryAutomationTestPlayerController> Controller;
		TObjectPtr<ARpgInventoryAutomationTestPlayerState> PlayerState;
		TObjectPtr<URpgInventoryManagerComponent> Inventory;
		TObjectPtr<URpgPlayerInventoryLayoutComponent> InventoryLayout;
		TObjectPtr<URpgEquipmentLoadoutComponent> EquipmentLoadout;
		TObjectPtr<URpgActionBarComponent> ActionBar;
	};

	bool CreatePlayerFixture(
		const FScopedInvalidationWorld& TestWorld,
		const TCHAR* DebugPrefix,
		FPlayerFixture& OutFixture)
	{
		UWorld* World = TestWorld.GetWorld();
		if (!World)
		{
			return false;
		}

		const FString ControllerName = FString::Printf(
			TEXT("%sController"),
			DebugPrefix);
		const FString PlayerStateName = FString::Printf(
			TEXT("%sPlayerState"),
			DebugPrefix);
		OutFixture.Controller =
			TestWorld.SpawnActor<
				ARpgInventoryAutomationTestPlayerController>(
				*ControllerName);
		OutFixture.PlayerState =
			TestWorld.SpawnActor<
				ARpgInventoryAutomationTestPlayerState>(
				*PlayerStateName);
		if (!OutFixture.Controller || !OutFixture.PlayerState)
		{
			return false;
		}

		OutFixture.Controller->SetPlayerState(
			OutFixture.PlayerState);
		OutFixture.PlayerState->SetOwner(
			OutFixture.Controller);
		OutFixture.Inventory =
			OutFixture.PlayerState->GetInventoryManagerComponent();
		OutFixture.InventoryLayout =
			OutFixture.Controller->
				GetPlayerInventoryLayoutComponent();
		OutFixture.EquipmentLoadout =
			OutFixture.Controller->
				GetEquipmentLoadoutComponent();
		OutFixture.ActionBar =
			OutFixture.Controller->GetActionBarComponent();
		return OutFixture.Inventory &&
			OutFixture.InventoryLayout &&
			OutFixture.EquipmentLoadout &&
			OutFixture.ActionBar;
	}

	template <typename DelegateType>
	class FScopedNoArgumentDelegateCounter
	{
	public:
		FScopedNoArgumentDelegateCounter(
			DelegateType& InDelegate,
			UObject* CounterOuter)
			: Delegate(&InDelegate)
		{
			Counter =
				NewObject<
					URpgInventoryAutomationTestDynamicDelegateCounter>(
						CounterOuter,
						NAME_None,
						RF_Transient);
			if (Counter)
			{
				Delegate->AddUniqueDynamic(
					Counter,
					&URpgInventoryAutomationTestDynamicDelegateCounter::RecordInvocation);
				bBound = true;
			}
		}

		~FScopedNoArgumentDelegateCounter()
		{
			Unbind();
		}

		FScopedNoArgumentDelegateCounter(
			const FScopedNoArgumentDelegateCounter&) = delete;
		FScopedNoArgumentDelegateCounter& operator=(
			const FScopedNoArgumentDelegateCounter&) = delete;

		bool IsBound() const
		{
			return bBound && Counter != nullptr;
		}

		int32 GetCount() const
		{
			return Counter ? Counter->GetInvocationCount() : 0;
		}

		void ResetCount()
		{
			if (Counter)
			{
				Counter->ResetInvocationCount();
			}
		}

		void Unbind()
		{
			if (bBound && Delegate && Counter)
			{
				Delegate->RemoveDynamic(
					Counter,
					&URpgInventoryAutomationTestDynamicDelegateCounter::RecordInvocation);
			}

			bBound = false;
			Delegate = nullptr;
			Counter = nullptr;
		}

	private:
		DelegateType* Delegate = nullptr;
		URpgInventoryAutomationTestDynamicDelegateCounter* Counter =
			nullptr;
		bool bBound = false;
	};

	class FScopedGameplayMessageListener
	{
	public:
		FScopedGameplayMessageListener() = default;

		~FScopedGameplayMessageListener()
		{
			Unregister();
		}

		FScopedGameplayMessageListener(
			const FScopedGameplayMessageListener&) = delete;
		FScopedGameplayMessageListener& operator=(
			const FScopedGameplayMessageListener&) = delete;

		void Set(FGameplayMessageListenerHandle InHandle)
		{
			Unregister();
			Handle = MoveTemp(InHandle);
		}

		void Unregister()
		{
			if (Handle.IsValid())
			{
				Handle.Unregister();
			}
		}

	private:
		FGameplayMessageListenerHandle Handle;
	};

	void BroadcastInventoryChanged(
		UWorld* World,
		URpgInventoryManagerComponent* Inventory)
	{
		if (!World || !Inventory)
		{
			return;
		}

		FRpgInventoryChangeMessage Message;
		Message.InventoryOwner = Inventory;
		UGameplayMessageSubsystem::Get(World).BroadcastMessage(
			GetInventoryChangedChannel(),
			Message);
	}

	void BroadcastLayoutChanged(
		UWorld* World,
		AActor* Owner,
		URpgPlayerInventoryLayoutComponent* InventoryLayout)
	{
		if (!World || !InventoryLayout)
		{
			return;
		}

		FRpgPlayerInventoryLayoutChangedMessage Message;
		Message.Owner = Owner;
		Message.LayoutComponent = InventoryLayout;
		UGameplayMessageSubsystem::Get(World).BroadcastMessage(
			RpgGameplayTags::Rpg_InventoryLayout_Message_Changed,
			Message);
	}

	void BroadcastEquipmentChanged(
		UWorld* World,
		AActor* Owner)
	{
		if (!World || !Owner)
		{
			return;
		}

		FRpgEquipmentLoadoutSlotsChangedMessage Message;
		Message.Owner = Owner;
		UGameplayMessageSubsystem::Get(World).BroadcastMessage(
			RpgGameplayTags::
				Rpg_EquipmentLoadout_Message_SlotsChanged,
			Message);
	}

	void BroadcastActionBarChanged(
		UWorld* World,
		APlayerController* Owner,
		URpgActionBarComponent* ActionBar)
	{
		if (!World || !ActionBar)
		{
			return;
		}

		FRpgActionBarSlotsChangedMessage Message;
		Message.Owner = Owner;
		Message.ActionBarComponent = ActionBar;
		UGameplayMessageSubsystem::Get(World).BroadcastMessage(
			RpgGameplayTags::Rpg_ActionBar_Message_SlotsChanged,
			Message);
	}

	void BroadcastCraftingStationChanged(
		UWorld* World,
		URpgCraftingStationComponent* Station)
	{
		if (!World || !Station)
		{
			return;
		}

		FRpgCraftingStationChangeMessage Message;
		Message.Station = Station;
		UGameplayMessageSubsystem::Get(World).BroadcastMessage(
			FGameplayTag::RequestGameplayTag(
				TEXT("Rpg.Crafting.Message.StationChanged")),
			Message);
	}

	FInventoryPickup MakeTwoRowUnitPickup()
	{
		FInventoryPickup Pickup;
		for (int32 RowIndex = 0; RowIndex < 2; ++RowIndex)
		{
			FPickupTemplate& Row =
				Pickup.Templates.AddDefaulted_GetRef();
			Row.ItemDef =
				URpgInventoryAutomationTestUnitItemDefinition::
					StaticClass();
			Row.StackCount = 1;
		}
		return Pickup;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryViewModelCommitInvalidationTest,
	"SurvivalRpg.Inventory.ViewModel.Invalidation.CommitMessagesCoalescePerConsumer",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryViewModelCommitInvalidationTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryViewModelInvalidationTests;

	FScopedInvalidationWorld TestWorld;
	FPlayerFixture Player;
	if (!TestTrue(
			TEXT("The commit invalidation world exists"),
			TestWorld.IsValid()) ||
		!TestTrue(
			TEXT("The complete player fixture exists"),
			CreatePlayerFixture(
				TestWorld,
				TEXT("CommitInvalidation"),
				Player)))
	{
		return false;
	}

	URpgInventoryPanelViewModel* PanelViewModel =
		NewObject<URpgInventoryPanelViewModel>(
			Player.Controller,
			NAME_None,
			RF_Transient);
	URpgPlayerInventoryViewModel* PlayerViewModel =
		NewObject<URpgPlayerInventoryViewModel>(
			Player.Controller,
			NAME_None,
			RF_Transient);
	URpgActionBarViewModel* ActionBarViewModel =
		NewObject<URpgActionBarViewModel>(
			Player.Controller,
			NAME_None,
			RF_Transient);
	if (!TestNotNull(TEXT("The panel VM exists"), PanelViewModel) ||
		!TestNotNull(
			TEXT("The player aggregate VM exists"),
			PlayerViewModel) ||
		!TestNotNull(
			TEXT("The standalone actionbar VM exists"),
			ActionBarViewModel))
	{
		return false;
	}

	PanelViewModel->BindInventory(Player.Inventory);
	PlayerViewModel->BindPlayerController(Player.Controller);
	ActionBarViewModel->BindPlayerController(Player.Controller);

	FScopedNoArgumentDelegateCounter PanelCounter(
		PanelViewModel->OnEntriesChanged,
		PanelViewModel);
	FScopedNoArgumentDelegateCounter GearCounter(
		PlayerViewModel->OnGearSlotsChanged,
		PlayerViewModel);
	FScopedNoArgumentDelegateCounter GroupCounter(
		PlayerViewModel->OnSlotGroupsChanged,
		PlayerViewModel);
	FScopedNoArgumentDelegateCounter PlayerActionBarCounter(
		PlayerViewModel->OnActionBarSlotsChanged,
		PlayerViewModel);
	FScopedNoArgumentDelegateCounter StandaloneActionBarCounter(
		ActionBarViewModel->OnSlotsChanged,
		ActionBarViewModel);
	TestTrue(
		TEXT("Every commit delegate counter is bound"),
		PanelCounter.IsBound() &&
			GearCounter.IsBound() &&
			GroupCounter.IsBound() &&
			PlayerActionBarCounter.IsBound() &&
			StandaloneActionBarCounter.IsBound());

	TestWorld.PrimeTimerManager();
	int32 RawInventoryMessageCount = 0;
	FScopedGameplayMessageListener RawMessageListener;
	RawMessageListener.Set(
		UGameplayMessageSubsystem::Get(
			TestWorld.GetWorld()).
				RegisterListener<FRpgInventoryChangeMessage>(
					GetInventoryChangedChannel(),
					[&RawInventoryMessageCount,
					 ExpectedInventory = Player.Inventory](
						FGameplayTag,
						const FRpgInventoryChangeMessage& Message)
					{
						if (Message.InventoryOwner ==
							ExpectedInventory)
						{
							++RawInventoryMessageCount;
						}
					}));

	const int32 RevisionBeforeCommit =
		Player.Inventory->GetInventoryRevision();
	const FInventoryPickup Pickup = MakeTwoRowUnitPickup();
	TArray<FRpgInventoryItemId> AffectedItemIds;
	const FRpgInventoryMutationResult Result =
		Player.Inventory->AddPickupBatch(
			Pickup,
			AffectedItemIds);

	TestTrue(
		TEXT("The two-row pickup commits successfully"),
		Result.IsSuccess());
	TestEqual(
		TEXT("The commit reports two affected items"),
		AffectedItemIds.Num(),
		2);
	TestEqual(
		TEXT("The authoritative commit still emits two raw row messages"),
		RawInventoryMessageCount,
		2);
	TestEqual(
		TEXT("The authoritative commit advances revision exactly once"),
		Player.Inventory->GetInventoryRevision(),
		RevisionBeforeCommit + 1);
	TestEqual(
		TEXT("The panel does not refresh synchronously per row message"),
		PanelCounter.GetCount(),
		0);
	TestEqual(
		TEXT("Inventory messages do not refresh Gear synchronously"),
		GearCounter.GetCount(),
		0);
	TestEqual(
		TEXT("The player groups do not refresh synchronously per row message"),
		GroupCounter.GetCount(),
		0);
	TestEqual(
		TEXT("The player actionbar does not refresh synchronously per row message"),
		PlayerActionBarCounter.GetCount(),
		0);
	TestEqual(
		TEXT("The standalone actionbar does not refresh synchronously per row message"),
		StandaloneActionBarCounter.GetCount(),
		0);

	TestWorld.AdvanceTimerFrame();
	TestEqual(
		TEXT("The panel coalesces the complete commit into one refresh"),
		PanelCounter.GetCount(),
		1);
	TestEqual(
		TEXT("Inventory-only invalidation leaves Gear untouched"),
		GearCounter.GetCount(),
		0);
	TestEqual(
		TEXT("The player groups refresh once for the complete commit"),
		GroupCounter.GetCount(),
		1);
	TestEqual(
		TEXT("The player actionbar refreshes once for the complete commit"),
		PlayerActionBarCounter.GetCount(),
		1);
	TestEqual(
		TEXT("The standalone actionbar refreshes once for the complete commit"),
		StandaloneActionBarCounter.GetCount(),
		1);
	TestEqual(
		TEXT("The panel projects both committed inventory rows"),
		PanelViewModel->GetEntries().Num(),
		2);
	TestEqual(
		TEXT("The UI refresh does not advance authoritative revision"),
		Player.Inventory->GetInventoryRevision(),
		RevisionBeforeCommit + 1);
	TestEqual(
		TEXT("The UI refresh emits no additional inventory messages"),
		RawInventoryMessageCount,
		2);

	TestWorld.AdvanceTimerFrame();
	TestEqual(
		TEXT("A second timer frame produces no panel refresh"),
		PanelCounter.GetCount(),
		1);
	TestEqual(
		TEXT("A second timer frame produces no player-group refresh"),
		GroupCounter.GetCount(),
		1);
	TestEqual(
		TEXT("A second timer frame produces no standalone actionbar refresh"),
		StandaloneActionBarCounter.GetCount(),
		1);

	RawMessageListener.Unregister();
	PanelCounter.Unbind();
	GearCounter.Unbind();
	GroupCounter.Unbind();
	PlayerActionBarCounter.Unbind();
	StandaloneActionBarCounter.Unbind();
	ActionBarViewModel->UnbindActionBar();
	PlayerViewModel->UnbindPlayerInventory();
	PanelViewModel->UnbindInventory();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryViewModelSourceIsolationTest,
	"SurvivalRpg.Inventory.ViewModel.Invalidation.DistinctInventoriesRemainIsolated",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryViewModelSourceIsolationTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryViewModelInvalidationTests;

	FScopedInvalidationWorld TestWorld;
	if (!TestTrue(
			TEXT("The source-isolation world exists"),
			TestWorld.IsValid()))
	{
		return false;
	}

	URpgInventoryManagerComponent* InventoryA =
		TestWorld.CreateInventory(TEXT("InvalidationInventoryA"));
	URpgInventoryManagerComponent* InventoryB =
		TestWorld.CreateInventory(TEXT("InvalidationInventoryB"));
	URpgInventoryManagerComponent* InventoryC =
		TestWorld.CreateInventory(TEXT("InvalidationInventoryC"));
	if (!TestNotNull(TEXT("Inventory A exists"), InventoryA) ||
		!TestNotNull(TEXT("Inventory B exists"), InventoryB) ||
		!TestNotNull(TEXT("Foreign inventory C exists"), InventoryC) ||
		!TestNotNull(
			TEXT("Inventory A receives its fixture item"),
			InventoryA->GrantItemDefinition(
				URpgInventoryAutomationTestUnitItemDefinition::
					StaticClass(),
				1)) ||
		!TestNotNull(
			TEXT("Inventory B receives its fixture items"),
			InventoryB->GrantItemDefinition(
				URpgInventoryAutomationTestUnitItemDefinition::
					StaticClass(),
				2)))
	{
		return false;
	}

	URpgInventoryPanelViewModel* PanelA =
		NewObject<URpgInventoryPanelViewModel>(
			InventoryA,
			NAME_None,
			RF_Transient);
	URpgInventoryPanelViewModel* PanelB =
		NewObject<URpgInventoryPanelViewModel>(
			InventoryB,
			NAME_None,
			RF_Transient);
	if (!TestNotNull(TEXT("Panel A exists"), PanelA) ||
		!TestNotNull(TEXT("Panel B exists"), PanelB))
	{
		return false;
	}

	PanelA->BindInventory(InventoryA);
	PanelB->BindInventory(InventoryB);
	FScopedNoArgumentDelegateCounter CounterA(
		PanelA->OnEntriesChanged,
		PanelA);
	FScopedNoArgumentDelegateCounter CounterB(
		PanelB->OnEntriesChanged,
		PanelB);
	TestTrue(
		TEXT("Both source-isolation counters are bound"),
		CounterA.IsBound() && CounterB.IsBound());
	TestWorld.PrimeTimerManager();

	for (int32 Index = 0; Index < 3; ++Index)
	{
		BroadcastInventoryChanged(
			TestWorld.GetWorld(),
			InventoryA);
	}
	for (int32 Index = 0; Index < 2; ++Index)
	{
		BroadcastInventoryChanged(
			TestWorld.GetWorld(),
			InventoryC);
	}
	TestEqual(
		TEXT("Panel A waits for its queued refresh"),
		CounterA.GetCount(),
		0);
	TestEqual(
		TEXT("Panel B ignores A and foreign C before the tick"),
		CounterB.GetCount(),
		0);
	TestWorld.AdvanceTimerFrame();
	TestEqual(
		TEXT("Panel A coalesces its three messages"),
		CounterA.GetCount(),
		1);
	TestEqual(
		TEXT("Panel B remains untouched by other inventories"),
		CounterB.GetCount(),
		0);

	CounterA.ResetCount();
	CounterB.ResetCount();
	for (int32 Index = 0; Index < 3; ++Index)
	{
		BroadcastInventoryChanged(
			TestWorld.GetWorld(),
			Index % 2 == 0 ? InventoryB : InventoryA);
	}
	BroadcastInventoryChanged(
		TestWorld.GetWorld(),
		InventoryA);
	BroadcastInventoryChanged(
		TestWorld.GetWorld(),
		InventoryB);
	TestEqual(
		TEXT("Interleaved A messages remain deferred"),
		CounterA.GetCount(),
		0);
	TestEqual(
		TEXT("Interleaved B messages remain deferred"),
		CounterB.GetCount(),
		0);
	TestWorld.AdvanceTimerFrame();
	TestEqual(
		TEXT("Panel A owns one independent invalidation queue"),
		CounterA.GetCount(),
		1);
	TestEqual(
		TEXT("Panel B owns one independent invalidation queue"),
		CounterB.GetCount(),
		1);
	TestEqual(
		TEXT("Panel A retains its exact source"),
		PanelA->GetObservedInventory(),
		InventoryA);
	TestEqual(
		TEXT("Panel B retains its exact source"),
		PanelB->GetObservedInventory(),
		InventoryB);
	TestEqual(
		TEXT("Panel A projects only its single item"),
		PanelA->GetEntries().Num(),
		1);
	TestEqual(
		TEXT("Panel B projects only its two items"),
		PanelB->GetEntries().Num(),
		2);

	CounterA.Unbind();
	CounterB.Unbind();
	PanelA->UnbindInventory();
	PanelB->UnbindInventory();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryViewModelPendingLifecycleTest,
	"SurvivalRpg.Inventory.ViewModel.Invalidation.PendingLifecycleRejectsStaleCallbacks",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryViewModelPendingLifecycleTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryViewModelInvalidationTests;

	FScopedInvalidationWorld TestWorld;
	if (!TestTrue(
			TEXT("The pending-lifecycle world exists"),
			TestWorld.IsValid()))
	{
		return false;
	}

	URpgInventoryManagerComponent* InventoryA =
		TestWorld.CreateInventory(TEXT("PendingLifecycleInventoryA"));
	URpgInventoryManagerComponent* InventoryB =
		TestWorld.CreateInventory(TEXT("PendingLifecycleInventoryB"));
	if (!TestNotNull(TEXT("Lifecycle inventory A exists"), InventoryA) ||
		!TestNotNull(TEXT("Lifecycle inventory B exists"), InventoryB) ||
		!TestNotNull(
			TEXT("Lifecycle inventory A receives its fixture item"),
			InventoryA->GrantItemDefinition(
				URpgInventoryAutomationTestUnitItemDefinition::
					StaticClass(),
				1)) ||
		!TestNotNull(
			TEXT("Lifecycle inventory B receives its fixture items"),
			InventoryB->GrantItemDefinition(
				URpgInventoryAutomationTestUnitItemDefinition::
					StaticClass(),
				2)))
	{
		return false;
	}

	URpgInventoryPanelViewModel* Panel =
		NewObject<URpgInventoryPanelViewModel>(
			InventoryA,
			NAME_None,
			RF_Transient);
	if (!TestNotNull(TEXT("The lifecycle panel exists"), Panel))
	{
		return false;
	}

	Panel->BindInventory(InventoryA);
	FScopedNoArgumentDelegateCounter Counter(
		Panel->OnEntriesChanged,
		Panel);
	TestTrue(
		TEXT("The lifecycle delegate counter is bound"),
		Counter.IsBound());
	TestWorld.PrimeTimerManager();

	// A queued callback must not survive a direct source rebind when B did
	// not itself request a refresh.
	BroadcastInventoryChanged(
		TestWorld.GetWorld(),
		InventoryA);
	TestEqual(
		TEXT("The A invalidation is initially deferred"),
		Counter.GetCount(),
		0);
	Panel->BindInventory(InventoryB);
	Counter.ResetCount();
	TestWorld.AdvanceTimerFrame();
	TestEqual(
		TEXT("A direct A-to-B rebind cancels the stale A callback"),
		Counter.GetCount(),
		0);
	TestEqual(
		TEXT("The direct rebind retains only final source B"),
		Panel->GetObservedInventory(),
		InventoryB);

	// Explicit unbind has its own synchronous legacy signal. After that
	// signal is removed from the observation window no queued callback may
	// publish an additional empty refresh.
	Panel->BindInventory(InventoryA);
	Counter.ResetCount();
	BroadcastInventoryChanged(
		TestWorld.GetWorld(),
		InventoryA);
	Panel->UnbindInventory();
	Counter.ResetCount();
	TestWorld.AdvanceTimerFrame();
	TestEqual(
		TEXT("Explicit unbind cancels the pending callback"),
		Counter.GetCount(),
		0);
	TestNull(
		TEXT("Explicit unbind leaves no observed inventory"),
		Panel->GetObservedInventory());

	// Canceling A, unbinding, and then scheduling B must leave exactly one
	// live timer owned by the final binding.
	Panel->BindInventory(InventoryA);
	Counter.ResetCount();
	BroadcastInventoryChanged(
		TestWorld.GetWorld(),
		InventoryA);
	Panel->UnbindInventory();
	Panel->BindInventory(InventoryB);
	Counter.ResetCount();
	BroadcastInventoryChanged(
		TestWorld.GetWorld(),
		InventoryB);
	BroadcastInventoryChanged(
		TestWorld.GetWorld(),
		InventoryB);
	TestEqual(
		TEXT("The final B invalidations remain deferred"),
		Counter.GetCount(),
		0);
	TestWorld.AdvanceTimerFrame();
	TestEqual(
		TEXT("Only the final B source refreshes"),
		Counter.GetCount(),
		1);
	TestEqual(
		TEXT("The final queued refresh still observes B"),
		Panel->GetObservedInventory(),
		InventoryB);
	TestEqual(
		TEXT("The final queued refresh projects B's two items"),
		Panel->GetEntries().Num(),
		2);
	TestWorld.AdvanceTimerFrame();
	TestEqual(
		TEXT("No stale lifecycle timer survives another frame"),
		Counter.GetCount(),
		1);

	Counter.Unbind();
	Panel->UnbindInventory();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgInventoryViewModelReentrantInvalidationTest,
	"SurvivalRpg.Inventory.ViewModel.Invalidation.ReentrantMessageDefersToNextFrame",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgInventoryViewModelReentrantInvalidationTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryViewModelInvalidationTests;

	FScopedInvalidationWorld TestWorld;
	if (!TestTrue(
			TEXT("The reentrant invalidation world exists"),
			TestWorld.IsValid()))
	{
		return false;
	}

	URpgInventoryManagerComponent* Inventory =
		TestWorld.CreateInventory(
			TEXT("ReentrantInvalidationInventory"));
	URpgInventoryPanelViewModel* Panel =
		NewObject<URpgInventoryPanelViewModel>(
			Inventory,
			NAME_None,
			RF_Transient);
	URpgInventoryAutomationTestReentrantInvalidationTarget* Target =
		NewObject<
			URpgInventoryAutomationTestReentrantInvalidationTarget>(
				Panel,
				NAME_None,
				RF_Transient);
	if (!TestNotNull(
			TEXT("The reentrant inventory exists"),
			Inventory) ||
		!TestNotNull(
			TEXT("The reentrant panel exists"),
			Panel) ||
		!TestNotNull(
			TEXT("The reentrant delegate target exists"),
			Target))
	{
		return false;
	}

	Panel->BindInventory(Inventory);
	Target->Configure(
		TestWorld.GetWorld(),
		Inventory);
	Panel->OnEntriesChanged.AddUniqueDynamic(
		Target,
		&URpgInventoryAutomationTestReentrantInvalidationTarget::RecordAndBroadcastInventoryChange);
	TestWorld.PrimeTimerManager();

	BroadcastInventoryChanged(
		TestWorld.GetWorld(),
		Inventory);
	TestEqual(
		TEXT("The initial inventory message remains deferred"),
		Target->GetInvocationCount(),
		0);

	TestWorld.AdvanceTimerFrame();
	TestEqual(
		TEXT("The first frame publishes exactly one panel refresh"),
		Target->GetInvocationCount(),
		1);

	// The first callback emitted one nested StackChanged message. It must
	// remain queued instead of executing again in this timer-manager tick.
	TestWorld.AdvanceTimerFrame();
	TestEqual(
		TEXT("The reentrant invalidation is preserved for the next frame"),
		Target->GetInvocationCount(),
		2);
	TestWorld.AdvanceTimerFrame();
	TestEqual(
		TEXT("No third refresh remains after the one nested message"),
		Target->GetInvocationCount(),
		2);

	Panel->OnEntriesChanged.RemoveDynamic(
		Target,
		&URpgInventoryAutomationTestReentrantInvalidationTarget::RecordAndBroadcastInventoryChange);
	Panel->UnbindInventory();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgPlayerInventoryViewModelSelectiveDomainsTest,
	"SurvivalRpg.Inventory.ViewModel.Invalidation.PlayerAggregateUnionsSelectiveDomains",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgPlayerInventoryViewModelSelectiveDomainsTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryViewModelInvalidationTests;

	FScopedInvalidationWorld TestWorld;
	FPlayerFixture Player;
	if (!TestTrue(
			TEXT("The selective-domain world exists"),
			TestWorld.IsValid()) ||
		!TestTrue(
			TEXT("The selective-domain player fixture exists"),
			CreatePlayerFixture(
				TestWorld,
				TEXT("SelectiveDomains"),
				Player)))
	{
		return false;
	}

	URpgInventoryManagerComponent* ForeignInventory =
		TestWorld.CreateInventory(
			TEXT("SelectiveDomainsForeignInventory"));
	URpgPlayerInventoryViewModel* ViewModel =
		NewObject<URpgPlayerInventoryViewModel>(
			Player.Controller,
			NAME_None,
			RF_Transient);
	if (!TestNotNull(
			TEXT("The foreign inventory exists"),
			ForeignInventory) ||
		!TestNotNull(
			TEXT("The selective-domain player VM exists"),
			ViewModel))
	{
		return false;
	}

	ViewModel->BindPlayerController(Player.Controller);
	FScopedNoArgumentDelegateCounter GearCounter(
		ViewModel->OnGearSlotsChanged,
		ViewModel);
	FScopedNoArgumentDelegateCounter GroupCounter(
		ViewModel->OnSlotGroupsChanged,
		ViewModel);
	FScopedNoArgumentDelegateCounter ActionBarCounter(
		ViewModel->OnActionBarSlotsChanged,
		ViewModel);
	TestTrue(
		TEXT("Every selective-domain counter is bound"),
		GearCounter.IsBound() &&
			GroupCounter.IsBound() &&
			ActionBarCounter.IsBound());
	TestWorld.PrimeTimerManager();

	for (int32 Index = 0; Index < 3; ++Index)
	{
		BroadcastActionBarChanged(
			TestWorld.GetWorld(),
			Player.Controller,
			Player.ActionBar);
	}
	for (int32 Index = 0; Index < 2; ++Index)
	{
		BroadcastInventoryChanged(
			TestWorld.GetWorld(),
			ForeignInventory);
	}
	TestEqual(
		TEXT("Actionbar-only messages defer Gear"),
		GearCounter.GetCount(),
		0);
	TestEqual(
		TEXT("Actionbar-only messages defer groups"),
		GroupCounter.GetCount(),
		0);
	TestEqual(
		TEXT("Actionbar-only messages are queued"),
		ActionBarCounter.GetCount(),
		0);
	TestWorld.AdvanceTimerFrame();
	TestEqual(
		TEXT("Actionbar-only invalidation does not refresh Gear"),
		GearCounter.GetCount(),
		0);
	TestEqual(
		TEXT("Actionbar-only invalidation does not refresh groups"),
		GroupCounter.GetCount(),
		0);
	TestEqual(
		TEXT("Three actionbar messages coalesce into one refresh"),
		ActionBarCounter.GetCount(),
		1);

	GearCounter.ResetCount();
	GroupCounter.ResetCount();
	ActionBarCounter.ResetCount();
	for (int32 Index = 0; Index < 2; ++Index)
	{
		BroadcastInventoryChanged(
			TestWorld.GetWorld(),
			Player.Inventory);
		BroadcastLayoutChanged(
			TestWorld.GetWorld(),
			Player.Controller,
			Player.InventoryLayout);
	}
	TestEqual(
		TEXT("Inventory and layout messages do not synchronously refresh Gear"),
		GearCounter.GetCount(),
		0);
	TestEqual(
		TEXT("Inventory and layout messages defer groups"),
		GroupCounter.GetCount(),
		0);
	TestEqual(
		TEXT("Inventory and layout messages defer the actionbar"),
		ActionBarCounter.GetCount(),
		0);
	TestWorld.AdvanceTimerFrame();
	TestEqual(
		TEXT("Inventory and layout invalidation leaves Gear untouched"),
		GearCounter.GetCount(),
		0);
	TestEqual(
		TEXT("Inventory and layout domains union into one group refresh"),
		GroupCounter.GetCount(),
		1);
	TestEqual(
		TEXT("Inventory and layout domains union into one actionbar refresh"),
		ActionBarCounter.GetCount(),
		1);

	GearCounter.ResetCount();
	GroupCounter.ResetCount();
	ActionBarCounter.ResetCount();
	BroadcastEquipmentChanged(
		TestWorld.GetWorld(),
		Player.Controller);
	BroadcastInventoryChanged(
		TestWorld.GetWorld(),
		Player.Inventory);
	BroadcastLayoutChanged(
		TestWorld.GetWorld(),
		Player.Controller,
		Player.InventoryLayout);
	BroadcastActionBarChanged(
		TestWorld.GetWorld(),
		Player.Controller,
		Player.ActionBar);
	TestEqual(
		TEXT("The mixed domain burst remains deferred for Gear"),
		GearCounter.GetCount(),
		0);
	TestEqual(
		TEXT("The mixed domain burst remains deferred for groups"),
		GroupCounter.GetCount(),
		0);
	TestEqual(
		TEXT("The mixed domain burst remains deferred for the actionbar"),
		ActionBarCounter.GetCount(),
		0);
	TestWorld.AdvanceTimerFrame();
	TestEqual(
		TEXT("The mixed domain burst refreshes Gear once"),
		GearCounter.GetCount(),
		1);
	TestEqual(
		TEXT("The mixed domain burst refreshes groups once"),
		GroupCounter.GetCount(),
		1);
	TestEqual(
		TEXT("The mixed domain burst refreshes the actionbar once"),
		ActionBarCounter.GetCount(),
		1);
	TestWorld.AdvanceTimerFrame();
	TestEqual(
		TEXT("No mixed-domain refresh survives another frame"),
		GearCounter.GetCount() +
			GroupCounter.GetCount() +
			ActionBarCounter.GetCount(),
		3);

	GearCounter.Unbind();
	GroupCounter.Unbind();
	ActionBarCounter.Unbind();
	ViewModel->UnbindPlayerInventory();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgBaseStorageViewModelInvalidationTest,
	"SurvivalRpg.BaseStorage.ViewModel.Invalidation.FirstResourceRowCoalescesProducerMessages",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgBaseStorageViewModelInvalidationTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryViewModelInvalidationTests;

	FScopedInvalidationWorld TestWorld;
	if (!TestTrue(
			TEXT("The base-storage invalidation world exists"),
			TestWorld.IsValid()))
	{
		return false;
	}

	AActor* StorageOwner =
		TestWorld.SpawnActor(TEXT("InvalidationBaseStorageOwner"));
	URpgBaseStorageComponent* BaseStorage =
		TestWorld.CreateComponent<URpgBaseStorageComponent>(
			StorageOwner,
			TEXT("BaseStorage"));
	URpgBaseStorageViewModel* ViewModel =
		NewObject<URpgBaseStorageViewModel>(
			BaseStorage,
			NAME_None,
			RF_Transient);
	if (!TestNotNull(
			TEXT("The base-storage owner exists"),
			StorageOwner) ||
		!TestNotNull(
			TEXT("The base-storage component exists"),
			BaseStorage) ||
		!TestNotNull(
			TEXT("The base-storage VM exists"),
			ViewModel))
	{
		return false;
	}

	const TArray<TSubclassOf<URpgInventoryItemDefinition>>
		AllowedResources;
	ViewModel->BindBaseStorage(
		BaseStorage,
		AllowedResources);
	FScopedNoArgumentDelegateCounter Counter(
		ViewModel->OnResourcesChanged,
		ViewModel);
	TestTrue(
		TEXT("The base-storage delegate counter is bound"),
		Counter.IsBound());
	TestWorld.PrimeTimerManager();

	int32 RawMessageCount = 0;
	int32 OrderChangedMessageCount = 0;
	int32 CapacityChangedMessageCount = 0;
	FScopedGameplayMessageListener RawMessageListener;
	RawMessageListener.Set(
		UGameplayMessageSubsystem::Get(
			TestWorld.GetWorld()).
				RegisterListener<FRpgBaseResourceChangeMessage>(
					GetBaseStorageChangedChannel(),
					[&RawMessageCount,
					 &OrderChangedMessageCount,
					 &CapacityChangedMessageCount,
					 ExpectedStorage = BaseStorage](
						FGameplayTag,
						const FRpgBaseResourceChangeMessage& Message)
					{
						if (Message.StorageOwner !=
							ExpectedStorage)
						{
							return;
						}

						++RawMessageCount;
						OrderChangedMessageCount +=
							Message.bOrderChanged ? 1 : 0;
						CapacityChangedMessageCount +=
							Message.bCapacityChanged ? 1 : 0;
					}));

	const TSubclassOf<URpgInventoryItemDefinition>
		MaterialDefinition =
			URpgInventoryAutomationTestMaterialDefinition::
				StaticClass();
	BaseStorage->AddResourceCapacity(
		MaterialDefinition,
		10);
	TestEqual(
		TEXT("First row creation keeps both synchronous producer messages"),
		RawMessageCount,
		2);
	TestEqual(
		TEXT("One producer message announces the new row order"),
		OrderChangedMessageCount,
		1);
	TestEqual(
		TEXT("One producer message announces the new capacity"),
		CapacityChangedMessageCount,
		1);
	TestEqual(
		TEXT("The base-storage VM does not rebuild per producer message"),
		Counter.GetCount(),
		0);
	TestTrue(
		TEXT("The queued VM still exposes its previous empty snapshot"),
		ViewModel->GetResources().IsEmpty());

	TestWorld.AdvanceTimerFrame();
	TestEqual(
		TEXT("Both producer messages coalesce into one base-storage refresh"),
		Counter.GetCount(),
		1);
	TestEqual(
		TEXT("The base-storage VM projects the new resource row"),
		ViewModel->GetResources().Num(),
		1);
	if (ViewModel->GetResources().Num() == 1)
	{
		TestEqual(
			TEXT("The projected row keeps the material definition"),
			ViewModel->GetResources()[0]->GetItemDefinition(),
			MaterialDefinition);
	}
	TestEqual(
		TEXT("The authoritative resource capacity is unchanged by UI refresh"),
		BaseStorage->GetResourceCapacity(MaterialDefinition),
		10);
	TestWorld.AdvanceTimerFrame();
	TestEqual(
		TEXT("No base-storage refresh survives another frame"),
		Counter.GetCount(),
		1);

	RawMessageListener.Unregister();
	Counter.Unbind();
	ViewModel->UnbindBaseStorage();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgCraftingViewModelInventoryInvalidationTest,
	"SurvivalRpg.Crafting.ViewModel.Invalidation.InventoryBurstsCoalesceByRelevantSource",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgCraftingViewModelInventoryInvalidationTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgInventoryViewModelInvalidationTests;

	FScopedInvalidationWorld TestWorld;
	if (!TestTrue(
			TEXT("The crafting invalidation world exists"),
			TestWorld.IsValid()))
	{
		return false;
	}

	AActor* StationOwner =
		TestWorld.SpawnActor(TEXT("InvalidationCraftingStationOwner"));
	AActor* RequestingActor =
		TestWorld.SpawnActor(TEXT("InvalidationCraftingRequester"));
	AActor* ForeignActor =
		TestWorld.SpawnActor(TEXT("InvalidationCraftingForeignActor"));
	URpgCraftingStationComponent* Station =
		TestWorld.CreateComponent<URpgCraftingStationComponent>(
			StationOwner,
			TEXT("CraftingStation"));
	URpgInventoryManagerComponent* ResourceInventory =
		TestWorld.CreateComponent<URpgInventoryManagerComponent>(
			RequestingActor,
			TEXT("ResourceInventory"));
	URpgInventoryManagerComponent* ForeignInventory =
		TestWorld.CreateComponent<URpgInventoryManagerComponent>(
			ForeignActor,
			TEXT("ForeignInventory"));
	URpgCraftingStationViewModel* ViewModel =
		NewObject<URpgCraftingStationViewModel>(
			Station,
			NAME_None,
			RF_Transient);
	if (!TestNotNull(
			TEXT("The crafting station owner exists"),
			StationOwner) ||
		!TestNotNull(
			TEXT("The crafting requester exists"),
			RequestingActor) ||
		!TestNotNull(
			TEXT("The crafting foreign actor exists"),
			ForeignActor) ||
		!TestNotNull(
			TEXT("The crafting station exists"),
			Station) ||
		!TestNotNull(
			TEXT("The requester resource inventory exists"),
			ResourceInventory) ||
		!TestNotNull(
			TEXT("The foreign resource inventory exists"),
			ForeignInventory) ||
		!TestNotNull(
			TEXT("The crafting station VM exists"),
			ViewModel))
	{
		return false;
	}

	ViewModel->BindCraftingStation(
		Station,
		RequestingActor);
	FScopedNoArgumentDelegateCounter DetailsCounter(
		ViewModel->OnSelectedRecipeDetailsChanged,
		ViewModel);
	TestTrue(
		TEXT("The crafting details counter is bound"),
		DetailsCounter.IsBound());
	TestWorld.PrimeTimerManager();

	for (int32 Index = 0; Index < 3; ++Index)
	{
		BroadcastInventoryChanged(
			TestWorld.GetWorld(),
			ForeignInventory);
	}
	TestEqual(
		TEXT("Foreign inventory messages do not refresh details synchronously"),
		DetailsCounter.GetCount(),
		0);
	TestWorld.AdvanceTimerFrame();
	TestEqual(
		TEXT("Foreign inventory messages do not queue crafting details"),
		DetailsCounter.GetCount(),
		0);

	DetailsCounter.ResetCount();
	for (int32 Index = 0; Index < 4; ++Index)
	{
		BroadcastInventoryChanged(
			TestWorld.GetWorld(),
			ResourceInventory);
		if (Index < 2)
		{
			BroadcastInventoryChanged(
				TestWorld.GetWorld(),
				ForeignInventory);
		}
	}
	TestEqual(
		TEXT("Relevant crafting inventory messages remain deferred"),
		DetailsCounter.GetCount(),
		0);
	TestWorld.AdvanceTimerFrame();
	TestEqual(
		TEXT("Relevant crafting inventory bursts refresh details once"),
		DetailsCounter.GetCount(),
		1);
	TestWorld.AdvanceTimerFrame();
	TestEqual(
		TEXT("No crafting details refresh survives another frame"),
		DetailsCounter.GetCount(),
		1);

	DetailsCounter.ResetCount();
	for (int32 Index = 0; Index < 3; ++Index)
	{
		BroadcastCraftingStationChanged(
			TestWorld.GetWorld(),
			Station);
	}
	TestEqual(
		TEXT("Station-wide crafting invalidations remain deferred"),
		DetailsCounter.GetCount(),
		0);
	TestWorld.AdvanceTimerFrame();
	TestEqual(
		TEXT("Station-wide crafting bursts refresh details once"),
		DetailsCounter.GetCount(),
		1);

	DetailsCounter.Unbind();
	ViewModel->UnbindCraftingStation();
	return true;
}

#endif
