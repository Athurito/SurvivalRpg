#include "RpgPlayerInventoryWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "RpgStorageInventoryWidget.h"
#include "SurvivalRpg/ActionBar/RpgActionBarComponent.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Equipment/RpgEquipmentLoadoutComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgInventoryAutomationTestTypes.h"
#include "SurvivalRpg/Inventory/RpgInventoryDragDrop.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Inventory/RpgPlayerInventoryLayoutComponent.h"
#include "SurvivalRpg/Mvvm/Inventory/RpgPlayerInventoryViewModels.h"
#include "SurvivalRpg/UI/RpgUIScreenPayload.h"

#include "CommonLocalPlayer.h"
#include "CoreGlobals.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "ICommonInputModule.h"
#include "Misc/AutomationTest.h"
#include "TimerManager.h"
#include "Widgets/SWidget.h"

namespace RpgPlayerStorageInventoryLifecycleIntegrationTests
{
	constexpr float TimerFrameDeltaSeconds = 1.0f / 60.0f;
	constexpr uint8 GearRefreshDomain = 1 << 0;
	constexpr uint8 SlotGroupsRefreshDomain = 1 << 1;
	constexpr uint8 ActionBarRefreshDomain = 1 << 2;
	constexpr uint8 InventoryAndLayoutRefreshDomains =
		SlotGroupsRefreshDomain | ActionBarRefreshDomain;
	constexpr uint8 AllRefreshDomains =
		GearRefreshDomain |
		SlotGroupsRefreshDomain |
		ActionBarRefreshDomain;

	class FScopedPlayerWidgetWorld
	{
	public:
		FScopedPlayerWidgetWorld()
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
			if (!World)
			{
				return;
			}

			FActorSpawnParameters ControllerParameters;
			ControllerParameters.Name = MakeUniqueObjectName(
				World,
				ARpgInventoryAutomationTestPlayerController::StaticClass(),
				TEXT("PlayerStorageLifecycleController"));
			ControllerParameters.ObjectFlags = RF_Transient;
			ControllerParameters.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Controller = World->SpawnActor<
				ARpgInventoryAutomationTestPlayerController>(
				ControllerParameters);

			FActorSpawnParameters PlayerStateParameters;
			PlayerStateParameters.Name = MakeUniqueObjectName(
				World,
				ARpgInventoryAutomationTestPlayerState::StaticClass(),
				TEXT("PlayerStorageLifecyclePlayerState"));
			PlayerStateParameters.ObjectFlags = RF_Transient;
			PlayerStateParameters.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			PlayerState = World->SpawnActor<
				ARpgInventoryAutomationTestPlayerState>(
				PlayerStateParameters);

			LocalPlayer = NewObject<UCommonLocalPlayer>(
				GEngine,
				NAME_None,
				RF_Transient);
			if (!Controller || !PlayerState || !LocalPlayer)
			{
				return;
			}

			Controller->SetPlayerState(PlayerState);
			PlayerState->SetOwner(Controller);

			// InitializeStandalone does not populate the controller list used
			// by UMG's owning-player lookup, so mirror the real local-player
			// relationship explicitly for this isolated UI world.
			World->AddController(Controller);
			Controller->SetPlayer(LocalPlayer);
		}

		~FScopedPlayerWidgetWorld()
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

