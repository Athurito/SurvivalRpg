// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"
#include "Network/RpgCombatNetworkTestTypes.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Core/Character/RpgCharacter.h"
#include "SurvivalRpg/Core/Character/RpgDownedComponent.h"
#include "SurvivalRpg/Core/Character/RpgHealthComponent.h"
#include "SurvivalRpg/Core/Game/Experience/RpgExperienceActionSet.h"
#include "SurvivalRpg/Core/Game/Experience/RpgExperienceDefinition.h"
#include "SurvivalRpg/Core/Game/Experience/RpgExperienceManagerComponent.h"
#include "SurvivalRpg/Core/Game/RpgGameModeBase.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/Cosmetics/RpgPawnComponent_CharacterParts.h"
#include "SurvivalRpg/Development/RpgDeveloperSettings.h"
#include "SurvivalRpg/Equipment/RpgEquipmentDefinition.h"
#include "SurvivalRpg/Equipment/RpgEquipmentManagerComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryUiActionComponent.h"

/**
 * GASP-16 content contract for Lyra-style character parts in a real network session.
 *
 * The Mantle Experience composes the Paladin action set (GASP-17), which gives every pawn the MetaHuman Paladin body
 * part through the controller parts component.
 * Armor reaches pawns through the production route: inventory grant, the owner's equipment intent, loadout,
 * equipment instance, character part. Only replicated part lists, collected cosmetic tags and the Leader Pose
 * binding are asserted; masks, grooms and materials stay presentation details of the part Blueprints.
 */
namespace RpgCharacterPartsPIETests
{
	constexpr TCHAR PawnClassPath[] = TEXT("/Game/SurvivalRpg/Characters/GASP/CMC/RPG/Traversal/BP_RpgGasp_MantleCharacter.BP_RpgGasp_MantleCharacter_C");
	constexpr TCHAR GameModePath[] = TEXT("/Game/SurvivalRpg/Maps/Test/GaspMantle/BP_Rpg_GaspMantleTestGameMode.BP_Rpg_GaspMantleTestGameMode_C");
	constexpr TCHAR BodyPartPath[] = TEXT("/Game/SurvivalRpg/Characters/MetaHumans/Parts/B_RpgPart_MetaHumanPaladin.B_RpgPart_MetaHumanPaladin_C");
	constexpr TCHAR InvisibleMeshPath[] = TEXT("/Game/SurvivalRpg/Characters/GASP/Manny/Meshes/SKM_Manny_Simple_GaspLook_Invis.SKM_Manny_Simple_GaspLook_Invis");
	constexpr const TCHAR* ArmorItemPaths[] = {
		TEXT("/GF_Combat_Core/Items/Armor/ID_BasicHelmet.ID_BasicHelmet_C"),
		TEXT("/GF_Combat_Core/Items/Armor/ID_PaladinChest.ID_PaladinChest_C"),
		TEXT("/GF_Combat_Core/Items/Armor/ID_PaladinBracers.ID_PaladinBracers_C"),
		TEXT("/GF_Combat_Core/Items/Armor/ID_PaladinPants.ID_PaladinPants_C"),
		TEXT("/GF_Combat_Core/Items/Armor/ID_PaladinBoots.ID_PaladinBoots_C"),
	};
	constexpr ERpgEquipmentSlot ArmorSlots[] = {
		ERpgEquipmentSlot::Head, ERpgEquipmentSlot::Chest, ERpgEquipmentSlot::Hands, ERpgEquipmentSlot::Legs, ERpgEquipmentSlot::Feet };
	constexpr int32 ArmorCount = UE_ARRAY_COUNT(ArmorItemPaths);
	const FName LeaderComponentTag(TEXT("CharacterPartLeader"));

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
			Prefix = TEXT("SurvivalRpg_CharacterPartsAutomation_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
			Handle = FGameModeEvents::OnGameModeInitializedEvent().AddRaw(this, &FScopedSaveIsolation::OnInitialized);
		}
		bool IsIsolated(UWorld* World) const
		{
			if (!World || World->WorldType != EWorldType::PIE) return false;
			if (World->GetNetMode() == NM_Client) return World->GetAuthGameMode() == nullptr;
			const ARpgGameModeBase* GameMode = World->GetAuthGameMode<ARpgGameModeBase>();
			return GameMode && !GameMode->bEnableDiskPersistence && GameMode->WorldSaveSlotName.StartsWith(Prefix)
				&& GameMode->OfflineProfileKey.StartsWith(Prefix);
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
		TWeakObjectPtr<ARpgCharacter> PawnBeforeDeath;
	};

