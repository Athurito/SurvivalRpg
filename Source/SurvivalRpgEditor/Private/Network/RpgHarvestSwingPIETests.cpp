#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"
#include "Network/RpgCombatNetworkTestTypes.h"
#include "Network/RpgLootHarvestNetworkTestTypes.h"

#include "AbilitySystem/Abilities/RpgGameplayAbility_Harvest.h"
#include "AbilitySystemGlobals.h"
#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
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
#include "Harvesting/RpgHarvestTargetingComponent.h"
#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility_Dodge.h"
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
	constexpr int32 FieldSections = 6;

	/** Swings the hold step lets the held input repeat before the swinger dodges. */
	constexpr int32 HeldSwings = 3;

	/** Montage position in seconds before which a swing is still winding up; the axe's window opens at 0.367 s. */
	constexpr float WindupPositionSeconds = 0.2f;

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

	const FGameplayTag& PrimaryInputTag()
	{
		static const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TEXT("InputTag.Weapon.Primary"));
		return Tag;
	}

	const FGameplayTag& DodgeInputTag()
	{
		static const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TEXT("InputTag.Ability.Dodge"));
		return Tag;
	}

	/** The dodge the Prototype Experience grants on the dodge input, or an invalid handle. */
	FGameplayAbilitySpecHandle FindDodgeHandle(URpgAbilitySystemComponent* AbilitySystem)
	{
		if (AbilitySystem)
		{
			for (const FGameplayAbilitySpec& Spec : AbilitySystem->GetActivatableAbilities())
			{
				if (Spec.Ability && Spec.Ability->IsA<URpgGameplayAbility_Dodge>() &&
					Spec.GetDynamicSpecSourceTags().HasTagExact(DodgeInputTag()))
				{
					return Spec.Handle;
				}
			}
		}
		return FGameplayAbilitySpecHandle();
	}

	/** Position of the montage Character plays, or a negative value when it plays none. */
	float GetMontagePosition(const ARpgCharacter* Character)
	{
		const USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
		const UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
		const UAnimMontage* Montage = AnimInstance ? AnimInstance->GetCurrentActiveMontage() : nullptr;
		return Montage ? AnimInstance->Montage_GetPosition(Montage) : -1.0f;
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

	/** Hold step: the swinger's dodge, its client's swing count at the press, and the server's swing log. */
	FGameplayAbilitySpecHandle DodgeHandle;
	uint32 ClientSwingBaseline = 0;
	FVector WalkStart = FVector::ZeroVector;
	double FastestSwingingSpeed = 0.0;
	TArray<double> ServerSwingStartTimes;
	int32 ServerSwingCancelledEnds = 0;
	TWeakObjectPtr<URpgAbilitySystemComponent> ServerSwingSystem;
	FDelegateHandle ServerSwingActivatedHandle;
	FDelegateHandle ServerSwingEndedHandle;

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

	/** Server: records when each of the swinger's swings starts and how many were cancelled, as they happen. */
	void StartServerSwingLog(URpgAbilitySystemComponent* AbilitySystem)
	{
		StopServerSwingLog();
		ServerSwingStartTimes.Reset();
		ServerSwingCancelledEnds = 0;
		ServerSwingSystem = AbilitySystem;
		ServerSwingActivatedHandle = AbilitySystem->AbilityActivatedCallbacks.AddLambda([this](UGameplayAbility* Ability)
		{
			const UWorld* World = Ability ? Ability->GetWorld() : nullptr;
			if (World && Ability->IsInstantiated() && Ability->GetCurrentAbilitySpecHandle() == SwingHandle)
			{
				ServerSwingStartTimes.Add(World->GetTimeSeconds());
			}
		});
		ServerSwingEndedHandle = AbilitySystem->OnAbilityEnded.AddLambda([this](const FAbilityEndedData& Data)
		{
			ServerSwingCancelledEnds += Data.AbilitySpecHandle == SwingHandle && Data.bWasCancelled ? 1 : 0;
		});
	}

	void StopServerSwingLog()
	{
		if (URpgAbilitySystemComponent* AbilitySystem = ServerSwingSystem.Get())
		{
			AbilitySystem->AbilityActivatedCallbacks.Remove(ServerSwingActivatedHandle);
			AbilitySystem->OnAbilityEnded.Remove(ServerSwingEndedHandle);
		}
		ServerSwingSystem.Reset();
		ServerSwingActivatedHandle.Reset();
		ServerSwingEndedHandle.Reset();
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
		StopServerSwingLog();
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
						URpgEquipmentManagerComponent* EquipmentManager = Character->FindComponentByClass<URpgEquipmentManagerComponent>();
						URpgAbilitySystemComponent* AbilitySystem = Character->GetRpgAbilitySystemComponent();
						ASSERT_THAT(IsTrue(EquipmentManager && AbilitySystem));
						EquipmentManager->UnequipItemInSlot(ERpgEquipmentSlot::MainHand);
						URpgEquipmentInstance* Axe = EquipmentManager->EquipItemInSlot(AxeDefinition, ERpgEquipmentSlot::MainHand);
						ASSERT_THAT(IsNotNull(Axe));
						auto FindAxeSwings = [AbilitySystem, Axe]()
						{
							TArray<FGameplayAbilitySpecHandle> Handles;
							for (const FGameplayAbilitySpec& Spec : AbilitySystem->GetActivatableAbilities())
							{
								const URpgGameplayAbility_Harvest* Swing = Cast<URpgGameplayAbility_Harvest>(Spec.Ability);
								if (Swing && Swing->UsesToolSwing() && Spec.SourceObject.Get() == Axe)
								{
									Handles.Add(Spec.Handle);
								}
							}
							return Handles;
						};
						// The axe's equipment grants its ability set with the main-hand slot; grant it directly only where it did not.
						TArray<FGameplayAbilitySpecHandle> AxeSwings = FindAxeSwings();
						if (AxeSwings.IsEmpty())
						{
							FRpgAbilitySet_GrantedHandles Granted;
							AxeAbilities->GiveToAbilitySystem(AbilitySystem, &Granted, Axe);
							AxeSwings = FindAxeSwings();
						}
						// A second swing on the primary input would replace the first one whenever a press activates both.
						ASSERT_THAT(AreEqual(1, AxeSwings.Num()));
						SwingHandle = AxeSwings[0];
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

	TEST_METHOD(RemoteHoldSwingsOnceEachUntilDodgeAndPressesAgain)
	{
		using namespace RpgHarvestSwingPIETests;

		QueueSwingerReady();
		Network
			.ThenServer(TEXT("Hold: the server logs the swinger's swings"),
				[this](FNetworkState& State)
				{
					URpgAbilitySystemComponent* AbilitySystem = nullptr;
					const URpgGameplayAbility_Harvest* Swing = FindSwing(State.World, AbilitySystem);
					ASSERT_THAT(IsTrue(Swing && AbilitySystem));
					HarvestCountBaseline = Swing->GetAuthorityToolSwingHarvestCountForTests();
					StartServerSwingLog(AbilitySystem);
				})
			.ThenClient(TEXT("Hold: nothing is marked before a hit, and the swinger holds the primary input"), 0,
				[this](FNetworkState& State)
				{
					URpgAbilitySystemComponent* AbilitySystem = nullptr;
					const URpgGameplayAbility_Harvest* Swing = FindSwing(State.World, AbilitySystem);
					ASSERT_THAT(IsTrue(Swing && AbilitySystem));
					const URpgHarvestTargetingComponent* Targeting =
						URpgHarvestTargetingComponent::FindForController(State.World->GetFirstPlayerController());
					ASSERT_THAT(IsNotNull(Targeting));
					// The resource is right ahead, where the view looks, yet a tool swing aims at nothing.
					ASSERT_THAT(AreEqual(0, Targeting->GetCurrentPreview().Targets.Num()));
					DodgeHandle = FindDodgeHandle(AbilitySystem);
					ASSERT_THAT(IsTrue(DodgeHandle.IsValid()));
					ClientSwingBaseline = Swing->GetToolSwingStartCountForTests();
					// Held from here on: the player controller processes the held input every frame.
					AbilitySystem->AbilityInputTagPressed(PrimaryInputTag());
				})
			.UntilServer(Describe(FString::Printf(TEXT("Hold: the held input swung %d times, each taking one section"), HeldSwings)),
				[this](FNetworkState& State)
				{
					URpgAbilitySystemComponent* AbilitySystem = nullptr;
					const URpgGameplayAbility_Harvest* Swing = FindSwing(State.World, AbilitySystem);
					return Swing && Swing->GetAuthorityToolSwingHarvestCountForTests() >= HarvestCountBaseline + HeldSwings &&
						GetRemainingSections(State) == FieldSections - HeldSwings;
				},
				NetworkTimeout())
			.UntilClient(TEXT("Hold: the next held swing winds up on the client"), 0,
				[this](FNetworkState& State)
				{
					URpgAbilitySystemComponent* AbilitySystem = nullptr;
					const URpgGameplayAbility_Harvest* Swing = FindSwing(State.World, AbilitySystem);
					const float Position = GetMontagePosition(FindLocalCharacter(State.World));
					return Swing && Swing->IsActive() &&
						Swing->GetToolSwingStartCountForTests() >= ClientSwingBaseline + HeldSwings + 1 &&
						Position >= 0.0f && Position < WindupPositionSeconds;
				},
				NetworkTimeout())
			.ThenClient(TEXT("Hold: the stock indicator shows the struck resource, and the swinger dodges while holding"), 0,
				[this](FNetworkState& State)
				{
					URpgAbilitySystemComponent* AbilitySystem = nullptr;
					const URpgGameplayAbility_Harvest* Swing = FindSwing(State.World, AbilitySystem);
					ASSERT_THAT(IsTrue(Swing && AbilitySystem));
					TotalOwnerPredictions = Swing->GetOwnerPredictedToolCueCountForTests();
					const URpgHarvestTargetingComponent* Targeting =
						URpgHarvestTargetingComponent::FindForController(State.World->GetFirstPlayerController());
					ASSERT_THAT(IsNotNull(Targeting));
					// The last predicted hit is less than the linger time ago; the stock shown is the replicated one.
					if (TotalOwnerPredictions > 0)
					{
						ERpgHarvestTargetStatus Status = ERpgHarvestTargetStatus::None;
						int32 Remaining = 0;
						int32 SectionCount = 0;
						int32 ToTake = 0;
						ASSERT_THAT(IsTrue(Targeting->IsPreviewStruckTarget()));
						ASSERT_THAT(IsTrue(Targeting->GetPrimaryTargetStatus(Status, Remaining, SectionCount, ToTake)));
						ASSERT_THAT(AreEqual(FieldSections - HeldSwings, Remaining));
						ASSERT_THAT(AreEqual(FieldSections, SectionCount));
					}
					// The dodge replaces the swing during its windup, before the window opens.
					ASSERT_THAT(IsTrue(AbilitySystem->TryActivateAbility(DodgeHandle)));
					ASSERT_THAT(IsFalse(Swing->IsActive()));
				})
			.UntilServer(TEXT("Hold: the server's dodge replaced its swing"),
				[this](FNetworkState& State)
				{
					URpgAbilitySystemComponent* AbilitySystem = nullptr;
					FindSwing(State.World, AbilitySystem);
					const FGameplayAbilitySpec* Spec = AbilitySystem ? AbilitySystem->FindAbilitySpecFromHandle(SwingHandle) : nullptr;
					if (!Spec || Spec->IsActive() || ServerSwingCancelledEnds < 1)
					{
						return false;
					}
					ResultTime = State.World->GetTimeSeconds();
					return true;
				},
				NetworkTimeout())
			.UntilServer(TEXT("Hold: the input stays held for longer than a swing"),
				[this](FNetworkState& State) { return State.World->GetTimeSeconds() - ResultTime >= 2.0 * QuietSeconds; },
				NetworkTimeout())
			.ThenServer(TEXT("Hold: every held swing committed once, at the montage's pace, and the dodged one took nothing"),
				[this](FNetworkState& State)
				{
					URpgAbilitySystemComponent* AbilitySystem = nullptr;
					const URpgGameplayAbility_Harvest* Swing = FindSwing(State.World, AbilitySystem);
					ASSERT_THAT(IsNotNull(Swing));
					ASSERT_THAT(AreEqual(HarvestCountBaseline + HeldSwings, Swing->GetAuthorityToolSwingHarvestCountForTests()));
					ASSERT_THAT(AreEqual(FieldSections - HeldSwings, GetRemainingSections(State)));
					ASSERT_THAT(AreEqual(HeldSwings + 1, ServerSwingStartTimes.Num()));
					ASSERT_THAT(AreEqual(1, ServerSwingCancelledEnds));
					ASSERT_THAT(AreEqual(HeldSwings, CountPlayedHits(State.World)));
					// Each held swing followed the last one at its blend-out: never sooner, and without a pause.
					const double Cycle = Swing->GetToolSwingCycleSecondsForTests();
					ASSERT_THAT(IsTrue(Cycle > 1.0));
					for (int32 Index = 1; Index < ServerSwingStartTimes.Num(); ++Index)
					{
						const double Interval = ServerSwingStartTimes[Index] - ServerSwingStartTimes[Index - 1];
						TestRunner->AddInfo(FString::Printf(TEXT("Held swing %d started %.3f s after the one before (cycle %.3f s)"), Index, Interval, Cycle));
						ASSERT_THAT(IsTrue(Interval >= Cycle - URpgGameplayAbility_Harvest::ToolSwingCadenceToleranceSeconds));
						ASSERT_THAT(IsTrue(Interval <= Cycle + 0.35));
					}
				})
			.UntilClient(TEXT("Hold: the client stays idle while holding and played every hit once"), 0,
				[this](FNetworkState& State)
				{
					return IsSwingIdle(State.World) && GetRemainingSections(State) == FieldSections - HeldSwings &&
						CountPlayedHits(State.World) == HeldSwings;
				},
				NetworkTimeout())
			.ThenClient(TEXT("Hold: the dodged hold started no further swing, and the swinger releases"), 0,
				[this](FNetworkState& State)
				{
					URpgAbilitySystemComponent* AbilitySystem = nullptr;
					const URpgGameplayAbility_Harvest* Swing = FindSwing(State.World, AbilitySystem);
					ASSERT_THAT(IsTrue(Swing && AbilitySystem));
					ASSERT_THAT(AreEqual(ClientSwingBaseline + HeldSwings + 1, Swing->GetToolSwingStartCountForTests()));
					AbilitySystem->AbilityInputTagReleased(PrimaryInputTag());
				})
			.ThenServer(TEXT("Hold: the swinger returns to its lane after the dodge"),
				[this](FNetworkState& State)
				{
					ARpgCharacter* Character = FindCharacterByPlayerId(State.World, SwingerId);
					ASSERT_THAT(IsNotNull(Character));
					Character->TeleportTo(SwingerLocation, FRotator::ZeroRotator);
					Character->GetCharacterMovement()->StopMovementImmediately();
					Character->ForceNetUpdate();
				})
			.ThenClient(TEXT("Hold: the swinger looks along its lane again"), 0,
				[](FNetworkState& State)
				{
					if (APlayerController* PlayerController = State.World->GetFirstPlayerController())
					{
						PlayerController->SetControlRotation(FRotator::ZeroRotator);
					}
				})
			.UntilServer(TEXT("Hold: the swinger stands in its lane with its dodge over"),
				[this](FNetworkState& State)
				{
					const ARpgCharacter* Character = FindCharacterByPlayerId(State.World, SwingerId);
					const URpgAbilitySystemComponent* AbilitySystem = Character ? Character->GetRpgAbilitySystemComponent() : nullptr;
					const FGameplayAbilitySpec* Dodge = AbilitySystem ? AbilitySystem->FindAbilitySpecFromHandle(DodgeHandle) : nullptr;
					return Character && Dodge && !Dodge->IsActive() && IsSwingIdle(State.World) &&
						FVector::Dist2D(Character->GetActorLocation(), SwingerLocation) < 5.0 &&
						Character->GetCharacterMovement()->IsMovingOnGround();
				},
				NetworkTimeout())
			.UntilClient(TEXT("Hold: the client stands in its lane, ready to swing"), 0,
				[this](FNetworkState& State)
				{
					const ARpgCharacter* Character = FindLocalCharacter(State.World);
					URpgAbilitySystemComponent* AbilitySystem = Character ? Character->GetRpgAbilitySystemComponent() : nullptr;
					const FGameplayAbilitySpec* Dodge = AbilitySystem ? AbilitySystem->FindAbilitySpecFromHandle(DodgeHandle) : nullptr;
					return Character && Dodge && !Dodge->IsActive() && IsSwingIdle(State.World) &&
						FVector::Dist2D(Character->GetActorLocation(), SwingerLocation) < 5.0 &&
						Character->GetCharacterMovement()->IsMovingOnGround();
				},
				NetworkTimeout())
			.ThenClient(TEXT("Hold: a fresh press swings again"), 0,
				[this](FNetworkState& State)
				{
					URpgAbilitySystemComponent* AbilitySystem = nullptr;
					ASSERT_THAT(IsNotNull(FindSwing(State.World, AbilitySystem)));
					if (APlayerController* PlayerController = State.World->GetFirstPlayerController())
					{
						PlayerController->SetControlRotation(FRotator::ZeroRotator);
					}
					AbilitySystem->AbilityInputTagPressed(PrimaryInputTag());
				})
			.UntilClient(TEXT("Hold: the pressed swing runs, and the swinger lets go"), 0,
				[this](FNetworkState& State)
				{
					URpgAbilitySystemComponent* AbilitySystem = nullptr;
					const URpgGameplayAbility_Harvest* Swing = FindSwing(State.World, AbilitySystem);
					if (!Swing || !Swing->IsActive())
					{
						return false;
					}
					AbilitySystem->AbilityInputTagReleased(PrimaryInputTag());
					return true;
				},
				NetworkTimeout())
			.UntilServer(TEXT("Hold: the pressed swing took one section and ended"),
				[this](FNetworkState& State)
				{
					URpgAbilitySystemComponent* AbilitySystem = nullptr;
					const URpgGameplayAbility_Harvest* Swing = FindSwing(State.World, AbilitySystem);
					if (!Swing || GetRemainingSections(State) != FieldSections - HeldSwings - 1 || !IsSwingIdle(State.World))
					{
						return false;
					}
					ResultTime = State.World->GetTimeSeconds();
					return true;
				},
				NetworkTimeout())
			.UntilServer(TEXT("Hold: a further swing would have had time to start"),
				[this](FNetworkState& State) { return State.World->GetTimeSeconds() - ResultTime >= QuietSeconds; },
				NetworkTimeout())
			.ThenServer(TEXT("Hold: the press swung once and committed once"),
				[this](FNetworkState& State)
				{
					URpgAbilitySystemComponent* AbilitySystem = nullptr;
					const URpgGameplayAbility_Harvest* Swing = FindSwing(State.World, AbilitySystem);
					ASSERT_THAT(IsNotNull(Swing));
					ASSERT_THAT(AreEqual(HarvestCountBaseline + HeldSwings + 1, Swing->GetAuthorityToolSwingHarvestCountForTests()));
					ASSERT_THAT(AreEqual(FieldSections - HeldSwings - 1, GetRemainingSections(State)));
					ASSERT_THAT(AreEqual(HeldSwings + 2, ServerSwingStartTimes.Num()));
					ASSERT_THAT(AreEqual(HeldSwings + 1, CountPlayedHits(State.World)));
					ASSERT_THAT(AreEqual(0, RpgCombatImpact::GetAbsorbedImpactCountForTests(State.World)));
				})
			.UntilClient(TEXT("Hold: the client shows the stock and played every hit once"), 0,
				[this](FNetworkState& State)
				{
					return IsSwingIdle(State.World) && GetRemainingSections(State) == FieldSections - HeldSwings - 1 &&
						CountPlayedHits(State.World) == HeldSwings + 1;
				},
				NetworkTimeout())
			.ThenClient(TEXT("Hold: report the swinger's predictions"), 0,
				[this](FNetworkState& State)
				{
					URpgAbilitySystemComponent* AbilitySystem = nullptr;
					const URpgGameplayAbility_Harvest* Swing = FindSwing(State.World, AbilitySystem);
					ASSERT_THAT(IsNotNull(Swing));
					TotalOwnerPredictions = Swing->GetOwnerPredictedToolCueCountForTests();
					ASSERT_THAT(IsTrue(TotalOwnerPredictions <= static_cast<uint32>(HeldSwings + 1)));
					TestRunner->AddInfo(FString::Printf(TEXT("Owner predictions: %u of %d hits"), TotalOwnerPredictions, HeldSwings + 1));
				});

		// Movement stays free: the swinger walks away from the field while holding, and the swings keep repeating.
		Network
			.ThenClient(TEXT("Walk: the swinger holds the input again and starts walking"), 0,
				[this](FNetworkState& State)
				{
					URpgAbilitySystemComponent* AbilitySystem = nullptr;
					const URpgGameplayAbility_Harvest* Swing = FindSwing(State.World, AbilitySystem);
					const ARpgCharacter* Character = FindLocalCharacter(State.World);
					ASSERT_THAT(IsTrue(Swing && AbilitySystem && Character));
					ClientSwingBaseline = Swing->GetToolSwingStartCountForTests();
					WalkStart = Character->GetActorLocation();
					FastestSwingingSpeed = 0.0;
					AbilitySystem->AbilityInputTagPressed(PrimaryInputTag());
				})
			.UntilClient(TEXT("Walk: two swings ran while the swinger walked at speed"), 0,
				[this](FNetworkState& State)
				{
					URpgAbilitySystemComponent* AbilitySystem = nullptr;
					const URpgGameplayAbility_Harvest* Swing = FindSwing(State.World, AbilitySystem);
					ARpgCharacter* Character = FindLocalCharacter(State.World);
					if (!Swing || !Character)
					{
						return false;
					}
					Character->AddMovementInput(FVector(-1.0, 0.0, 0.0), 1.0f);
					if (Swing->IsActive())
					{
						FastestSwingingSpeed = FMath::Max(FastestSwingingSpeed, Character->GetVelocity().Size2D());
					}
					return Swing->GetToolSwingStartCountForTests() >= ClientSwingBaseline + 2 && FastestSwingingSpeed > 150.0 &&
						FVector::Dist2D(Character->GetActorLocation(), WalkStart) > 150.0;
				},
				NetworkTimeout())
			.ThenClient(TEXT("Walk: the swinger lets go"), 0,
				[this](FNetworkState& State)
				{
					URpgAbilitySystemComponent* AbilitySystem = nullptr;
					ASSERT_THAT(IsNotNull(FindSwing(State.World, AbilitySystem)));
					AbilitySystem->AbilityInputTagReleased(PrimaryInputTag());
					TestRunner->AddInfo(FString::Printf(TEXT("Fastest ground speed while swinging: %.0f cm/s"), FastestSwingingSpeed));
				})
			.UntilServer(TEXT("Walk: the server ran the walking swings and moved the swinger"),
				[this](FNetworkState& State)
				{
					const ARpgCharacter* Character = FindCharacterByPlayerId(State.World, SwingerId);
					return Character && IsSwingIdle(State.World) && ServerSwingStartTimes.Num() >= HeldSwings + 4 &&
						FVector::Dist2D(Character->GetActorLocation(), SwingerLocation) > 150.0;
				},
				NetworkTimeout())
			.ThenServer(TEXT("Walk: no swing committed more than once"),
				[this](FNetworkState& State)
				{
					URpgAbilitySystemComponent* AbilitySystem = nullptr;
					const URpgGameplayAbility_Harvest* Swing = FindSwing(State.World, AbilitySystem);
					ASSERT_THAT(IsNotNull(Swing));
					const int32 Harvests = static_cast<int32>(Swing->GetAuthorityToolSwingHarvestCountForTests() - HarvestCountBaseline);
					ASSERT_THAT(IsTrue(Harvests <= ServerSwingStartTimes.Num() - ServerSwingCancelledEnds));
					ASSERT_THAT(AreEqual(FieldSections - Harvests, GetRemainingSections(State)));
				});
	}
};

#endif // ENABLE_PIE_NETWORK_TEST
#endif // WITH_DEV_AUTOMATION_TESTS
