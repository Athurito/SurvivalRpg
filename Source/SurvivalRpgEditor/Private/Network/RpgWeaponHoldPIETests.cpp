#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"
#include "Network/RpgCombatNetworkTestTypes.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "SurvivalRpg/Core/Character/RpgCharacter.h"
#include "SurvivalRpg/Core/Game/Experience/RpgExperienceDefinition.h"
#include "SurvivalRpg/Core/Game/Experience/RpgExperienceManagerComponent.h"
#include "SurvivalRpg/Core/Game/RpgGameModeBase.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/Development/RpgDeveloperSettings.h"
#include "SurvivalRpg/Equipment/RpgEquipmentManagerComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryUiActionComponent.h"

/**
 * CMB-02 seam: the equipment manager links the hold layer of every item in hand on the gameplay mesh of each machine.
 *
 * The Mantle Experience spawns the CMC pawn armed with the sword and shield, whose definitions carry hold presentations
 * for the Manny skeleton. Only the binding is asserted (a linked instance per occupied hand, none for an empty hand);
 * the poses themselves stay designer content.
 */
namespace RpgWeaponHoldPIETests
{
	constexpr TCHAR GameModePath[] = TEXT("/Game/SurvivalRpg/Maps/Test/GaspMantle/BP_Rpg_GaspMantleTestGameMode.BP_Rpg_GaspMantleTestGameMode_C");
	constexpr TCHAR SwordItemPath[] = TEXT("/GF_Combat_Core/Items/Weapons/ID_BasicSword.ID_BasicSword_C");

	FPrimaryAssetId ExperienceId()
	{
		return FPrimaryAssetId(URpgExperienceDefinition::StaticClass()->GetFName(), TEXT("RpgGaspMantleExperience"));
	}
	FTimespan Timeout() { return FTimespan::FromSeconds(90.0); }

	/** Installed before CQTest creates any PIE world; keeps the session away from real world saves. */
	class FScopedSaveIsolation final
	{
	public:
		~FScopedSaveIsolation() { FGameModeEvents::OnGameModeInitializedEvent().Remove(Handle); }
		void Start()
		{
			Prefix = TEXT("SurvivalRpg_WeaponHoldAutomation_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
			Handle = FGameModeEvents::OnGameModeInitializedEvent().AddRaw(this, &FScopedSaveIsolation::OnInitialized);
		}
		bool IsIsolated(UWorld* World) const
		{
			if (!World || World->WorldType != EWorldType::PIE) return false;
			if (World->GetNetMode() == NM_Client) return World->GetAuthGameMode() == nullptr;
			const ARpgGameModeBase* GameMode = World->GetAuthGameMode<ARpgGameModeBase>();
			return GameMode && !GameMode->bEnableDiskPersistence && GameMode->WorldSaveSlotName.StartsWith(Prefix);
		}
	private:
		void OnInitialized(AGameModeBase* Initialized)
		{
			ARpgGameModeBase* GameMode = Cast<ARpgGameModeBase>(Initialized);
			if (!GameMode || !GameMode->GetWorld() || GameMode->GetWorld()->WorldType != EWorldType::PIE) return;
			GameMode->bEnableDiskPersistence = false;
			GameMode->WorldSaveSlotName = FString::Printf(TEXT("%s_%u"), *Prefix, GameMode->GetUniqueID());
			GameMode->WorldSaveBackupSlotName = GameMode->WorldSaveSlotName + TEXT("_Backup");
			GameMode->WorldSaveRecoverySlotName = GameMode->WorldSaveSlotName + TEXT("_Recovery");
			GameMode->OfflineProfileKey = Prefix;
		}
		FString Prefix;
		FDelegateHandle Handle;
	};

	struct FState : FBasePIENetworkComponentState
	{
		TWeakObjectPtr<UAnimInstance> MainBeforeReinit;
	};

