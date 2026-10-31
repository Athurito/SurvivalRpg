#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"
#include "Network/RpgCombatNetworkTestTypes.h"
#include "Network/RpgLootHarvestNetworkTestTypes.h"

#include "AbilitySystem/Abilities/RpgGameplayAbility_Harvest.h"
#include "AbilitySystemGlobals.h"
#include "Components/CapsuleComponent.h"
#include "Engine/NetDriver.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameplayCueManager.h"
#include "Harvesting/RpgHarvestableInstancesComponent.h"
#include "Harvesting/RpgHarvestInstanceStockComponent.h"
#include "Harvesting/RpgHarvestProfile.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySet.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Combat/RpgCombatImpact.h"
#include "SurvivalRpg/Core/Character/RpgCharacter.h"
#include "SurvivalRpg/Core/Game/Experience/RpgExperienceDefinition.h"
#include "SurvivalRpg/Core/Game/Experience/RpgExperienceManagerComponent.h"
#include "SurvivalRpg/Core/Game/RpgGameModeBase.h"
#include "SurvivalRpg/Development/RpgDeveloperSettings.h"
#include "SurvivalRpg/Equipment/RpgEquipmentDefinition.h"
#include "SurvivalRpg/Equipment/RpgEquipmentInstance.h"
#include "SurvivalRpg/Equipment/RpgEquipmentManagerComponent.h"
#include "SurvivalRpg/Inventory/Loot/RpgLootTable.h"
#include "SurvivalRpg/Inventory/RpgDroppedInventoryActor.h"

#if ENABLE_PIE_NETWORK_TEST

namespace RpgHarvestSwingPIETests
{
	constexpr TCHAR PrototypeExperienceName[] = TEXT("RpgPrototypeExperience");
	constexpr TCHAR PrototypeGameModeClassPath[] = TEXT("/Game/SurvivalRpg/Core/Game/BP_Rpg_GameMode.BP_Rpg_GameMode_C");
	constexpr TCHAR PrototypeCharacterClassPrefix[] = TEXT("/Game/SurvivalRpg/Core/Character/BP_Rpg_Character");
	constexpr TCHAR AxeDefinitionClassPath[] = TEXT("/GF_Harvesting_Magic/Equipment/Tools/ED_Tool_Axe.ED_Tool_Axe_C");
	constexpr TCHAR AxeAbilitySetPath[] = TEXT("/GF_Harvesting_Magic/GAS/AbilitySets/AS_Tool_Axe.AS_Tool_Axe");

	/** A real 60 Hz server, as in the combat PIE tests: slow rendered frames slow the worlds down instead of skipping. */
	constexpr float MaxSimulatedFrameTime = 1.0f / 60.0f;

	/** Sections of the field's one resource; each committed swing takes one. */
	constexpr int32 FieldSections = 4;

	/** Seconds the test waits after a result for late copies of cues or commits. */
	constexpr double QuietSeconds = 1.5;

	/** The swinger's lane; it faces +X as its spawn point does. The host waits behind and beside it. */
	const FVector SwingerLocation(0.0, 0.0, 100.0);
	const FVector HostLocation(-400.0, 300.0, 100.0);

	struct FNetworkState : public FBasePIENetworkComponentState
	{
		ARpgNetworkAutomationHarvestInstancesFixture* Field = nullptr;
	};

	FTimespan NetworkTimeout()
	{
		return FTimespan::FromSeconds(90.0);
	}

	const FGameplayTag& ToolHitCueTag()
	{
		static const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.Harvesting.ToolHit"));
		return Tag;
	}

	ARpgCharacter* FindLocalCharacter(const UWorld* World)
	{
		const APlayerController* PlayerController = IsValid(World) ? World->GetFirstPlayerController() : nullptr;
		return PlayerController ? Cast<ARpgCharacter>(PlayerController->GetPawn()) : nullptr;
	}