	ARpgCharacter* LocalCharacter(UWorld* World)
	{
		const APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
		return Controller ? Cast<ARpgCharacter>(Controller->GetPawn()) : nullptr;
	}
	ARpgPlayerState* FindPlayerState(UWorld* World, int32 PlayerId)
	{
		const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
		if (!GameState || PlayerId == INDEX_NONE) return nullptr;
		for (APlayerState* PlayerState : GameState->PlayerArray)
		{
			if (PlayerState && PlayerState->GetPlayerId() == PlayerId) return Cast<ARpgPlayerState>(PlayerState);
		}
		return nullptr;
	}
	/** The pawn currently possessed by the player, never a corpse that still carries the old PlayerState. */
	ARpgCharacter* PlayerCharacter(UWorld* World, int32 PlayerId)
	{
		const ARpgPlayerState* PlayerState = FindPlayerState(World, PlayerId);
		return PlayerState ? PlayerState->GetPawn<ARpgCharacter>() : nullptr;
	}
	bool Ready(UWorld* World, const ARpgCharacter* Character)
	{
		const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
		const URpgExperienceManagerComponent* Manager = GameState ? GameState->FindComponentByClass<URpgExperienceManagerComponent>() : nullptr;
		return World && World->GetNetDriver() && Manager && Manager->IsExperienceLoaded()
			&& Manager->GetCurrentExperienceChecked()->GetPrimaryAssetId() == ExperienceId()
			&& Character && Character->GetClass()->GetPathName() == PawnClassPath
			&& Character->GetPlayerState() && Character->GetRpgAbilitySystemComponent();
	}
	URpgPawnComponent_CharacterParts* PartsOf(const ARpgCharacter* Character)
	{
		return Character ? Character->FindComponentByClass<URpgPawnComponent_CharacterParts>() : nullptr;
	}
	USkeletalMeshComponent* LeaderMeshOf(const AActor* BodyPart)
	{
		TInlineComponentArray<USkeletalMeshComponent*> Meshes(BodyPart);
		for (USkeletalMeshComponent* Mesh : Meshes)
		{
			if (Mesh->ComponentHasTag(LeaderComponentTag)) return Mesh;
		}
		return nullptr;
	}
	bool HasCoverTag(const URpgPawnComponent_CharacterParts* Parts, const TCHAR* Tag)
	{
		const FGameplayTag Cover = FGameplayTag::RequestGameplayTag(TEXT("Cosmetic.Cover"));
		return Parts->GetCombinedTags(Cover).HasTagExact(FGameplayTag::RequestGameplayTag(Tag));
	}