	ARpgCharacter* LocalCharacter(UWorld* World)
	{
		const APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
		return Controller ? Cast<ARpgCharacter>(Controller->GetPawn()) : nullptr;
	}
	ARpgCharacter* PlayerCharacter(UWorld* World, int32 PlayerId)
	{
		const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
		if (!GameState || PlayerId == INDEX_NONE) return nullptr;
		for (APlayerState* PlayerState : GameState->PlayerArray)
		{
			if (PlayerState && PlayerState->GetPlayerId() == PlayerId) return PlayerState->GetPawn<ARpgCharacter>();
		}
		return nullptr;
	}
	bool Ready(UWorld* World, const ARpgCharacter* Character)
	{
		const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
		const URpgExperienceManagerComponent* Manager = GameState ? GameState->FindComponentByClass<URpgExperienceManagerComponent>() : nullptr;
		return World && World->GetNetDriver() && Manager && Manager->IsExperienceLoaded()
			&& Manager->GetCurrentExperienceChecked()->GetPrimaryAssetId() == ExperienceId()
			&& Character && Character->GetPlayerState() && Character->GetEquipmentManagerComponent()
			&& Character->GetMesh() && Character->GetMesh()->GetAnimInstance();
	}
	bool Occupied(const ARpgCharacter* Character, ERpgEquipmentSlot Slot)
	{
		return Character->GetEquipmentManagerComponent()->GetEquipmentInstanceInSlot(Slot) != nullptr;
	}
	/** The hold layer for Slot is an instance linked on the gameplay mesh's current main AnimInstance. */
	bool Linked(const ARpgCharacter* Character, ERpgEquipmentSlot Slot)
	{
		const UAnimInstance* Layer = Character->GetEquipmentManagerComponent()->GetWeaponHoldLayerInstance(Slot);
		const UAnimInstance* Main = Character->GetMesh()->GetAnimInstance();
		return Layer && Main && Layer != Main && Main->GetLinkedAnimLayerInstanceByClass(Layer->GetClass()) == Layer;
	}
	/** Each hand shows a linked hold layer exactly when it is occupied; MainHandExpected pins the main hand's state. */
	bool HoldMatches(UWorld* World, const ARpgCharacter* Character, bool bMainHandExpected)
	{
		if (!Ready(World, Character) || Occupied(Character, ERpgEquipmentSlot::MainHand) != bMainHandExpected) return false;
		for (const ERpgEquipmentSlot Slot : { ERpgEquipmentSlot::MainHand, ERpgEquipmentSlot::OffHand })
		{
			if (Occupied(Character, Slot) != Linked(Character, Slot)
				|| (!Occupied(Character, Slot) && Character->GetEquipmentManagerComponent()->GetWeaponHoldLayerInstance(Slot)))
			{
				return false;
			}
		}
		return true;
	}
	bool RefuseRunningPIE(FAutomationTestBase& Test)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && IsValid(Context.World()))
			{
				Test.AddError(TEXT("Weapon hold automation refuses to interrupt an existing PIE session."));
				return true;
			}
		}
		return false;
	}
	/** Sends one equipment intent for the sword from the local controller exactly as the inventory UI does. */
	void RequestSwordInHand(UWorld* World, FAutomationTestBase& Test)
	{
		APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
		URpgInventoryUiActionComponent* Actions = Controller ? Controller->FindComponentByClass<URpgInventoryUiActionComponent>() : nullptr;
		const ARpgPlayerState* PlayerState = Controller ? Controller->GetPlayerState<ARpgPlayerState>() : nullptr;
		URpgInventoryManagerComponent* Inventory = PlayerState ? PlayerState->GetInventoryManagerComponent() : nullptr;
		const FRpgInventoryEntryView* Sword = nullptr;
		TArray<FRpgInventoryEntryView> Entries = Inventory ? Inventory->GetAllEntries() : TArray<FRpgInventoryEntryView>();
		for (const FRpgInventoryEntryView& Entry : Entries)
		{
			if (Entry.Instance && Entry.Instance->GetItemDef() && Entry.Instance->GetItemDef()->GetPathName() == SwordItemPath)
			{
				Sword = &Entry;
				break;
			}
		}
		if (!Test.TestTrue(TEXT("The owner's sword waits in the inventory"), Actions && Sword))
		{
			return;
		}
		FRpgInventoryEquipmentIntent Intent;
		Intent.EnsureRequestId();
		Intent.ItemId = Sword->ItemId;
		Intent.ExpectedEntryId = Sword->EntryId;
		Intent.ExpectedSourcePlacement = Sword->Placement;
		Intent.ExpectedQuantity = Sword->StackCount;
		Intent.Operation = ERpgInventoryEquipmentIntentOperation::EquipDefaultAndActivate;
		Actions->RequestApplyInventoryEquipmentIntent(Inventory, Intent);
	}
}