	ARpgCharacter* FindCharacterByPlayerId(UWorld* World, const int32 PlayerId)
	{
		if (!IsValid(World) || PlayerId == INDEX_NONE)
		{
			return nullptr;
		}
		for (TActorIterator<ARpgCharacter> It(World); It; ++It)
		{
			const APlayerState* PlayerState = It->GetPlayerState();
			if (PlayerState && PlayerState->GetPlayerId() == PlayerId)
			{
				return *It;
			}
		}
		return nullptr;
	}

	bool IsPrototypeExperienceReady(const UWorld* World, const int32 ExpectedClients)
	{
		if (!IsValid(World) || !World->GetNetDriver())
		{
			return false;
		}
		const ENetMode NetMode = World->GetNetMode();
		const bool bExpectedWorld = NetMode == NM_Client ||
			(NetMode == NM_ListenServer && World->GetNetDriver()->ClientConnections.Num() >= ExpectedClients);
		const AGameStateBase* GameState = World->GetGameState();
		const URpgExperienceManagerComponent* ExperienceManager =
			GameState ? GameState->FindComponentByClass<URpgExperienceManagerComponent>() : nullptr;
		if (!bExpectedWorld || !ExperienceManager || !ExperienceManager->IsExperienceLoaded())
		{
			return false;
		}
		const URpgExperienceDefinition* Experience = ExperienceManager->GetCurrentExperienceChecked();
		return Experience &&
			Experience->GetPrimaryAssetId() ==
				FPrimaryAssetId(URpgExperienceDefinition::StaticClass()->GetFName(), PrototypeExperienceName);
	}

	bool IsPrototypeCharacterReady(const ARpgCharacter* Character)
	{
		const USkeletalMeshComponent* Mesh = IsValid(Character) ? Character->GetMesh() : nullptr;
		return Mesh && IsValid(Mesh->GetAnimInstance()) &&
			Character->GetClass()->GetPathName().StartsWith(PrototypeCharacterClassPrefix) &&
			IsValid(Character->GetPlayerState()) && IsValid(Character->GetRpgAbilitySystemComponent());
	}

	/** Four sections, one material each, dropped at the resource when the harvester's inventory is full. */
	URpgHarvestProfile* MakeFieldProfile(UObject* Outer)
	{
		URpgHarvestProfile* Profile = NewObject<URpgHarvestProfile>(Outer);
		URpgLootTable* Table = NewObject<URpgLootTable>(Profile);
		FRpgLootGroup& Group = Table->Groups.AddDefaulted_GetRef();
		Group.Mode = ERpgLootGroupMode::Independent;
		Group.GroupChancePercent = 100.0f;
		FRpgLootEntry& Entry = Group.Entries.AddDefaulted_GetRef();
		Entry.ItemDefinition = URpgNetworkAutomationMaterialDefinition::StaticClass();
		Entry.MinimumQuantity = 1;
		Entry.MaximumQuantity = 1;
		Entry.ChancePercent = 100.0f;
		Profile->LootTable = Table;
		Profile->SectionCount = FieldSections;
		Profile->MinimumRespawnSeconds = 0.0f;
		Profile->MaximumRespawnSeconds = 0.0f;
		Profile->OverflowDropClass = ARpgDroppedInventoryActor::StaticClass();
		return Profile;
	}

	/**
	 * Loads the field on one machine, the way every machine loads a PCG partition actor: one 100 cm cube with the
	 * harvestable collision, centered at Location.
	 */
	ARpgNetworkAutomationHarvestInstancesFixture* LoadField(UWorld* World, const FVector& Location)
	{
		UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		ARpgNetworkAutomationHarvestInstancesFixture* Field = World && Cube
			? World->SpawnActorDeferred<ARpgNetworkAutomationHarvestInstancesFixture>(
				ARpgNetworkAutomationHarvestInstancesFixture::StaticClass(),
				FTransform(Location),
				nullptr,
				nullptr,
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn)
			: nullptr;
		if (!Field || !Field->ConfigureHarvestProfile(MakeFieldProfile(Field)))
		{
			return nullptr;
		}
		Field->GetHarvestableInstances()->SetStaticMesh(Cube);
		Field->GetHarvestableInstances()->AddInstance(FTransform::Identity);
		Field->FinishSpawning(FTransform(Location));
		return Field;
	}