	/**
	 * True when this machine shows the MetaHuman Paladin body plus exactly Armor armor parts, every armor mesh
	 * follows the body's leader mesh, the gameplay mesh switched to its invisible variant and the head is covered
	 * exactly when the helmet is worn.
	 */
	bool WearsPaladin(const ARpgCharacter* Character, int32 Armor, bool bHelmet)
	{
		const URpgPawnComponent_CharacterParts* Parts = PartsOf(Character);
		if (!Parts) return false;
		const TArray<AActor*> Actors = Parts->GetCharacterPartActors();
		const AActor* Body = nullptr;
		for (const AActor* Actor : Actors)
		{
			if (Actor && Actor->GetClass()->GetPathName() == BodyPartPath)
			{
				if (Body) return false;
				Body = Actor;
			}
		}
		USkeletalMeshComponent* Leader = Body ? LeaderMeshOf(Body) : nullptr;
		if (!Leader || Actors.Num() != Armor + 1) return false;
		for (const AActor* Actor : Actors)
		{
			if (!Actor || Actor == Body) continue;
			TInlineComponentArray<USkeletalMeshComponent*> Meshes(Actor);
			if (Meshes.IsEmpty()) return false;
			for (const USkeletalMeshComponent* Mesh : Meshes)
			{
				if (Mesh->LeaderPoseComponent.Get() != Leader) return false;
			}
		}
		const USkeletalMesh* GameplayMesh = Character->GetMesh()->GetSkeletalMeshAsset();
		return GameplayMesh && GameplayMesh->GetPathName() == InvisibleMeshPath
			&& Parts->GetCombinedTags(FGameplayTag()).HasTagExact(FGameplayTag::RequestGameplayTag(TEXT("Cosmetic.Body")))
			&& HasCoverTag(Parts, TEXT("Cosmetic.Cover.Head")) == bHelmet;
	}
	bool SlotEquipped(const ARpgCharacter* Character, ERpgEquipmentSlot Slot)
	{
		const URpgEquipmentManagerComponent* Equipment = Character ? Character->GetEquipmentManagerComponent() : nullptr;
		return Equipment && Equipment->GetEquipmentInstanceInSlot(Slot) != nullptr;
	}
	URpgInventoryManagerComponent* InventoryOf(UWorld* World, int32 PlayerId)
	{
		const ARpgPlayerState* PlayerState = FindPlayerState(World, PlayerId);
		return PlayerState ? PlayerState->GetInventoryManagerComponent() : nullptr;
	}
	bool FindEntry(const URpgInventoryManagerComponent* Inventory, const TCHAR* ItemPath, FRpgInventoryEntryView& OutEntry)
	{
		if (!Inventory) return false;
		for (const FRpgInventoryEntryView& Entry : Inventory->GetAllEntries())
		{
			if (Entry.Instance && Entry.Instance->GetItemDef() && Entry.Instance->GetItemDef()->GetPathName() == ItemPath)
			{
				OutEntry = Entry;
				return true;
			}
		}
		return false;
	}
	/** Grants the first Count armor items to the player's inventory on the authority. */
	void GrantArmor(UWorld* World, int32 PlayerId, int32 Count, FAutomationTestBase& Test)
	{
		URpgInventoryManagerComponent* Inventory = InventoryOf(World, PlayerId);
		Test.TestNotNull(TEXT("The player's inventory exists on the server"), Inventory);
		for (int32 Index = 0; Inventory && Index < Count; ++Index)
		{
			UClass* ItemClass = LoadClass<URpgInventoryItemDefinition>(nullptr, ArmorItemPaths[Index]);
			Test.TestNotNull(*FString::Printf(TEXT("%s loads"), ArmorItemPaths[Index]), ItemClass);
			if (ItemClass)
			{
				Test.TestNotNull(*FString::Printf(TEXT("%s is granted"), ArmorItemPaths[Index]), Inventory->GrantItemDefinition(ItemClass));
			}
		}
	}
	/** True when the local controller's inventory replica holds the first Count armor items. */
	bool OwnerHoldsArmor(UWorld* World, int32 Count)
	{
		const APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
		const ARpgPlayerState* PlayerState = Controller ? Controller->GetPlayerState<ARpgPlayerState>() : nullptr;
		const URpgInventoryManagerComponent* Inventory = PlayerState ? PlayerState->GetInventoryManagerComponent() : nullptr;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			FRpgInventoryEntryView Entry;
			if (!FindEntry(Inventory, ArmorItemPaths[Index], Entry)) return false;
		}
		return Inventory != nullptr;
	}
	/** Sends one whole-entry equipment intent from the local controller exactly as the inventory UI does. */
	void RequestEquipmentIntent(UWorld* World, const TCHAR* ItemPath, ERpgInventoryEquipmentIntentOperation Operation, FAutomationTestBase& Test)
	{
		APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
		URpgInventoryUiActionComponent* Actions = Controller ? Controller->FindComponentByClass<URpgInventoryUiActionComponent>() : nullptr;
		const ARpgPlayerState* PlayerState = Controller ? Controller->GetPlayerState<ARpgPlayerState>() : nullptr;
		URpgInventoryManagerComponent* Inventory = PlayerState ? PlayerState->GetInventoryManagerComponent() : nullptr;
		FRpgInventoryEntryView Entry;
		if (!Test.TestTrue(*FString::Printf(TEXT("%s waits in the inventory"), ItemPath), Actions && FindEntry(Inventory, ItemPath, Entry)))
		{
			return;
		}
		FRpgInventoryEquipmentIntent Intent;
		Intent.EnsureRequestId();
		Intent.ItemId = Entry.ItemId;
		Intent.ExpectedEntryId = Entry.EntryId;
		Intent.ExpectedSourcePlacement = Entry.Placement;
		Intent.ExpectedQuantity = Entry.StackCount;
		Intent.Operation = Operation;
		Actions->RequestApplyInventoryEquipmentIntent(Inventory, Intent);
	}
	bool RefuseRunningPIE(FAutomationTestBase& Test)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && IsValid(Context.World()))
			{
				Test.AddError(TEXT("Character parts automation refuses to interrupt an existing PIE session."));
				return true;
			}
		}
		return false;
	}
}