			GFrameCounter = CachedFrameCounter;
		}

		bool IsValid() const
		{
			return World &&
				Controller &&
				PlayerState &&
				LocalPlayer &&
				Controller->GetRpgPlayerState() == PlayerState &&
				GetCanonicalPlayerInventory() &&
				Controller->GetPlayerInventoryLayoutComponent() &&
				Controller->GetEquipmentLoadoutComponent() &&
				Controller->GetActionBarComponent();
		}

		UWorld* GetWorld() const
		{
			return World;
		}

		ARpgInventoryAutomationTestPlayerController* GetController() const
		{
			return Controller;
		}

		ARpgInventoryAutomationTestPlayerState* GetPlayerState() const
		{
			return PlayerState;
		}

		URpgInventoryManagerComponent* GetCanonicalPlayerInventory() const
		{
			return PlayerState
				? PlayerState->GetInventoryManagerComponent()
				: nullptr;
		}

		URpgInventoryManagerComponent* CreateSecondaryInventory(
			const TCHAR* DebugName) const
		{
			if (!World)
			{
				return nullptr;
			}

			FActorSpawnParameters OwnerParameters;
			OwnerParameters.Name = MakeUniqueObjectName(
				World,
				AActor::StaticClass(),
				FName(DebugName));
			OwnerParameters.ObjectFlags = RF_Transient;
			OwnerParameters.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AActor* Owner = World->SpawnActor<AActor>(OwnerParameters);
			if (!Owner)
			{
				return nullptr;
			}

			URpgInventoryManagerComponent* Inventory =
				NewObject<URpgInventoryManagerComponent>(
					Owner,
					MakeUniqueObjectName(
						Owner,
						URpgInventoryManagerComponent::StaticClass(),
						TEXT("StorageInventory")),
					RF_Transient);
			if (!Inventory)
			{
				return nullptr;
			}

			Owner->AddInstanceComponent(Inventory);
			Inventory->RegisterComponent();
			return Inventory;
		}

		/** Drains existing next-tick work and makes later queues eligible on the following explicit frame. */
		void PrimeTimerManager() const
		{
			if (World)
			{
				++GFrameCounter;
				World->GetTimerManager().Tick(0.0f);
			}
		}

		/** Advances only presentation timers, avoiding unrelated standalone-world actor ticks. */
		void AdvanceTimerFrame() const
		{
			if (World)
			{
				++GFrameCounter;
				World->GetTimerManager().Tick(
					TimerFrameDeltaSeconds);
			}
		}

	private:
		const uint64 CachedFrameCounter = GFrameCounter;
		TObjectPtr<UGameInstance> GameInstance;
		TObjectPtr<UWorld> World;
		TObjectPtr<ARpgInventoryAutomationTestPlayerController> Controller;
		TObjectPtr<ARpgInventoryAutomationTestPlayerState> PlayerState;
		TObjectPtr<UCommonLocalPlayer> LocalPlayer;
	};

	struct FViewModelDelegateCounters
	{
		bool Bind(URpgPlayerInventoryViewModel* ViewModel)
		{
			if (!ViewModel)
			{
				return false;
			}

			Gear = NewObject<
				URpgInventoryAutomationTestDynamicDelegateCounter>(
					ViewModel,
					NAME_None,
					RF_Transient);
			Groups = NewObject<
				URpgInventoryAutomationTestDynamicDelegateCounter>(
					ViewModel,
					NAME_None,
					RF_Transient);
			ActionBar = NewObject<
				URpgInventoryAutomationTestDynamicDelegateCounter>(
					ViewModel,
					NAME_None,
					RF_Transient);
			if (!Gear || !Groups || !ActionBar)
			{
				return false;
			}

			ViewModel->OnGearSlotsChanged.AddUniqueDynamic(
				Gear,
				&URpgInventoryAutomationTestDynamicDelegateCounter::RecordInvocation);
			ViewModel->OnSlotGroupsChanged.AddUniqueDynamic(
				Groups,
				&URpgInventoryAutomationTestDynamicDelegateCounter::RecordInvocation);
			ViewModel->OnActionBarSlotsChanged.AddUniqueDynamic(
				ActionBar,
				&URpgInventoryAutomationTestDynamicDelegateCounter::RecordInvocation);
			return true;
		}

		void Reset() const
		{
			if (Gear)
			{
				Gear->ResetInvocationCount();
			}
			if (Groups)
			{
				Groups->ResetInvocationCount();
			}
			if (ActionBar)
			{
				ActionBar->ResetInvocationCount();
			}
		}

		void Unbind(URpgPlayerInventoryViewModel* ViewModel)
		{
			if (ViewModel)
			{
				if (Gear)
				{
					ViewModel->OnGearSlotsChanged.RemoveDynamic(
						Gear,
						&URpgInventoryAutomationTestDynamicDelegateCounter::RecordInvocation);
				}
				if (Groups)
				{
					ViewModel->OnSlotGroupsChanged.RemoveDynamic(
						Groups,
						&URpgInventoryAutomationTestDynamicDelegateCounter::RecordInvocation);
				}
				if (ActionBar)
				{
					ViewModel->OnActionBarSlotsChanged.RemoveDynamic(
						ActionBar,
						&URpgInventoryAutomationTestDynamicDelegateCounter::RecordInvocation);
				}
			}

			Gear = nullptr;
			Groups = nullptr;
			ActionBar = nullptr;
		}

		int32 GetGearCount() const
		{
			return Gear ? Gear->GetInvocationCount() : 0;
		}

		int32 GetGroupCount() const
		{
			return Groups ? Groups->GetInvocationCount() : 0;
		}

		int32 GetActionBarCount() const
		{
			return ActionBar ? ActionBar->GetInvocationCount() : 0;
		}

		TObjectPtr<URpgInventoryAutomationTestDynamicDelegateCounter> Gear;
		TObjectPtr<URpgInventoryAutomationTestDynamicDelegateCounter> Groups;
		TObjectPtr<URpgInventoryAutomationTestDynamicDelegateCounter> ActionBar;
	};

	URpgInventoryScreenPayload* MakeStoragePayload(
		UObject* Outer,
		URpgInventoryManagerComponent* CanonicalPlayerInventory,
		URpgInventoryManagerComponent* SecondaryInventory)
	{
		URpgInventoryScreenPayload* Payload =
			NewObject<URpgInventoryScreenPayload>(
				Outer,
				NAME_None,
				RF_Transient);
		if (Payload)
		{
			Payload->PrimaryInventory = CanonicalPlayerInventory;
			Payload->SecondaryInventory = SecondaryInventory;
		}
		return Payload;
	}

	void BroadcastInventoryChanged(
		UWorld* World,
		URpgInventoryManagerComponent* Inventory)
	{
		if (!World || !Inventory)
		{
			return;
		}

		FRpgInventoryChangeMessage InventoryMessage;
		InventoryMessage.InventoryOwner = Inventory;
		UGameplayMessageSubsystem::Get(World).BroadcastMessage(
			FGameplayTag::RequestGameplayTag(
				TEXT("Rpg.Inventory.Message.StackChanged")),
			InventoryMessage);
	}

	void BroadcastLayoutChanged(
		UWorld* World,
		ARpgInventoryAutomationTestPlayerController* Controller,
		URpgPlayerInventoryLayoutComponent* InventoryLayout)
	{
		if (!World || !Controller || !InventoryLayout)
		{
			return;
		}

		FRpgPlayerInventoryLayoutChangedMessage LayoutMessage;
		LayoutMessage.Owner = Controller;
		LayoutMessage.LayoutComponent = InventoryLayout;
		UGameplayMessageSubsystem::Get(World).BroadcastMessage(
			RpgGameplayTags::Rpg_InventoryLayout_Message_Changed,
			LayoutMessage);
	}

	void BroadcastEquipmentChanged(
		UWorld* World,
		ARpgInventoryAutomationTestPlayerController* Controller)
	{
		if (!World || !Controller)
		{
			return;
		}

		FRpgEquipmentLoadoutSlotsChangedMessage EquipmentMessage;
		EquipmentMessage.Owner = Controller;
		UGameplayMessageSubsystem::Get(World).BroadcastMessage(
			RpgGameplayTags::
				Rpg_EquipmentLoadout_Message_SlotsChanged,
			EquipmentMessage);
	}

	void BroadcastActionBarChanged(
		UWorld* World,
		ARpgInventoryAutomationTestPlayerController* Controller,
		URpgActionBarComponent* ActionBar)
	{
		if (!World || !Controller || !ActionBar)
		{
			return;
		}

		FRpgActionBarSlotsChangedMessage ActionBarMessage;
		ActionBarMessage.Owner = Controller;
		ActionBarMessage.ActionBarComponent = ActionBar;
		UGameplayMessageSubsystem::Get(World).BroadcastMessage(
			RpgGameplayTags::Rpg_ActionBar_Message_SlotsChanged,
			ActionBarMessage);
	}

	void BroadcastAllPlayerPresentationMessages(
		UWorld* World,
		ARpgInventoryAutomationTestPlayerController* Controller,
		URpgInventoryManagerComponent* Inventory,
		URpgPlayerInventoryLayoutComponent* InventoryLayout,
		URpgActionBarComponent* ActionBar)
	{
		BroadcastInventoryChanged(World, Inventory);
		BroadcastLayoutChanged(
			World,
			Controller,
			InventoryLayout);
		BroadcastEquipmentChanged(World, Controller);
		BroadcastActionBarChanged(
			World,
			Controller,
			ActionBar);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRpgPlayerStorageInventoryLifecycleIntegrationTest,
	"SurvivalRpg.Inventory.UI.PlayerStorageLifecycleIntegration",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FRpgPlayerStorageInventoryLifecycleIntegrationTest::RunTest(
	const FString& Parameters)
{
	using namespace RpgPlayerStorageInventoryLifecycleIntegrationTests;

	FScopedPlayerWidgetWorld TestWorld;
	if (!TestTrue(
			TEXT(
				"The Player/Storage lifecycle fixture owns a real controller, player state, and canonical inventory"),
			TestWorld.IsValid()))
	{
		return false;
	}

	ARpgInventoryAutomationTestPlayerController* Controller =
		TestWorld.GetController();
	ARpgInventoryAutomationTestPlayerState* PlayerState =
		TestWorld.GetPlayerState();
	URpgInventoryManagerComponent* CanonicalInventory =
		TestWorld.GetCanonicalPlayerInventory();
	URpgPlayerInventoryLayoutComponent* InventoryLayout =
		Controller->GetPlayerInventoryLayoutComponent();
	URpgEquipmentLoadoutComponent* EquipmentLoadout =
		Controller->GetEquipmentLoadoutComponent();
	URpgActionBarComponent* ActionBar =
		Controller->GetActionBarComponent();
	URpgInventoryManagerComponent* SecondaryInventory =
		TestWorld.CreateSecondaryInventory(
			TEXT("PlayerStorageLifecycleSecondaryOwner"));
	if (!TestEqual(
			TEXT("The controller resolves the fixture PlayerState"),
			Controller->GetRpgPlayerState(),
			static_cast<ARpgPlayerState*>(PlayerState)) ||
		!TestNotNull(
			TEXT("The canonical inventory is the PlayerState component"),
			CanonicalInventory) ||
		!TestNotNull(
			TEXT("The controller exposes its real layout component"),
			InventoryLayout) ||
		!TestNotNull(
			TEXT("The controller layout resolves fixture PawnData"),
			InventoryLayout
				? InventoryLayout->GetLayoutDefinition()
				: nullptr) ||
		!TestNotNull(
			TEXT("The controller exposes its real equipment component"),
			EquipmentLoadout) ||
		!TestNotNull(
			TEXT("The controller exposes its real actionbar component"),
			ActionBar) ||
		!TestNotNull(
			TEXT("The independent secondary inventory exists"),
			SecondaryInventory))
	{
		return false;
	}

	UClass* PlayerWidgetClass =
		LoadClass<URpgPlayerInventoryWidget>(
			nullptr,
			TEXT(
				"/Game/SurvivalRpg/Inventory/UI/"
				"CUI_PlayerInventory.CUI_PlayerInventory_C"));
	UClass* StorageWidgetClass =
		LoadClass<URpgStorageInventoryWidget>(
			nullptr,
			TEXT(
				"/Game/SurvivalRpg/UI/"
				"CUI_StorageSpatial.CUI_StorageSpatial_C"));
	if (!TestNotNull(
			TEXT("The authored Player Inventory screen loads"),
			PlayerWidgetClass) ||
		!TestNotNull(
			TEXT("The authored Storage screen loads"),
			StorageWidgetClass))
	{
		return false;
	}

	URpgPlayerInventoryWidget* PlayerWidget =
		CreateWidget<URpgPlayerInventoryWidget>(
			Controller,
			PlayerWidgetClass);
	URpgStorageInventoryWidget* StorageWidget =
		CreateWidget<URpgStorageInventoryWidget>(
			Controller,
			StorageWidgetClass);
	if (!TestNotNull(
			TEXT("The Player screen initializes with its owning controller"),
			PlayerWidget) ||
		!TestNotNull(
			TEXT("The Storage screen initializes with the same controller"),
			StorageWidget))
	{
		return false;
	}

	ICommonInputModule::GetSettings().LoadData();
	TSharedPtr<SWidget> PlayerSlate = PlayerWidget->TakeWidget();
	TSharedPtr<SWidget> StorageSlate = StorageWidget->TakeWidget();
	if (!TestTrue(
			TEXT("The Player screen constructs its Slate representation"),
			PlayerSlate.IsValid()) ||
		!TestTrue(
			TEXT("The Storage screen constructs its Slate representation"),
			StorageSlate.IsValid()) ||
		!TestEqual(
			TEXT("The Player screen retains the real owning controller"),
			PlayerWidget->GetOwningPlayer(),
			static_cast<APlayerController*>(Controller)) ||
		!TestEqual(
			TEXT("The Storage screen retains the same owning controller"),
			StorageWidget->GetOwningPlayer(),
			static_cast<APlayerController*>(Controller)))
	{
		return false;
	}

	URpgInventoryScreenPayload* InitialStoragePayload =
		MakeStoragePayload(
			StorageWidget,
			CanonicalInventory,
			SecondaryInventory);
	if (!TestNotNull(
			TEXT("The initial canonical Storage payload exists"),
			InitialStoragePayload))
	{
		return false;
	}
	IRpgUIScreenPayloadReceiver::Execute_ReceiveScreenPayload(
		StorageWidget,
		InitialStoragePayload);

	PlayerWidget->ActivateWidget();
	StorageWidget->ActivateWidget();
	URpgPlayerInventoryViewModel* PlayerViewModel =
		PlayerWidget->GetPlayerInventoryViewModel();
	URpgPlayerInventoryViewModel* StorageViewModel =
		StorageWidget->GetStoragePlayerInventoryViewModel();
	if (!TestTrue(
			TEXT("The Player screen activates"),
			PlayerWidget->IsActivated()) ||
		!TestTrue(
			TEXT("The Storage screen activates"),
			StorageWidget->IsActivated()) ||
		!TestNotNull(
			TEXT("The Player screen owns its aggregate VM"),
			PlayerViewModel) ||
		!TestNotNull(
			TEXT("The Storage screen owns its aggregate player-side VM"),
			StorageViewModel) ||
		!TestNotEqual(
			TEXT("Player and Storage keep distinct screen-owned aggregate VMs"),
			PlayerViewModel,
			StorageViewModel) ||
		!TestEqual(
			TEXT("Storage accepts the canonical PlayerState inventory as Primary"),
			StorageWidget->GetInventoryScreenPayload(),
			InitialStoragePayload) ||
		!TestEqual(
			TEXT("Storage's coordinator resolves the same canonical player inventory"),
			StorageWidget->GetInventoryDragDropCoordinator()
				? StorageWidget->GetInventoryDragDropCoordinator()->
					GetPlayerInventory()
				: nullptr,
			CanonicalInventory))
	{
		return false;
	}

	auto CountValidListenerHandles =
		[](const URpgPlayerInventoryViewModel* ViewModel)
		{
			return ViewModel
				? static_cast<int32>(
					ViewModel->InventoryChangedHandle.IsValid()) +
					static_cast<int32>(
						ViewModel->LayoutChangedHandle.IsValid()) +
					static_cast<int32>(
						ViewModel->EquipmentChangedHandle.IsValid()) +
					static_cast<int32>(
						ViewModel->ActionBarChangedHandle.IsValid())
				: 0;
		};
	auto VerifyBoundSources =
		[this,
		 CanonicalInventory,
		 InventoryLayout,
		 EquipmentLoadout,
		 ActionBar,
		 &CountValidListenerHandles](
			const TCHAR* Phase,
			const URpgPlayerInventoryViewModel* ViewModel)
		{
			const FString Prefix(Phase);
			return TestEqual(
					*(Prefix + TEXT(": observes the canonical PlayerState inventory")),
					ViewModel
						? ViewModel->ObservedPlayerInventory.Get()
						: nullptr,
					CanonicalInventory) &&
				TestEqual(
					*(Prefix + TEXT(": observes the controller layout")),
					ViewModel
						? ViewModel->ObservedInventoryLayout.Get()
						: nullptr,
					InventoryLayout) &&
				TestEqual(
					*(Prefix + TEXT(": observes the controller equipment loadout")),
					ViewModel
						? ViewModel->ObservedEquipmentLoadout.Get()
						: nullptr,
					EquipmentLoadout) &&
				TestEqual(
					*(Prefix + TEXT(": observes the controller actionbar")),
					ViewModel
						? ViewModel->ObservedActionBar.Get()
						: nullptr,
					ActionBar) &&
				TestEqual(
					*(Prefix + TEXT(": owns exactly four active message handles")),
					CountValidListenerHandles(ViewModel),
					4) &&
				TestEqual(
					*(Prefix + TEXT(": has no queued refresh immediately after bind")),
					ViewModel
						? ViewModel->PendingRefreshDomains
						: static_cast<uint8>(MAX_uint8),
					static_cast<uint8>(0));
		};
	auto VerifyUnboundSources =
		[this, &CountValidListenerHandles](
			const TCHAR* Phase,
			const URpgPlayerInventoryViewModel* ViewModel)
		{
			const FString Prefix(Phase);
			return TestNull(
					*(Prefix + TEXT(": releases the player inventory")),
					ViewModel
						? ViewModel->ObservedPlayerInventory.Get()
						: nullptr) &&
				TestNull(
					*(Prefix + TEXT(": releases the controller layout")),
					ViewModel
						? ViewModel->ObservedInventoryLayout.Get()
						: nullptr) &&
				TestNull(
					*(Prefix + TEXT(": releases the equipment loadout")),
					ViewModel
						? ViewModel->ObservedEquipmentLoadout.Get()
						: nullptr) &&
				TestNull(
					*(Prefix + TEXT(": releases the actionbar")),
					ViewModel
						? ViewModel->ObservedActionBar.Get()
						: nullptr) &&
				TestEqual(
					*(Prefix + TEXT(": invalidates every message handle")),
					CountValidListenerHandles(ViewModel),
					0) &&
				TestEqual(
					*(Prefix + TEXT(": clears every pending refresh domain")),
					ViewModel
						? ViewModel->PendingRefreshDomains
						: static_cast<uint8>(MAX_uint8),
					static_cast<uint8>(0));
		};

	if (!VerifyBoundSources(
			TEXT("Active Player screen"),
			PlayerViewModel) ||
		!VerifyBoundSources(
			TEXT("Active Storage screen"),
			StorageViewModel))
	{
		return false;
	}

	FViewModelDelegateCounters PlayerCounters;
	FViewModelDelegateCounters StorageCounters;
	if (!TestTrue(
			TEXT("Player VM delegate counters bind"),
			PlayerCounters.Bind(PlayerViewModel)) ||
		!TestTrue(
			TEXT("Storage VM delegate counters bind"),
			StorageCounters.Bind(StorageViewModel)))
	{
		return false;
	}

	auto VerifyCounterState =
		[this](
			const TCHAR* Phase,
			const FViewModelDelegateCounters& Counters,
			const int32 ExpectedGearCount,
			const int32 ExpectedGroupCount,
			const int32 ExpectedActionBarCount)
		{
			const FString Prefix(Phase);
			bool bValid = true;
			bValid &= TestEqual(
				*(Prefix + TEXT(": Gear refresh count")),
				Counters.GetGearCount(),
				ExpectedGearCount);
			bValid &= TestEqual(
				*(Prefix + TEXT(": group refresh count")),
				Counters.GetGroupCount(),
				ExpectedGroupCount);
			bValid &= TestEqual(
				*(Prefix + TEXT(": actionbar refresh count")),
				Counters.GetActionBarCount(),
				ExpectedActionBarCount);
			return bValid;
		};
	auto VerifyPendingDomains =
		[this, PlayerViewModel, StorageViewModel](
			const TCHAR* Phase,
			const uint8 ExpectedDomains)
		{
			const FString Prefix(Phase);
			bool bValid = true;
			bValid &= TestEqual(
				*(Prefix + TEXT(": Player pending refresh domains")),
				PlayerViewModel->PendingRefreshDomains,
				ExpectedDomains);
			bValid &= TestEqual(
				*(Prefix + TEXT(": Storage pending refresh domains")),
				StorageViewModel->PendingRefreshDomains,
				ExpectedDomains);
			return bValid;
		};

	TestWorld.PrimeTimerManager();
	PlayerCounters.Reset();
	StorageCounters.Reset();
	if (!TestNotNull(
			TEXT(
				"The canonical PlayerState inventory accepts a real item grant"),
			CanonicalInventory->GrantItemDefinition(
				URpgInventoryAutomationTestUnitItemDefinition::
					StaticClass(),
				1)))
	{
		return false;
	}
	VerifyCounterState(
		TEXT("Real inventory producer before tick, Player"),
		PlayerCounters,
		0,
		0,
		0);
	VerifyCounterState(
		TEXT("Real inventory producer before tick, Storage"),
		StorageCounters,
		0,
		0,
		0);
	VerifyPendingDomains(
		TEXT("Real inventory producer"),
		InventoryAndLayoutRefreshDomains);

	TestWorld.AdvanceTimerFrame();
	VerifyCounterState(
		TEXT("Real inventory producer after tick, Player"),
		PlayerCounters,
		0,
		1,
		1);
	VerifyCounterState(
		TEXT("Real inventory producer after tick, Storage"),
		StorageCounters,
		0,
		1,
		1);

	PlayerCounters.Reset();
	StorageCounters.Reset();
	BroadcastLayoutChanged(
		TestWorld.GetWorld(),
		Controller,
		InventoryLayout);
	VerifyPendingDomains(
		TEXT("Isolated layout listener"),
		InventoryAndLayoutRefreshDomains);
	TestWorld.AdvanceTimerFrame();
	VerifyCounterState(
		TEXT("Isolated layout listener, Player"),
		PlayerCounters,
		0,
		1,
		1);
	VerifyCounterState(
		TEXT("Isolated layout listener, Storage"),
		StorageCounters,
		0,
		1,
		1);

	PlayerCounters.Reset();
	StorageCounters.Reset();
	BroadcastActionBarChanged(
		TestWorld.GetWorld(),
		Controller,
		ActionBar);
	VerifyPendingDomains(
		TEXT("Isolated actionbar listener"),
		ActionBarRefreshDomain);
	TestWorld.AdvanceTimerFrame();
	VerifyCounterState(
		TEXT("Isolated actionbar listener, Player"),
		PlayerCounters,
		0,
		0,
		1);
	VerifyCounterState(
		TEXT("Isolated actionbar listener, Storage"),
		StorageCounters,
		0,
		0,
		1);

	PlayerCounters.Reset();
	StorageCounters.Reset();
	BroadcastEquipmentChanged(
		TestWorld.GetWorld(),
		Controller);
	VerifyPendingDomains(
		TEXT("Isolated equipment listener"),
		AllRefreshDomains);
	TestWorld.AdvanceTimerFrame();
	VerifyCounterState(
		TEXT("Isolated equipment listener, Player"),
		PlayerCounters,
		1,
		1,
		1);
	VerifyCounterState(
		TEXT("Isolated equipment listener, Storage"),
		StorageCounters,
		1,
		1,
		1);

	PlayerCounters.Reset();
	StorageCounters.Reset();
	BroadcastAllPlayerPresentationMessages(
		TestWorld.GetWorld(),
		Controller,
		CanonicalInventory,
		InventoryLayout,
		ActionBar);
	TestTrue(
		TEXT("The mixed listener burst queues Player before deactivation"),
		PlayerViewModel->PendingRefreshDomains != 0);
	TestTrue(
		TEXT("The mixed listener burst queues Storage"),
		StorageViewModel->PendingRefreshDomains != 0);

	PlayerWidget->DeactivateWidget();
	PlayerCounters.Reset();
	if (!TestFalse(
			TEXT("The Player screen deactivates"),
			PlayerWidget->IsActivated()) ||
		!VerifyUnboundSources(
			TEXT("Deactivated Player screen"),
			PlayerViewModel) ||
		!TestEqual(
			TEXT("Storage remains fully registered while its screen stays active"),
			CountValidListenerHandles(StorageViewModel),
			4))
	{
		return false;
	}

	TestWorld.AdvanceTimerFrame();
	TestEqual(
		TEXT("Player's pre-deactivation Gear refresh is cancelled"),
		PlayerCounters.GetGearCount(),
		0);
	TestEqual(
		TEXT("Player's pre-deactivation group refresh is cancelled"),
		PlayerCounters.GetGroupCount(),
		0);
	TestEqual(
		TEXT("Player's pre-deactivation actionbar refresh is cancelled"),
		PlayerCounters.GetActionBarCount(),
		0);
	TestEqual(
		TEXT("Active Storage still consumes the equipment domain once"),
		StorageCounters.GetGearCount(),
		1);
	TestEqual(
		TEXT("Active Storage still consumes the group domains once"),
		StorageCounters.GetGroupCount(),
		1);
	TestEqual(
		TEXT("Active Storage still consumes the actionbar domains once"),
		StorageCounters.GetActionBarCount(),
		1);

	StorageCounters.Reset();
	BroadcastAllPlayerPresentationMessages(
		TestWorld.GetWorld(),
		Controller,
		CanonicalInventory,
		InventoryLayout,
		ActionBar);
	TestTrue(
		TEXT("A second mixed burst queues Storage before its deactivation"),
		StorageViewModel->PendingRefreshDomains != 0);
	StorageWidget->DeactivateWidget();
	StorageCounters.Reset();
	if (!TestFalse(
			TEXT("The Storage screen deactivates"),
			StorageWidget->IsActivated()) ||
		!VerifyUnboundSources(
			TEXT("Deactivated Storage screen"),
			StorageViewModel))
	{
		return false;
	}

	TestWorld.AdvanceTimerFrame();
	TestEqual(
		TEXT("Storage's pre-deactivation Gear refresh is cancelled"),
		StorageCounters.GetGearCount(),
		0);
	TestEqual(
		TEXT("Storage's pre-deactivation group refresh is cancelled"),
		StorageCounters.GetGroupCount(),
		0);
	TestEqual(
		TEXT("Storage's pre-deactivation actionbar refresh is cancelled"),
		StorageCounters.GetActionBarCount(),
		0);

	BroadcastAllPlayerPresentationMessages(
		TestWorld.GetWorld(),
		Controller,
		CanonicalInventory,
		InventoryLayout,
		ActionBar);
	TestWorld.AdvanceTimerFrame();
	TestEqual(
		TEXT("No Player listener survives while both screens are inactive"),
		PlayerCounters.GetGearCount() +
			PlayerCounters.GetGroupCount() +
			PlayerCounters.GetActionBarCount(),
		0);
	TestEqual(
		TEXT("No Storage listener survives while both screens are inactive"),
		StorageCounters.GetGearCount() +
			StorageCounters.GetGroupCount() +
			StorageCounters.GetActionBarCount(),
		0);

	URpgInventoryScreenPayload* ReactivationPayload =
		MakeStoragePayload(
			StorageWidget,
			CanonicalInventory,
			SecondaryInventory);
	if (!TestNotNull(
			TEXT("A fresh canonical payload exists for Storage reactivation"),
			ReactivationPayload))
	{
		return false;
	}
	IRpgUIScreenPayloadReceiver::Execute_ReceiveScreenPayload(
		StorageWidget,
		ReactivationPayload);
	PlayerWidget->ActivateWidget();
	StorageWidget->ActivateWidget();
	if (!TestEqual(
			TEXT("Player pooling retains the same aggregate VM"),
			PlayerWidget->GetPlayerInventoryViewModel(),
			PlayerViewModel) ||
		!TestEqual(
			TEXT("Storage pooling retains the same aggregate VM"),
			StorageWidget->GetStoragePlayerInventoryViewModel(),
			StorageViewModel) ||
		!VerifyBoundSources(
			TEXT("Reactivated Player screen"),
			PlayerViewModel) ||
		!VerifyBoundSources(
			TEXT("Reactivated Storage screen"),
			StorageViewModel))
	{
		return false;
	}

	TestWorld.AdvanceTimerFrame();
	PlayerCounters.Reset();
	StorageCounters.Reset();
	BroadcastAllPlayerPresentationMessages(
		TestWorld.GetWorld(),
		Controller,
		CanonicalInventory,
		InventoryLayout,
		ActionBar);
	TestWorld.AdvanceTimerFrame();
	TestEqual(
		TEXT("Reactivated Player receives Gear exactly once"),
		PlayerCounters.GetGearCount(),
		1);
	TestEqual(
		TEXT("Reactivated Player receives groups exactly once"),
		PlayerCounters.GetGroupCount(),
		1);
	TestEqual(
		TEXT("Reactivated Player receives actionbar exactly once"),
		PlayerCounters.GetActionBarCount(),
		1);
	TestEqual(
		TEXT("Reactivated Storage receives Gear exactly once"),
		StorageCounters.GetGearCount(),
		1);
	TestEqual(
		TEXT("Reactivated Storage receives groups exactly once"),
		StorageCounters.GetGroupCount(),
		1);
	TestEqual(
		TEXT("Reactivated Storage receives actionbar exactly once"),
		StorageCounters.GetActionBarCount(),
		1);

	PlayerWidget->DeactivateWidget();
	StorageWidget->DeactivateWidget();
	const bool bFinalPlayerCleanup =
		VerifyUnboundSources(
			TEXT("Final Player cleanup"),
			PlayerViewModel);
	const bool bFinalStorageCleanup =
		VerifyUnboundSources(
			TEXT("Final Storage cleanup"),
			StorageViewModel);
	PlayerCounters.Unbind(PlayerViewModel);
	StorageCounters.Unbind(StorageViewModel);
	PlayerSlate.Reset();
	StorageSlate.Reset();
	return bFinalPlayerCleanup && bFinalStorageCleanup;
}

#endif
