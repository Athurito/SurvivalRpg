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
#include "Misc/Guid.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Core/Character/RpgCharacter.h"
#include "SurvivalRpg/Core/Character/RpgDownedComponent.h"
#include "SurvivalRpg/Core/Character/RpgHealthComponent.h"
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
 * GASP-16 pilot content contract for Lyra-style character parts in a real network session.
 *
 * The pilot Experience gives every pawn the MetaHuman Paladin body part through the controller parts component;
 * armor reaches the pawn through the authoritative equipment manager, whose instances add the definition's parts.
 * Only replicated part lists, collected cosmetic tags and the Leader Pose binding are asserted; masks, grooms and
 * materials stay presentation details of the part Blueprints. The listen-server host also equips through the real
 * inventory route (grant, equipment intent, loadout). Remote owners do not: their sequential inventory equips
 * currently crash FastArray replication of the inventory list (see the GASP-16 report).
 */
namespace RpgCharacterPartsPIETests
{
	constexpr TCHAR PawnClassPath[] = TEXT("/Game/SurvivalRpg/Characters/GASP/CMC/RPG/Traversal/BP_RpgGasp_MantleCharacter.BP_RpgGasp_MantleCharacter_C");
	constexpr TCHAR GameModePath[] = TEXT("/Game/SurvivalRpg/Maps/Test/GaspMantle/BP_Rpg_GaspMantleTestGameMode.BP_Rpg_GaspMantleTestGameMode_C");
	constexpr TCHAR BodyPartPath[] = TEXT("/Game/SurvivalRpg/Characters/MetaHumans/Parts/B_RpgPart_MetaHumanPaladin.B_RpgPart_MetaHumanPaladin_C");
	constexpr TCHAR InvisibleMeshPath[] = TEXT("/Game/SurvivalRpg/Characters/GASP/Manny/Meshes/SKM_Manny_Simple_GaspLook_Invis.SKM_Manny_Simple_GaspLook_Invis");
	constexpr const TCHAR* ArmorDefinitionPaths[] = {
		TEXT("/GF_Combat_Core/Equipment/Armor/ED_BasicHelmet.ED_BasicHelmet_C"),
		TEXT("/GF_Combat_Core/Equipment/Armor/ED_PaladinChest.ED_PaladinChest_C"),
		TEXT("/GF_Combat_Core/Equipment/Armor/ED_PaladinBracers.ED_PaladinBracers_C"),
		TEXT("/GF_Combat_Core/Equipment/Armor/ED_PaladinPants.ED_PaladinPants_C"),
		TEXT("/GF_Combat_Core/Equipment/Armor/ED_PaladinBoots.ED_PaladinBoots_C"),
	};
	constexpr ERpgEquipmentSlot ArmorSlots[] = {
		ERpgEquipmentSlot::Head, ERpgEquipmentSlot::Chest, ERpgEquipmentSlot::Hands, ERpgEquipmentSlot::Legs, ERpgEquipmentSlot::Feet };
	constexpr const TCHAR* ArmorItemPaths[] = {
		TEXT("/GF_Combat_Core/Items/Armor/ID_BasicHelmet.ID_BasicHelmet_C"),
		TEXT("/GF_Combat_Core/Items/Armor/ID_PaladinChest.ID_PaladinChest_C"),
		TEXT("/GF_Combat_Core/Items/Armor/ID_PaladinBracers.ID_PaladinBracers_C"),
		TEXT("/GF_Combat_Core/Items/Armor/ID_PaladinPants.ID_PaladinPants_C"),
		TEXT("/GF_Combat_Core/Items/Armor/ID_PaladinBoots.ID_PaladinBoots_C"),
	};
	constexpr int32 ArmorCount = UE_ARRAY_COUNT(ArmorDefinitionPaths);
	const FName LeaderComponentTag(TEXT("CharacterPartLeader"));