#if ENABLE_PIE_NETWORK_TEST

NETWORK_TEST_CLASS(CharacterPartsListenServerPIE, "SurvivalRpg.Cosmetics.CharacterParts")
{
	using FState = RpgCharacterPartsPIETests::FState;
	// Destruction is reversed: network cleanup finishes while save isolation is still installed.
	RpgCharacterPartsPIETests::FScopedSaveIsolation Isolation;
	FRpgScopedNetworkTestGround Ground;
	FPIENetworkComponent<FState> Network{TestRunner, TestCommandBuilder, bInitializing};
	FPrimaryAssetId OriginalExperience;
	bool bConfigured = false;
	int32 SubjectId = INDEX_NONE;

	BEFORE_EACH()
	{
		using namespace RpgCharacterPartsPIETests;
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

	TEST_METHOD(InventoryArmorReachesOwnerProxyLateJoinAndRespawn)
	{
		using namespace RpgCharacterPartsPIETests;
		if (!bConfigured) return;
		Network.ThenServer(TEXT("Server persistence was isolated before InitGame"), [this](FState& State)
			{ ASSERT_THAT(IsTrue(Isolation.IsIsolated(State.World))); })
			.UntilClient(TEXT("Remote owner wears the controller-granted MetaHuman body"), 0, [](FState& State)
				{ ARpgCharacter* Character = LocalCharacter(State.World); return Ready(State.World, Character) && WearsPaladin(Character, 0, false); }, Timeout())
			.ThenClient(TEXT("Capture the remote owner's identity"), 0, [this](FState& State)
			{
				ARpgCharacter* Character = LocalCharacter(State.World);
				ASSERT_THAT(IsTrue(Character->GetLocalRole() == ROLE_AutonomousProxy));
				SubjectId = Character->GetPlayerState()->GetPlayerId();
			})
			.UntilServer(TEXT("Listen server spawns the body part for the remote pawn and its own pawn"), [this](FState& State)
			{
				ARpgCharacter* Remote = PlayerCharacter(State.World, SubjectId);
				ARpgCharacter* Host = LocalCharacter(State.World);
				return Ready(State.World, Remote) && WearsPaladin(Remote, 0, false) && Ready(State.World, Host) && WearsPaladin(Host, 0, false);
			}, Timeout())
			.ThenServer(TEXT("Grant the five Paladin items to the remote owner"), [this](FState& State)
				{ GrantArmor(State.World, SubjectId, ArmorCount, *TestRunner); })
			.UntilClient(TEXT("The owner's inventory replica receives every armor item"), 0, [](FState& State)
				{ return OwnerHoldsArmor(State.World, ArmorCount); }, Timeout());
		// One intent per step, as a player equips piece by piece in the inventory UI.
		for (int32 Index = 0; Index < ArmorCount; ++Index)
		{
			Network.ThenClient(TEXT("Owner equips the next piece through the inventory equipment intent"), 0, [this, Index](FState& State)
					{ RequestEquipmentIntent(State.World, ArmorItemPaths[Index], ERpgInventoryEquipmentIntentOperation::EquipDefaultAndActivate, *TestRunner); })
				.UntilServer(TEXT("The owner's loadout equips that piece"), [this, Index](FState& State)
					{ return SlotEquipped(PlayerCharacter(State.World, SubjectId), ArmorSlots[Index]); }, Timeout());
		}
		Network.UntilServer(TEXT("Server shows the body and five armor parts"), [this](FState& State)
				{ return WearsPaladin(PlayerCharacter(State.World, SubjectId), ArmorCount, true); }, Timeout())
			.UntilClient(TEXT("Owner receives the replicated parts and binds the armor to the body"), 0, [](FState& State)
				{ return WearsPaladin(LocalCharacter(State.World), ArmorCount, true); }, Timeout())
			.ThenClientJoins()
			.UntilClient(TEXT("Late joiner reconstructs the armored simulated proxy"), 1, [this](FState& State)
			{
				ARpgCharacter* Remote = PlayerCharacter(State.World, SubjectId);
				return Isolation.IsIsolated(State.World) && Ready(State.World, Remote)
					&& Remote->GetLocalRole() == ROLE_SimulatedProxy && WearsPaladin(Remote, ArmorCount, true);
			}, Timeout())
			.ThenClient(TEXT("Owner takes the helmet off through the unequip intent"), 0, [this](FState& State)
				{ RequestEquipmentIntent(State.World, ArmorItemPaths[0], ERpgInventoryEquipmentIntentOperation::UnequipToContent, *TestRunner); })
			.UntilServer(TEXT("Server removes only the helmet part"), [this](FState& State)
			{
				ARpgCharacter* Remote = PlayerCharacter(State.World, SubjectId);
				return !SlotEquipped(Remote, ERpgEquipmentSlot::Head) && WearsPaladin(Remote, ArmorCount - 1, false);
			}, Timeout())
			.UntilClient(TEXT("Owner uncovers the head"), 0, [](FState& State)
				{ return WearsPaladin(LocalCharacter(State.World), ArmorCount - 1, false); }, Timeout())
			.UntilClient(TEXT("Late joiner uncovers the head of the proxy"), 1, [this](FState& State)
				{ return WearsPaladin(PlayerCharacter(State.World, SubjectId), ArmorCount - 1, false); }, Timeout())
			.ThenClients(TEXT("Record each client's pawn before death"), [this](FState& State)
				{ State.PawnBeforeDeath = PlayerCharacter(State.World, SubjectId); })
			.ThenServer(TEXT("Enter final death through the authoritative health and downed components"), [this](FState& State)
			{
				ARpgCharacter* Character = PlayerCharacter(State.World, SubjectId);
				URpgHealthComponent* Health = URpgHealthComponent::FindHealthComponent(Character);
				ASSERT_THAT(IsNotNull(Health));
				if (!Health) return;
				State.PawnBeforeDeath = Character;
				Health->DamageSelfDestruct(false);
				if (URpgDownedComponent* Downed = URpgDownedComponent::FindDownedComponent(Character); Downed && Downed->IsDowned())
				{
					Downed->ForceDeathFromDowned();
				}
			})
			.UntilClient(TEXT("Owner may respawn"), 0, [this](FState& State)
			{
				const ARpgPlayerState* PlayerState = FindPlayerState(State.World, SubjectId);
				return PlayerState && PlayerState->IsWaitingForRespawn() && PlayerState->CanRespawnNow();
			}, Timeout())
			.ThenClient(TEXT("Owner requests respawn through the controller RPC"), 0, [this](FState& State)
			{
				ARpgPlayerController* Controller = Cast<ARpgPlayerController>(State.World->GetFirstPlayerController());
				ASSERT_THAT(IsNotNull(Controller));
				if (Controller) Controller->RequestRespawn();
			})
			// The inventory still holds the four worn pieces, so the loadout re-equips them and the controller parts
			// component re-applies the body on every machine.
			.UntilServer(TEXT("The new pawn wears the body and the four remaining pieces"), [this](FState& State)
			{
				ARpgCharacter* Character = PlayerCharacter(State.World, SubjectId);
				return Ready(State.World, Character) && Character != State.PawnBeforeDeath.Get() && WearsPaladin(Character, ArmorCount - 1, false);
			}, Timeout())
			.UntilClient(TEXT("Owner sees the respawned pawn armored"), 0, [this](FState& State)
			{
				ARpgCharacter* Character = LocalCharacter(State.World);
				return Ready(State.World, Character) && Character != State.PawnBeforeDeath.Get() && WearsPaladin(Character, ArmorCount - 1, false);
			}, Timeout())
			.UntilClient(TEXT("Late joiner sees the respawned proxy armored"), 1, [this](FState& State)
			{
				ARpgCharacter* Character = PlayerCharacter(State.World, SubjectId);
				return Ready(State.World, Character) && Character != State.PawnBeforeDeath.Get() && WearsPaladin(Character, ArmorCount - 1, false);
			}, Timeout())
			.ThenServer(TEXT("Grant the five Paladin items to the listen-server host"), [this](FState& State)
				{ GrantArmor(State.World, State.World->GetFirstPlayerController()->GetPlayerState<APlayerState>()->GetPlayerId(), ArmorCount, *TestRunner); });
		for (int32 Index = 0; Index < ArmorCount; ++Index)
		{
			Network.ThenServer(TEXT("Host equips the next piece through the inventory equipment intent"), [this, Index](FState& State)
					{ RequestEquipmentIntent(State.World, ArmorItemPaths[Index], ERpgInventoryEquipmentIntentOperation::EquipDefaultAndActivate, *TestRunner); })
				.UntilServer(TEXT("The host's loadout equips that piece"), [Index](FState& State)
					{ return SlotEquipped(LocalCharacter(State.World), ArmorSlots[Index]); }, Timeout());
		}
		Network.UntilServer(TEXT("Host shows the body and five armor parts"), [](FState& State)
				{ return WearsPaladin(LocalCharacter(State.World), ArmorCount, true); }, Timeout())
			.UntilClients(TEXT("Both clients see the armored host"), [this](FState& State)
			{
				for (TActorIterator<ARpgCharacter> It(State.World); It; ++It)
				{
					if (It->GetPlayerState() && It->GetPlayerState()->GetPlayerId() != SubjectId
						&& It->GetPlayerState()->GetPawn() == *It && WearsPaladin(*It, ArmorCount, true))
					{
						return true;
					}
				}
				return false;
			}, Timeout())
			.ThenServer(TEXT("Persistence remains disabled through test completion"), [this](FState& State)
				{ ASSERT_THAT(IsTrue(Isolation.IsIsolated(State.World))); });
	}
};

NETWORK_TEST_CLASS(CharacterPartsDedicatedServerPIE, "SurvivalRpg.Cosmetics.CharacterParts")
{
	using FState = RpgCharacterPartsPIETests::FState;
	RpgCharacterPartsPIETests::FScopedSaveIsolation Isolation;
	FRpgScopedNetworkTestGround Ground;
	FPIENetworkComponent<FState> Network{TestRunner, TestCommandBuilder, bInitializing};
	FPrimaryAssetId OriginalExperience;
	bool bConfigured = false;
	int32 SubjectId = INDEX_NONE;

	BEFORE_EACH()
	{
		using namespace RpgCharacterPartsPIETests;
		if (RefuseRunningPIE(*TestRunner)) return;
		UClass* GameModeClass = LoadClass<ARpgGameModeBase>(nullptr, GameModePath);
		ASSERT_THAT(IsNotNull(GameModeClass));
		if (!GameModeClass) return;
		Isolation.Start();
		Ground.Start();
		OriginalExperience = GetDefault<URpgDeveloperSettings>()->ExperienceOverride;
		GetMutableDefault<URpgDeveloperSettings>()->ExperienceOverride = ExperienceId();
		bConfigured = true;
		FNetworkComponentBuilder<FState>().WithClients(1).AsDedicatedServer()
			.WithGameInstanceClass(FSoftClassPath(TEXT("/Game/SurvivalRpg/Core/Game/BP_Rpg_GameInstance.BP_Rpg_GameInstance_C")))
			.WithGameMode(GameModeClass).Build(Network);
	}
	AFTER_EACH()
	{
		if (bConfigured) GetMutableDefault<URpgDeveloperSettings>()->ExperienceOverride = OriginalExperience;
	}

	TEST_METHOD(DedicatedServerReplicatesPartsWithoutSpawningThem)
	{
		using namespace RpgCharacterPartsPIETests;
		if (!bConfigured) return;
		Network.UntilClient(TEXT("Client wears the controller-granted MetaHuman body"), 0, [](FState& State)
				{ ARpgCharacter* Character = LocalCharacter(State.World); return Ready(State.World, Character) && WearsPaladin(Character, 0, false); }, Timeout())
			.ThenClient(TEXT("Capture the client's identity"), 0, [this](FState& State)
				{ SubjectId = LocalCharacter(State.World)->GetPlayerState()->GetPlayerId(); })
			.ThenServer(TEXT("Grant helmet and chest on the dedicated server"), [this](FState& State)
			{
				ASSERT_THAT(IsTrue(State.World->GetNetMode() == NM_DedicatedServer && Isolation.IsIsolated(State.World)));
				GrantArmor(State.World, SubjectId, 2, *TestRunner);
			})
			.UntilClient(TEXT("Client inventory replica receives both items"), 0, [](FState& State)
				{ return OwnerHoldsArmor(State.World, 2); }, Timeout());
		for (int32 Index = 0; Index < 2; ++Index)
		{
			Network.ThenClient(TEXT("Client equips the next piece through the inventory equipment intent"), 0, [this, Index](FState& State)
					{ RequestEquipmentIntent(State.World, ArmorItemPaths[Index], ERpgInventoryEquipmentIntentOperation::EquipDefaultAndActivate, *TestRunner); })
				.UntilServer(TEXT("The dedicated server's loadout equips that piece"), [this, Index](FState& State)
					{ return SlotEquipped(PlayerCharacter(State.World, SubjectId), ArmorSlots[Index]); }, Timeout());
		}
		Network.UntilClient(TEXT("Client shows the body with helmet and chest"), 0, [](FState& State)
				{ return WearsPaladin(LocalCharacter(State.World), 2, true); }, Timeout())
			.ThenServer(TEXT("Dedicated server keeps the equipment but spawned no part actors"), [this](FState& State)
			{
				ARpgCharacter* Character = PlayerCharacter(State.World, SubjectId);
				const URpgPawnComponent_CharacterParts* Parts = PartsOf(Character);
				ASSERT_THAT(IsNotNull(Parts));
				if (!Parts) return;
				ASSERT_THAT(IsTrue(SlotEquipped(Character, ERpgEquipmentSlot::Head) && SlotEquipped(Character, ERpgEquipmentSlot::Chest)));
				ASSERT_THAT(IsTrue(Parts->GetCharacterPartActors().IsEmpty()));
				int32 PartActors = 0;
				for (TActorIterator<AActor> It(State.World); It; ++It)
				{
					if (It->GetClass()->GetPathName().Contains(TEXT("B_RpgPart_"))) ++PartActors;
				}
				ASSERT_THAT(AreEqual(0, PartActors));
			});
	}
};

#endif // ENABLE_PIE_NETWORK_TEST

/**
 * GASP-17 composition contract: the production CMC Experiences show the Paladin only through the removable
 * LAS_RpgCharacter_Paladin action set, and the UEFN Mover Experiences never compose it.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgPaladinExperienceCompositionTest,
	"SurvivalRpg.Cosmetics.CharacterParts.PaladinExperienceComposition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgPaladinExperienceCompositionTest::RunTest(const FString& Parameters)
{
	const URpgExperienceActionSet* Paladin = LoadObject<URpgExperienceActionSet>(nullptr,
		TEXT("/Game/SurvivalRpg/System/Experiences/ActionSets/LAS_RpgCharacter_Paladin.LAS_RpgCharacter_Paladin"));
	if (!TestNotNull(TEXT("The Paladin action set loads"), Paladin)) return false;
	// Unset when the Experience does not load; otherwise whether it composes the Paladin action set.
	auto Composes = [Paladin](const TCHAR* Name) -> TOptional<bool>
	{
		const UClass* Class = LoadClass<URpgExperienceDefinition>(nullptr,
			*FString::Printf(TEXT("/Game/SurvivalRpg/System/Experiences/%s.%s_C"), Name, Name));
		if (!Class) return {};
		return GetDefault<URpgExperienceDefinition>(Class)->ActionSets.ContainsByPredicate(
			[Paladin](const TObjectPtr<URpgExperienceActionSet>& Set) { return Set == Paladin; });
	};
	for (const TCHAR* Name : { TEXT("RpgGaspCMCExperience"), TEXT("RpgGaspMantleExperience"), TEXT("RpgInventoryStorageExperience") })
	{
		TestTrue(FString::Printf(TEXT("%s composes the Paladin action set"), Name), Composes(Name).Get(false));
	}
	for (const TCHAR* Name : { TEXT("RpgGaspMoverExperience"), TEXT("RpgGaspMoverRagdollExperience") })
	{
		const TOptional<bool> Result = Composes(Name);
		TestTrue(FString::Printf(TEXT("%s loads"), Name), Result.IsSet());
		TestFalse(FString::Printf(TEXT("%s keeps the UEFN Mover without the Paladin"), Name), Result.Get(true));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