	int32 GetRemainingSections(const FNetworkState& State)
	{
		return State.Field ? State.Field->GetHarvestableInstances()->GetRemainingSections(0) : INDEX_NONE;
	}
}

NETWORK_TEST_CLASS(HarvestSwingPIE, "SurvivalRpg.Network")
{
	using FNetworkState = RpgHarvestSwingPIETests::FNetworkState;

	struct FRoutedCue
	{
		TWeakObjectPtr<UWorld> World;
		FVector Location = FVector::ZeroVector;
	};

	FRpgScopedPIEWorldSaveIsolation WorldSaveIsolation;
	FRpgScopedNetworkTestGround Ground;
	FRpgScopedNetworkTestFrameTimeLimit FrameTimeLimit;
	FPIENetworkComponent<FNetworkState> Network{TestRunner, TestCommandBuilder, bInitializing};
	FPacketSimulationSettings PacketSettings;
	FPrimaryAssetId OriginalExperienceOverride;
	UClass* PrototypeGameModeClass = nullptr;
	TArray<FRoutedCue> RoutedCues;
	FDelegateHandle CueRoutedHandle;
	TArray<TSharedPtr<FString>> StepDescriptions;

	/** One process holds every machine, so these are shared: the swinger, its swing and the field's place. */
	int32 SwingerId = INDEX_NONE;
	FGameplayAbilitySpecHandle SwingHandle;
	FVector FieldLocation = FVector::ZeroVector;
	uint32 HarvestCountBaseline = 0;
	uint32 TotalOwnerPredictions = 0;
	double ResultTime = -1.0;

	const TCHAR* Describe(const FString& Description)
	{
		TSharedPtr<FString> Stored = MakeShared<FString>(Description);
		const TCHAR* Result = **Stored;
		StepDescriptions.Add(MoveTemp(Stored));
		return Result;
	}

	/** Tool hit cues that played in World: routing also reports the server copies an impact notify absorbed. */
	int32 CountPlayedHits(const UWorld* World) const
	{
		int32 Routed = 0;
		for (const FRoutedCue& Cue : RoutedCues)
		{
			Routed += Cue.World.Get() == World ? 1 : 0;
		}
		return Routed - RpgCombatImpact::GetAbsorbedImpactCountForTests(World);
	}

	/** The swinger's tool swing on this machine and its ability system, or null. */
	URpgGameplayAbility_Harvest* FindSwing(UWorld* World, URpgAbilitySystemComponent*& OutAbilitySystem) const
	{
		const ARpgCharacter* Character = RpgHarvestSwingPIETests::FindCharacterByPlayerId(World, SwingerId);
		OutAbilitySystem = Character ? Character->GetRpgAbilitySystemComponent() : nullptr;
		FGameplayAbilitySpec* Spec = OutAbilitySystem ? OutAbilitySystem->FindAbilitySpecFromHandle(SwingHandle) : nullptr;
		return Spec ? Cast<URpgGameplayAbility_Harvest>(Spec->GetPrimaryInstance()) : nullptr;
	}

	/** Whether the swing on this machine is idle: granted, instanced, not running and without window state. */
	bool IsSwingIdle(UWorld* World) const
	{
		URpgAbilitySystemComponent* AbilitySystem = nullptr;
		const URpgGameplayAbility_Harvest* Swing = FindSwing(World, AbilitySystem);
		const FGameplayAbilitySpec* Spec = AbilitySystem ? AbilitySystem->FindAbilitySpecFromHandle(SwingHandle) : nullptr;
		return Swing && Spec && !Spec->IsActive() && !Swing->HasToolSwingResidualStateForTests();
	}

	BEFORE_EACH()
	{
		using namespace RpgHarvestSwingPIETests;
		StepDescriptions.Reset();
		RoutedCues.Reset();
		SwingerId = INDEX_NONE;
		SwingHandle = FGameplayAbilitySpecHandle();
		TotalOwnerPredictions = 0;
		ResultTime = -1.0;

		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && IsValid(Context.World()))
			{
				TestRunner->AddError(TEXT("Harvest swing PIE automation requires all existing PIE sessions to be stopped first."));
				return;
			}
		}

		URpgDeveloperSettings* DeveloperSettings = GetMutableDefault<URpgDeveloperSettings>();
		OriginalExperienceOverride = DeveloperSettings->ExperienceOverride;
		WorldSaveIsolation.Start();
		Ground.Start();
		FrameTimeLimit.Start(MaxSimulatedFrameTime);
		PrototypeGameModeClass = LoadClass<ARpgGameModeBase>(nullptr, PrototypeGameModeClassPath);
		ASSERT_THAT(IsNotNull(PrototypeGameModeClass));
		DeveloperSettings->ExperienceOverride =
			FPrimaryAssetId(URpgExperienceDefinition::StaticClass()->GetFName(), PrototypeExperienceName);

		// Every tool hit routed on every machine of this one-process session, local predictions included.
		if (UGameplayCueManager* CueManager = UAbilitySystemGlobals::Get().GetGameplayCueManager())
		{
			CueRoutedHandle = CueManager->OnGameplayCueRouted().AddLambda(
				[this](AActor* Target, const FGameplayTag Tag, const EGameplayCueEvent::Type Event,
					const FGameplayCueParameters& Parameters, EGameplayCueExecutionOptions)
				{
					if (Event == EGameplayCueEvent::Executed && Tag == RpgHarvestSwingPIETests::ToolHitCueTag())
					{
						RoutedCues.Add({Target ? Target->GetWorld() : nullptr, FVector(Parameters.Location)});
					}
				});
		}

		PacketSettings = FPacketSimulationSettings();
		PacketSettings.PktLag = 60;
		PacketSettings.PktLagVariance = 10;
		FNetworkComponentBuilder<FNetworkState>()
			.WithClients(1)
			.AsListenServer()
			.WithPacketSimulationSettings(&PacketSettings)
			.WithGameInstanceClass(FSoftClassPath(TEXT("/Game/SurvivalRpg/Core/Game/BP_Rpg_GameInstance.BP_Rpg_GameInstance_C")))
			.WithGameMode(PrototypeGameModeClass)
			.Build(Network);
	}

	AFTER_EACH()
	{
		if (UGameplayCueManager* CueManager = UAbilitySystemGlobals::Get().GetGameplayCueManager())
		{
			CueManager->OnGameplayCueRouted().Remove(CueRoutedHandle);
		}
		GetMutableDefault<URpgDeveloperSettings>()->ExperienceOverride = OriginalExperienceOverride;
	}

	/** The remote client stands in its lane with the axe in its main hand; the resource waits within its swing. */
	void QueueSwingerReady()
	{
		using namespace RpgHarvestSwingPIETests;

		Network
			.ThenServer(TEXT("Listen server uses its own nonpersistent test GameMode instance"),
				[this](FNetworkState& State) { ASSERT_THAT(IsTrue(WorldSaveIsolation.IsWorldIsolated(State.World))); })
			.UntilServer(TEXT("Prototype Experience loads with the client"),
				[](FNetworkState& State) { return IsPrototypeExperienceReady(State.World, 1); },
				NetworkTimeout())
			.UntilClient(TEXT("The client owns a grounded Prototype pawn"), 0,
				[](FNetworkState& State)
				{
					const ARpgCharacter* Character = FindLocalCharacter(State.World);
					return IsPrototypeExperienceReady(State.World, 1) && IsPrototypeCharacterReady(Character) &&
						Character->GetLocalRole() == ROLE_AutonomousProxy && Character->GetCharacterMovement()->IsMovingOnGround();
				},
				NetworkTimeout())
			.ThenClient(TEXT("The client is the swinger and looks along its lane"), 0,
				[this](FNetworkState& State)
				{
					ARpgCharacter* Character = FindLocalCharacter(State.World);
					ASSERT_THAT(IsTrue(Character && Character->GetPlayerState()));
					SwingerId = Character->GetPlayerState()->GetPlayerId();
					if (APlayerController* PlayerController = State.World->GetFirstPlayerController())
					{
						PlayerController->SetControlRotation(FRotator::ZeroRotator);
					}
				})
			.ThenServer(TEXT("Place the swinger in its lane and the host aside, and equip the axe"),
				[this](FNetworkState& State)
				{
					TSubclassOf<URpgEquipmentDefinition> AxeDefinition = LoadClass<URpgEquipmentDefinition>(nullptr, AxeDefinitionClassPath);
					const URpgAbilitySet* AxeAbilities = LoadObject<URpgAbilitySet>(nullptr, AxeAbilitySetPath);
					ASSERT_THAT(IsTrue(AxeDefinition && AxeAbilities));
					for (TActorIterator<ARpgCharacter> It(State.World); It; ++It)
					{
						ARpgCharacter* Character = *It;
						const bool bSwinger = Character->GetPlayerState() && Character->GetPlayerState()->GetPlayerId() == SwingerId;
						Character->TeleportTo(bSwinger ? SwingerLocation : HostLocation, FRotator::ZeroRotator);
						Character->GetCharacterMovement()->StopMovementImmediately();
						Character->ForceNetUpdate();
						if (!bSwinger)
						{
							continue;
						}
						// The axe's ability set comes with the item; the test grants it from the equipped axe directly.
						URpgEquipmentManagerComponent* EquipmentManager = Character->FindComponentByClass<URpgEquipmentManagerComponent>();
						URpgAbilitySystemComponent* AbilitySystem = Character->GetRpgAbilitySystemComponent();
						ASSERT_THAT(IsTrue(EquipmentManager && AbilitySystem));
						EquipmentManager->UnequipItemInSlot(ERpgEquipmentSlot::MainHand);
						URpgEquipmentInstance* Axe = EquipmentManager->EquipItemInSlot(AxeDefinition, ERpgEquipmentSlot::MainHand);
						ASSERT_THAT(IsNotNull(Axe));
						FRpgAbilitySet_GrantedHandles Granted;
						AxeAbilities->GiveToAbilitySystem(AbilitySystem, &Granted, Axe);
						for (const FGameplayAbilitySpec& Spec : AbilitySystem->GetActivatableAbilities())
						{
							const URpgGameplayAbility_Harvest* Swing = Cast<URpgGameplayAbility_Harvest>(Spec.Ability);
							if (Swing && Swing->UsesToolSwing() && Spec.SourceObject.Get() == Axe)
							{
								SwingHandle = Spec.Handle;
							}
						}
						ASSERT_THAT(IsTrue(SwingHandle.IsValid()));
					}
				})
			.ThenClient(TEXT("The swinger keeps looking along its lane after the teleport"), 0,
				[](FNetworkState& State)
				{
					if (APlayerController* PlayerController = State.World->GetFirstPlayerController())
					{
						PlayerController->SetControlRotation(FRotator::ZeroRotator);
					}
				})
			.UntilServer(TEXT("The swinger stands grounded in its lane with the axe in hand"),
				[this](FNetworkState& State)
				{
					const ARpgCharacter* Character = FindCharacterByPlayerId(State.World, SwingerId);
					const URpgEquipmentManagerComponent* EquipmentManager =
						Character ? Character->FindComponentByClass<URpgEquipmentManagerComponent>() : nullptr;
					const URpgEquipmentInstance* Axe = EquipmentManager ? EquipmentManager->GetEquipmentInstanceInSlot(ERpgEquipmentSlot::MainHand) : nullptr;
					return Character && Axe && !Axe->GetSpawnedActors().IsEmpty() &&
						FVector::Dist2D(Character->GetActorLocation(), SwingerLocation) < 5.0 &&
						Character->GetCharacterMovement()->IsMovingOnGround() && IsSwingIdle(State.World);
				},
				NetworkTimeout())
			.ThenServer(TEXT("The server loads the field where the axe strikes, about 1.1 m ahead at hip height"),
				[this](FNetworkState& State)
				{
					const ARpgCharacter* Character = FindCharacterByPlayerId(State.World, SwingerId);
					ASSERT_THAT(IsNotNull(Character));
					ASSERT_THAT(IsNotNull(URpgHarvestInstanceStockComponent::FindForWorld(State.World)));
					const double FeetZ = Character->GetActorLocation().Z - Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
					FieldLocation = FVector(Character->GetActorLocation().X + 115.0, Character->GetActorLocation().Y, FeetZ + 115.0);
					State.Field = LoadField(State.World, FieldLocation);
					ASSERT_THAT(IsNotNull(State.Field));
					ASSERT_THAT(AreEqual(FieldSections, GetRemainingSections(State)));
				})
			.UntilClient(TEXT("The client has the replicated stock, the axe and its swing"), 0,
				[this](FNetworkState& State)
				{
					const ARpgCharacter* Character = FindLocalCharacter(State.World);
					const URpgEquipmentManagerComponent* EquipmentManager =
						Character ? Character->FindComponentByClass<URpgEquipmentManagerComponent>() : nullptr;
					const URpgEquipmentInstance* Axe = EquipmentManager ? EquipmentManager->GetEquipmentInstanceInSlot(ERpgEquipmentSlot::MainHand) : nullptr;
					return URpgHarvestInstanceStockComponent::FindForWorld(State.World) && Axe && !Axe->GetSpawnedActors().IsEmpty() &&
						IsSwingIdle(State.World) && Character->GetCharacterMovement()->IsMovingOnGround();
				},
				NetworkTimeout())
			.ThenClient(TEXT("The client loads the same field"), 0,
				[this](FNetworkState& State)
				{
					State.Field = LoadField(State.World, FieldLocation);
					ASSERT_THAT(IsNotNull(State.Field));
					ASSERT_THAT(AreEqual(FieldSections, GetRemainingSections(State)));
				});
	}

	/** The swinger swings once; ExpectedRemaining sections stay, and each machine has played ExpectedHits tool hits. */
	void QueueSwing(const int32 Round, const int32 ExpectedRemaining, const TArray<int32>& ExpectedClientHits)
	{
		using namespace RpgHarvestSwingPIETests;

		Network
			.ThenServer(Describe(FString::Printf(TEXT("Round %d: record the server's harvest count"), Round)),
				[this](FNetworkState& State)
				{
					URpgAbilitySystemComponent* AbilitySystem = nullptr;
					const URpgGameplayAbility_Harvest* Swing = FindSwing(State.World, AbilitySystem);
					ASSERT_THAT(IsNotNull(Swing));
					HarvestCountBaseline = Swing ? Swing->GetAuthorityToolSwingHarvestCountForTests() : 0;
					ResultTime = -1.0;
				})
			.ThenClient(Describe(FString::Printf(TEXT("Round %d: the swinger swings its axe"), Round)), 0,
				[this](FNetworkState& State)
				{
					URpgAbilitySystemComponent* AbilitySystem = nullptr;
					ASSERT_THAT(IsNotNull(FindSwing(State.World, AbilitySystem)));
					ASSERT_THAT(IsTrue(AbilitySystem->TryActivateAbility(SwingHandle)));
				})
			.UntilServer(Describe(FString::Printf(TEXT("Round %d: the server's swing took one section and ended"), Round)),
				[this, ExpectedRemaining](FNetworkState& State)
				{
					URpgAbilitySystemComponent* AbilitySystem = nullptr;
					const URpgGameplayAbility_Harvest* Swing = FindSwing(State.World, AbilitySystem);
					if (!Swing || GetRemainingSections(State) != ExpectedRemaining || !IsSwingIdle(State.World))
					{
						return false;
					}
					ResultTime = State.World->GetTimeSeconds();
					return true;
				},
				NetworkTimeout())
			.UntilServer(Describe(FString::Printf(TEXT("Round %d: late commits and cue copies had time to arrive"), Round)),
				[this](FNetworkState& State) { return State.World->GetTimeSeconds() - ResultTime >= QuietSeconds; },
				NetworkTimeout())
			.ThenServer(Describe(FString::Printf(TEXT("Round %d: one commit, and the host played its hit once"), Round)),
				[this, ExpectedRemaining](FNetworkState& State)
				{
					URpgAbilitySystemComponent* AbilitySystem = nullptr;
					const URpgGameplayAbility_Harvest* Swing = FindSwing(State.World, AbilitySystem);
					ASSERT_THAT(IsNotNull(Swing));
					ASSERT_THAT(AreEqual(ExpectedRemaining, GetRemainingSections(State)));
					ASSERT_THAT(AreEqual(HarvestCountBaseline + 1, Swing->GetAuthorityToolSwingHarvestCountForTests()));
					ASSERT_THAT(AreEqual(FieldSections - ExpectedRemaining, CountPlayedHits(State.World)));
					ASSERT_THAT(AreEqual(0, RpgCombatImpact::GetAbsorbedImpactCountForTests(State.World)));
				});

		for (int32 ClientIndex = 0; ClientIndex < ExpectedClientHits.Num(); ++ClientIndex)
		{
			const int32 ExpectedHits = ExpectedClientHits[ClientIndex];
			Network.UntilClient(
				Describe(FString::Printf(TEXT("Round %d: client %d shows the stock and played every hit once"), Round, ClientIndex)),
				ClientIndex,
				[this, ExpectedRemaining, ExpectedHits](FNetworkState& State)
				{
					return GetRemainingSections(State) == ExpectedRemaining && CountPlayedHits(State.World) == ExpectedHits;
				},
				NetworkTimeout());
		}
		Network.ThenClient(Describe(FString::Printf(TEXT("Round %d: the swinger predicted at most its own hit"), Round)), 0,
			[this, Round](FNetworkState& State)
			{
				URpgAbilitySystemComponent* AbilitySystem = nullptr;
				const URpgGameplayAbility_Harvest* Swing = FindSwing(State.World, AbilitySystem);
				ASSERT_THAT(IsNotNull(Swing));
				// Its own hit plays once either way: from its prediction, or late from the server when its own trace missed.
				TotalOwnerPredictions = Swing ? Swing->GetOwnerPredictedToolCueCountForTests() : 0;
				ASSERT_THAT(IsTrue(TotalOwnerPredictions <= static_cast<uint32>(Round + 1)));
			});
	}

	TEST_METHOD(RemoteSwingCommitsOncePlaysOnceEverywhereCancelsAndLateJoins)
	{
		using namespace RpgHarvestSwingPIETests;

		QueueSwingerReady();
		QueueSwing(0, FieldSections - 1, {1});

		// Cancelled before the attack window opens, as by a dodge or a tool switch: nothing is struck.
		Network
			.ThenServer(TEXT("Cancel: record the server's harvest count"),
				[this](FNetworkState& State)
				{
					URpgAbilitySystemComponent* AbilitySystem = nullptr;
					const URpgGameplayAbility_Harvest* Swing = FindSwing(State.World, AbilitySystem);
					ASSERT_THAT(IsNotNull(Swing));
					HarvestCountBaseline = Swing ? Swing->GetAuthorityToolSwingHarvestCountForTests() : 0;
				})
			.ThenClient(TEXT("Cancel: the swinger starts a swing"), 0,
				[this](FNetworkState& State)
				{
					URpgAbilitySystemComponent* AbilitySystem = nullptr;
					ASSERT_THAT(IsNotNull(FindSwing(State.World, AbilitySystem)));
					ASSERT_THAT(IsTrue(AbilitySystem->TryActivateAbility(SwingHandle)));
				})
			.UntilServer(TEXT("Cancel: the server runs the swing"),
				[this](FNetworkState& State)
				{
					URpgAbilitySystemComponent* AbilitySystem = nullptr;
					FindSwing(State.World, AbilitySystem);
					const FGameplayAbilitySpec* Spec = AbilitySystem ? AbilitySystem->FindAbilitySpecFromHandle(SwingHandle) : nullptr;
					return Spec && Spec->IsActive();
				},
				NetworkTimeout())
			.ThenClient(TEXT("Cancel: the swinger cancels before the window opens"), 0,
				[this](FNetworkState& State)
				{
					URpgAbilitySystemComponent* AbilitySystem = nullptr;
					const URpgGameplayAbility_Harvest* Swing = FindSwing(State.World, AbilitySystem);
					ASSERT_THAT(IsNotNull(Swing));
					ASSERT_THAT(AreEqual(0, Swing->GetToolSwingContactCountForTests()));
					AbilitySystem->CancelAbilityHandle(SwingHandle);
				})
			.UntilServer(TEXT("Cancel: the server's swing ended without window state"),
				[this](FNetworkState& State)
				{
					if (!IsSwingIdle(State.World))
					{
						return false;
					}
					ResultTime = State.World->GetTimeSeconds();
					return true;
				},
				NetworkTimeout())
			.UntilServer(TEXT("Cancel: a late commit would have had time to happen"),
				[this](FNetworkState& State) { return State.World->GetTimeSeconds() - ResultTime >= QuietSeconds; },
				NetworkTimeout())
			.ThenServer(TEXT("Cancel: the cancelled swing took nothing and played nothing"),
				[this](FNetworkState& State)
				{
					URpgAbilitySystemComponent* AbilitySystem = nullptr;
					const URpgGameplayAbility_Harvest* Swing = FindSwing(State.World, AbilitySystem);
					ASSERT_THAT(IsNotNull(Swing));
					ASSERT_THAT(AreEqual(FieldSections - 1, GetRemainingSections(State)));
					ASSERT_THAT(AreEqual(HarvestCountBaseline, Swing->GetAuthorityToolSwingHarvestCountForTests()));
					ASSERT_THAT(AreEqual(1, CountPlayedHits(State.World)));
				})
			.ThenClient(TEXT("Cancel: the swinger played no further hit"), 0,
				[this](FNetworkState& State)
				{
					ASSERT_THAT(AreEqual(FieldSections - 1, GetRemainingSections(State)));
					ASSERT_THAT(AreEqual(1, CountPlayedHits(State.World)));
				});

		// A late joiner receives the stock as it is and replays no earlier hit; the next swing plays once there too.
		Network
			.ThenClientJoins(NetworkTimeout())
			.UntilServer(TEXT("Late join establishes the second connection"),
				[](FNetworkState& State) { return IsPrototypeExperienceReady(State.World, 2); },
				NetworkTimeout())
			.UntilClient(TEXT("The late joiner is ready with the changed stock"), 1,
				[](FNetworkState& State)
				{
					const URpgHarvestInstanceStockComponent* Stock = URpgHarvestInstanceStockComponent::FindForWorld(State.World);
					return IsPrototypeExperienceReady(State.World, 2) && IsPrototypeCharacterReady(FindLocalCharacter(State.World)) &&
						Stock && Stock->GetNumChangedInstances() == 1;
				},
				NetworkTimeout())
			.ThenClient(TEXT("The late joiner loads the field, shows its stock and replays no hit"), 1,
				[this](FNetworkState& State)
				{
					State.Field = LoadField(State.World, FieldLocation);
					ASSERT_THAT(IsNotNull(State.Field));
					ASSERT_THAT(AreEqual(FieldSections - 1, GetRemainingSections(State)));
					ASSERT_THAT(AreEqual(0, CountPlayedHits(State.World)));
				})
			.UntilClient(TEXT("The swinger can swing again"), 0,
				[this](FNetworkState& State) { return IsSwingIdle(State.World); },
				NetworkTimeout());
		QueueSwing(1, FieldSections - 2, {2, 1});
		Network.ThenServer(TEXT("Report the swinger's predictions"),
			[this](FNetworkState& State)
			{
				(void)State;
				TestRunner->AddInfo(FString::Printf(TEXT("Owner predictions: %u of 2 hits"), TotalOwnerPredictions));
			});
	}
};

#endif // ENABLE_PIE_NETWORK_TEST
#endif // WITH_DEV_AUTOMATION_TESTS