	FPrimaryAssetId ExperienceId()
	{
		return FPrimaryAssetId(URpgExperienceDefinition::StaticClass()->GetFName(), TEXT("RpgGaspPaladinExperience"));
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
	/** Equips the first Count armor pieces in their slots through the authoritative equipment manager. */
	void EquipArmor(ARpgCharacter* Character, int32 Count, FAutomationTestBase& Test)
	{
		URpgEquipmentManagerComponent* Equipment = Character ? Character->GetEquipmentManagerComponent() : nullptr;
		Test.TestTrue(TEXT("The authoritative pawn has an equipment manager"), Equipment && Character->HasAuthority());
		for (int32 Index = 0; Equipment && Index < Count; ++Index)
		{
			UClass* Definition = LoadClass<URpgEquipmentDefinition>(nullptr, ArmorDefinitionPaths[Index]);
			Test.TestNotNull(*FString::Printf(TEXT("%s loads"), ArmorDefinitionPaths[Index]), Definition);
			if (Definition)
			{
				Test.TestNotNull(*FString::Printf(TEXT("%s equips"), ArmorDefinitionPaths[Index]),
					Equipment->EquipItemInSlot(Definition, ArmorSlots[Index]));
			}
		}
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
	/** Sends one whole-entry equipment intent through the controller's inventory UI action component. */
	void RequestInventoryEquip(APlayerController* Controller, const TCHAR* ItemPath, FAutomationTestBase& Test)
	{
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
		Intent.Operation = ERpgInventoryEquipmentIntentOperation::EquipDefaultAndActivate;
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

	TEST_METHOD(ArmorPartsReachOwnerProxyLateJoinAndRespawn)
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
			.ThenServer(TEXT("Equip the five Paladin pieces through the authoritative equipment manager"), [this](FState& State)
				{ EquipArmor(PlayerCharacter(State.World, SubjectId), ArmorCount, *TestRunner); })
			.UntilServer(TEXT("Server shows the body and five armor parts"), [this](FState& State)
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
			.ThenServer(TEXT("Take the helmet off"), [this](FState& State)
			{
				URpgEquipmentManagerComponent* Equipment = PlayerCharacter(State.World, SubjectId)->GetEquipmentManagerComponent();
				Equipment->UnequipItemInSlot(ERpgEquipmentSlot::Head);
				ASSERT_THAT(IsNull(Equipment->GetEquipmentInstanceInSlot(ERpgEquipmentSlot::Head)));
			})
			.UntilServer(TEXT("Server removes only the helmet part"), [this](FState& State)
				{ return WearsPaladin(PlayerCharacter(State.World, SubjectId), ArmorCount - 1, false); }, Timeout())
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
			// Directly equipped armor is not inventory-backed, so the loadout does not restore it; the controller
			// parts component must re-apply the body to the new pawn on every machine.
			.UntilServer(TEXT("The new pawn wears the controller-granted body again"), [this](FState& State)
			{
				ARpgCharacter* Character = PlayerCharacter(State.World, SubjectId);
				return Ready(State.World, Character) && Character != State.PawnBeforeDeath.Get() && WearsPaladin(Character, 0, false);
			}, Timeout())
			.UntilClient(TEXT("Owner sees the respawned pawn with its body"), 0, [this](FState& State)
			{
				ARpgCharacter* Character = LocalCharacter(State.World);
				return Ready(State.World, Character) && Character != State.PawnBeforeDeath.Get() && WearsPaladin(Character, 0, false);
			}, Timeout())
			.UntilClient(TEXT("Late joiner sees the respawned proxy with its body"), 1, [this](FState& State)
			{
				ARpgCharacter* Character = PlayerCharacter(State.World, SubjectId);
				return Ready(State.World, Character) && Character != State.PawnBeforeDeath.Get() && WearsPaladin(Character, 0, false);
			}, Timeout())
			.ThenServer(TEXT("Equip the chest on the respawned pawn"), [this](FState& State)
				{ EquipArmor(PlayerCharacter(State.World, SubjectId), 2, *TestRunner); })
			.UntilClient(TEXT("Late joiner sees the new pawn's armor"), 1, [this](FState& State)
				{ return WearsPaladin(PlayerCharacter(State.World, SubjectId), 2, true); }, Timeout())
			.ThenServer(TEXT("Grant the five Paladin items to the listen-server host"), [this](FState& State)
			{
				const ARpgPlayerState* PlayerState = State.World->GetFirstPlayerController()->GetPlayerState<ARpgPlayerState>();
				URpgInventoryManagerComponent* Inventory = PlayerState ? PlayerState->GetInventoryManagerComponent() : nullptr;
				ASSERT_THAT(IsNotNull(Inventory));
				if (!Inventory) return;
				for (const TCHAR* ItemPath : ArmorItemPaths)
				{
					UClass* ItemClass = LoadClass<URpgInventoryItemDefinition>(nullptr, ItemPath);
					ASSERT_THAT(IsNotNull(ItemClass));
					if (ItemClass) ASSERT_THAT(IsNotNull(Inventory->GrantItemDefinition(ItemClass)));
				}
			});
		// The host equips piece by piece, as a player does in the inventory UI.
		for (int32 Index = 0; Index < ArmorCount; ++Index)
		{
			Network.ThenServer(TEXT("Host equips the next piece through the inventory equipment intent"), [this, Index](FState& State)
					{ RequestInventoryEquip(State.World->GetFirstPlayerController(), ArmorItemPaths[Index], *TestRunner); })
				.UntilServer(TEXT("The host's loadout equips that piece"), [Index](FState& State)
				{
					const ARpgCharacter* Host = LocalCharacter(State.World);
					const URpgEquipmentManagerComponent* Equipment = Host ? Host->GetEquipmentManagerComponent() : nullptr;
					return Equipment && Equipment->GetEquipmentInstanceInSlot(ArmorSlots[Index]) != nullptr;
				}, Timeout());
		}
		Network.UntilServer(TEXT("Host shows the body and five armor parts"), [](FState& State)
				{ return WearsPaladin(LocalCharacter(State.World), ArmorCount, true); }, Timeout())
			.UntilClients(TEXT("Both clients see the inventory-equipped host armored"), [this](FState& State)
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
			.ThenServer(TEXT("Equip helmet and chest on the dedicated server"), [this](FState& State)
			{
				ASSERT_THAT(IsTrue(State.World->GetNetMode() == NM_DedicatedServer && Isolation.IsIsolated(State.World)));
				EquipArmor(PlayerCharacter(State.World, SubjectId), 2, *TestRunner);
			})
			.UntilClient(TEXT("Client shows the body with helmet and chest"), 0, [](FState& State)
				{ return WearsPaladin(LocalCharacter(State.World), 2, true); }, Timeout())
			.ThenServer(TEXT("Dedicated server keeps the equipment but spawned no part actors"), [this](FState& State)
			{
				ARpgCharacter* Character = PlayerCharacter(State.World, SubjectId);
				const URpgEquipmentManagerComponent* Equipment = Character ? Character->GetEquipmentManagerComponent() : nullptr;
				const URpgPawnComponent_CharacterParts* Parts = PartsOf(Character);
				ASSERT_THAT(IsNotNull(Equipment));
				ASSERT_THAT(IsNotNull(Parts));
				if (!Equipment || !Parts) return;
				ASSERT_THAT(IsNotNull(Equipment->GetEquipmentInstanceInSlot(ERpgEquipmentSlot::Head)));
				ASSERT_THAT(IsNotNull(Equipment->GetEquipmentInstanceInSlot(ERpgEquipmentSlot::Chest)));
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

#endif // WITH_DEV_AUTOMATION_TESTS