#if ENABLE_PIE_NETWORK_TEST

NETWORK_TEST_CLASS(WeaponHoldListenServerPIE, "SurvivalRpg.Combat.WeaponHold")
{
	using FState = RpgWeaponHoldPIETests::FState;
	// Destruction is reversed: network cleanup finishes while save isolation is still installed.
	RpgWeaponHoldPIETests::FScopedSaveIsolation Isolation;
	FRpgScopedNetworkTestGround Ground;
	FPIENetworkComponent<FState> Network{TestRunner, TestCommandBuilder, bInitializing};
	FPrimaryAssetId OriginalExperience;
	bool bConfigured = false;
	int32 SubjectId = INDEX_NONE;

	BEFORE_EACH()
	{
		using namespace RpgWeaponHoldPIETests;
		if (RefuseRunningPIE(*TestRunner)) return;
		UClass* GameModeClass = LoadClass<ARpgGameModeBase>(nullptr, GameModePath);
		ASSERT_THAT(IsNotNull(GameModeClass));
		if (!GameModeClass) return;
		Isolation.Start();
		Ground.Start();
		OriginalExperience = GetDefault<URpgDeveloperSettings>()->ExperienceOverride;
		GetMutableDefault<URpgDeveloperSettings>()->ExperienceOverride = ExperienceId();
		bConfigured = true;
		FNetworkComponentBuilder<FState>().WithClients(1).AsListenServer()
			.WithGameInstanceClass(FSoftClassPath(TEXT("/Game/SurvivalRpg/Core/Game/BP_Rpg_GameInstance.BP_Rpg_GameInstance_C")))
			.WithGameMode(GameModeClass).Build(Network);
	}
	AFTER_EACH()
	{
		if (bConfigured) GetMutableDefault<URpgDeveloperSettings>()->ExperienceOverride = OriginalExperience;
	}

	TEST_METHOD(HeldItemsLinkHoldLayersOnEveryRoleAndFollowTheHands)
	{
		using namespace RpgWeaponHoldPIETests;
		if (!bConfigured) return;
		Network.ThenServer(TEXT("Server persistence was isolated before InitGame"), [this](FState& State)
			{ ASSERT_THAT(IsTrue(Isolation.IsIsolated(State.World))); })
			.UntilClient(TEXT("The armed owner links a hold layer for both occupied hands"), 0, [](FState& State)
			{
				ARpgCharacter* Character = LocalCharacter(State.World);
				return HoldMatches(State.World, Character, true) && Occupied(Character, ERpgEquipmentSlot::OffHand);
			}, Timeout())
			.ThenClient(TEXT("Capture the autonomous owner's identity"), 0, [this](FState& State)
			{
				ARpgCharacter* Character = LocalCharacter(State.World);
				ASSERT_THAT(IsTrue(Character->GetLocalRole() == ROLE_AutonomousProxy));
				SubjectId = Character->GetPlayerState()->GetPlayerId();
			})
			.UntilServer(TEXT("The listen server links the hold layers of the remote pawn and its own pawn"), [this](FState& State)
			{
				return HoldMatches(State.World, PlayerCharacter(State.World, SubjectId), true)
					&& HoldMatches(State.World, LocalCharacter(State.World), true);
			}, Timeout())
			.ThenClientJoins()
			.UntilClient(TEXT("A late joiner links the hold layers of the remote simulated proxy"), 1, [this](FState& State)
			{
				ARpgCharacter* Remote = PlayerCharacter(State.World, SubjectId);
				return Isolation.IsIsolated(State.World) && Remote && Remote->GetLocalRole() == ROLE_SimulatedProxy
					&& HoldMatches(State.World, Remote, true) && Occupied(Remote, ERpgEquipmentSlot::OffHand);
			}, Timeout())
			.ThenClient(TEXT("Owner holsters both hands through the inventory UI action"), 0, [this](FState& State)
			{
				APlayerController* Controller = State.World->GetFirstPlayerController();
				URpgInventoryUiActionComponent* Actions = Controller ? Controller->FindComponentByClass<URpgInventoryUiActionComponent>() : nullptr;
				ASSERT_THAT(IsNotNull(Actions));
				if (Actions) Actions->RequestClearActiveHands();
			})
			.UntilServer(TEXT("Server empties both hands and unlinks their hold layers"), [this](FState& State)
			{
				ARpgCharacter* Remote = PlayerCharacter(State.World, SubjectId);
				return HoldMatches(State.World, Remote, false) && !Occupied(Remote, ERpgEquipmentSlot::OffHand);
			}, Timeout())
			.UntilClient(TEXT("Owner unlinks both hold layers"), 0, [](FState& State)
			{
				ARpgCharacter* Character = LocalCharacter(State.World);
				return HoldMatches(State.World, Character, false) && !Occupied(Character, ERpgEquipmentSlot::OffHand);
			}, Timeout())
			.UntilClient(TEXT("Late joiner unlinks both hold layers of the proxy"), 1, [this](FState& State)
			{
				ARpgCharacter* Remote = PlayerCharacter(State.World, SubjectId);
				return HoldMatches(State.World, Remote, false) && !Occupied(Remote, ERpgEquipmentSlot::OffHand);
			}, Timeout())
			.ThenClient(TEXT("Owner takes the sword back into the main hand"), 0, [this](FState& State)
				{ RequestSwordInHand(State.World, *TestRunner); })
			.UntilServer(TEXT("Server links the main-hand hold layer again"), [this](FState& State)
				{ return HoldMatches(State.World, PlayerCharacter(State.World, SubjectId), true); }, Timeout())
			.UntilClient(TEXT("Owner links the main-hand hold layer again"), 0, [](FState& State)
				{ return HoldMatches(State.World, LocalCharacter(State.World), true); }, Timeout())
			.UntilClient(TEXT("Late joiner links the main-hand hold layer of the proxy again"), 1, [this](FState& State)
				{ return HoldMatches(State.World, PlayerCharacter(State.World, SubjectId), true); }, Timeout())
			.ThenClient(TEXT("Replace the owner's main AnimInstance while no montage plays"), 0, [this](FState& State)
			{
				ARpgCharacter* Character = LocalCharacter(State.World);
				USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
				UClass* MainClass = Mesh ? Mesh->GetAnimClass() : nullptr;
				ASSERT_THAT(IsNotNull(MainClass));
				if (!MainClass) return;
				State.MainBeforeReinit = Mesh->GetAnimInstance();
				Mesh->SetAnimInstanceClass(nullptr);
				Mesh->SetAnimInstanceClass(MainClass);
			})
			.UntilClient(TEXT("The new main AnimInstance relinks the main-hand hold layer"), 0, [](FState& State)
			{
				ARpgCharacter* Character = LocalCharacter(State.World);
				return HoldMatches(State.World, Character, true) && Character->GetMesh()->GetAnimInstance() != State.MainBeforeReinit.Get();
			}, Timeout())
			.ThenServer(TEXT("Persistence remains disabled through test completion"), [this](FState& State)
				{ ASSERT_THAT(IsTrue(Isolation.IsIsolated(State.World))); });
	}
};

#endif // ENABLE_PIE_NETWORK_TEST

#endif // WITH_DEV_AUTOMATION_TESTS
