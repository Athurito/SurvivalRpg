#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"
#include "Network/RpgCombatNetworkTestTypes.h"
#include "Network/RpgLootHarvestNetworkTestTypes.h"

#include "AbilitySystem/Abilities/RpgGameplayAbility_Harvest.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/NetDriver.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameplayTags/RpgHarvestingMagicGameplayTags.h"
#include "Harvesting/RpgHarvestableInstancesComponent.h"
#include "Harvesting/RpgHarvestInstanceStockComponent.h"
#include "Harvesting/RpgHarvestProfile.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgFocusSet.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySet.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
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
#include "SurvivalRpg/Mvvm/Character/RpgCharacterStatsViewModel.h"
#include "SurvivalRpg/Mvvm/RpgUiSubsystem.h"

#if ENABLE_PIE_NETWORK_TEST

namespace RpgHarvestFocusPIETests
{
	constexpr TCHAR PrototypeExperienceName[] = TEXT("RpgPrototypeExperience");
	constexpr TCHAR PrototypeGameModeClassPath[] = TEXT("/Game/SurvivalRpg/Core/Game/BP_Rpg_GameMode.BP_Rpg_GameMode_C");
	constexpr TCHAR PrototypeCharacterClassPrefix[] = TEXT("/Game/SurvivalRpg/Core/Character/BP_Rpg_Character");
	constexpr TCHAR AxeDefinitionClassPath[] = TEXT("/GF_Harvesting_Magic/Equipment/Tools/ED_Tool_Axe.ED_Tool_Axe_C");
	constexpr TCHAR AxeAbilitySetPath[] = TEXT("/GF_Harvesting_Magic/GAS/AbilitySets/AS_Tool_Axe.AS_Tool_Axe");

	/** A real 60 Hz server, as in the other swing PIE tests. */
	constexpr float MaxSimulatedFrameTime = 1.0f / 60.0f;

	/** Sections of the field's one resource; each landed swing takes one. */
	constexpr int32 FieldSections = 6;

	/** Focus the server gives the host directly, so the client could notice the host's meter if it leaked. */
	constexpr float HostFocus = 30.0f;

	/** Seconds the test waits after a result for late fills or replication. */
	constexpr double QuietSeconds = 1.5;

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

	ARpgCharacter* FindLocalCharacter(const UWorld* World)
	{
		const APlayerController* PlayerController = IsValid(World) ? World->GetFirstPlayerController() : nullptr;
		return PlayerController ? Cast<ARpgCharacter>(PlayerController->GetPawn()) : nullptr;
	}

	/** The character of the player with PlayerId (bOther false), or of any other player (bOther true), in World. */
	ARpgCharacter* FindCharacter(UWorld* World, const int32 PlayerId, const bool bOther = false)
	{
		if (!IsValid(World) || PlayerId == INDEX_NONE)
		{
			return nullptr;
		}
		for (TActorIterator<ARpgCharacter> It(World); It; ++It)
		{
			const APlayerState* PlayerState = It->GetPlayerState();
			if (PlayerState && (PlayerState->GetPlayerId() == PlayerId) != bOther)
			{
				return *It;
			}
		}
		return nullptr;
	}

	/** Focus of Character's player state ability system on this machine, or a negative value without the set. */
	float GetFocus(const ARpgCharacter* Character)
	{
		const URpgAbilitySystemComponent* AbilitySystem = Character ? Character->GetRpgAbilitySystemComponent() : nullptr;
		return AbilitySystem && AbilitySystem->HasAttributeSetForAttribute(URpgFocusSet::GetFocusAttribute())
			? AbilitySystem->GetNumericAttribute(URpgFocusSet::GetFocusAttribute())
			: -1.0f;
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

	/** Loads the field on one machine: one 100 cm cube with the harvestable collision, centered at Location. */
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

	/** Reads a field of the local player's character stats view model by the name its bindings use. */
	template <typename TValue>
	bool ReadStatsField(const UWorld* World, const TCHAR* FieldName, TValue& OutValue)
	{
		const APlayerController* PlayerController = IsValid(World) ? World->GetFirstPlayerController() : nullptr;
		const ULocalPlayer* LocalPlayer = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
		const URpgUiSubsystem* Ui = LocalPlayer ? LocalPlayer->GetSubsystem<URpgUiSubsystem>() : nullptr;
		const URpgCharacterStatsViewModel* Stats = Ui ? Ui->GetCharacterStatsViewModel() : nullptr;
		const FProperty* Property = Stats ? Stats->GetClass()->FindPropertyByName(FieldName) : nullptr;
		if (!Property)
		{
			return false;
		}
		OutValue = *Property->ContainerPtrToValuePtr<TValue>(Stats);
		return true;
	}
}

NETWORK_TEST_CLASS(HarvestFocusPIE, "SurvivalRpg.Network")
{
	using FNetworkState = RpgHarvestFocusPIETests::FNetworkState;

	FRpgScopedPIEWorldSaveIsolation WorldSaveIsolation;
	FRpgScopedNetworkTestGround Ground;
	FRpgScopedNetworkTestFrameTimeLimit FrameTimeLimit;
	FPIENetworkComponent<FNetworkState> Network{TestRunner, TestCommandBuilder, bInitializing};
	FPacketSimulationSettings PacketSettings;
	FPrimaryAssetId OriginalExperienceOverride;
	UClass* PrototypeGameModeClass = nullptr;
	TArray<TSharedPtr<FString>> StepDescriptions;

	/** One process holds every machine, so these are shared: the swinger, its swing, the field and the focus per swing. */
	int32 SwingerId = INDEX_NONE;
	FGameplayAbilitySpecHandle SwingHandle;
	FVector FieldLocation = FVector::ZeroVector;
	float GainPerSwing = 0.0f;
	double ResultTime = -1.0;

	const TCHAR* Describe(const FString& Description)
	{
		TSharedPtr<FString> Stored = MakeShared<FString>(Description);
		const TCHAR* Result = **Stored;
		StepDescriptions.Add(MoveTemp(Stored));
		return Result;
	}

	/** The swinger's tool swing on this machine and its ability system, or null. */
	URpgGameplayAbility_Harvest* FindSwing(UWorld* World, URpgAbilitySystemComponent*& OutAbilitySystem) const
	{
		const ARpgCharacter* Character = RpgHarvestFocusPIETests::FindCharacter(World, SwingerId);
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
		using namespace RpgHarvestFocusPIETests;
		StepDescriptions.Reset();
		SwingerId = INDEX_NONE;
		SwingHandle = FGameplayAbilitySpecHandle();
		GainPerSwing = 0.0f;
		ResultTime = -1.0;

		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && IsValid(Context.World()))
			{
				TestRunner->AddError(TEXT("Harvest focus PIE automation requires all existing PIE sessions to be stopped first."));
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
		GetMutableDefault<URpgDeveloperSettings>()->ExperienceOverride = OriginalExperienceOverride;
	}

	/** The remote client stands in its lane with the axe in its main hand; the resource waits within its swing. */
	void QueueSwingerReady()
	{
		using namespace RpgHarvestFocusPIETests;

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
					const ARpgCharacter* Character = FindCharacter(State.World, SwingerId);
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
					const ARpgCharacter* Character = FindCharacter(State.World, SwingerId);
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

	/** The client swings once; afterwards its focus holds Swings times the gain on the server and on the client. */
	void QueueLandedSwing(const int32 Swings)
	{
		using namespace RpgHarvestFocusPIETests;

		Network
			.ThenClient(Describe(FString::Printf(TEXT("Swing %d: the client swings its axe"), Swings)), 0,
				[this](FNetworkState& State)
				{
					URpgAbilitySystemComponent* AbilitySystem = nullptr;
					ASSERT_THAT(IsNotNull(FindSwing(State.World, AbilitySystem)));
					ASSERT_THAT(IsTrue(AbilitySystem->TryActivateAbility(SwingHandle)));
				})
			.UntilServer(Describe(FString::Printf(TEXT("Swing %d: the server's swing took a section and ended"), Swings)),
				[this, Swings](FNetworkState& State)
				{
					if (GetRemainingSections(State) != FieldSections - Swings || !IsSwingIdle(State.World))
					{
						return false;
					}
					ResultTime = State.World->GetTimeSeconds();
					return true;
				},
				NetworkTimeout())
			.UntilServer(Describe(FString::Printf(TEXT("Swing %d: a late fill had time to arrive"), Swings)),
				[this](FNetworkState& State) { return State.World->GetTimeSeconds() - ResultTime >= QuietSeconds; },
				NetworkTimeout())
			.ThenServer(Describe(FString::Printf(TEXT("Swing %d: the server filled the client's focus once"), Swings)),
				[this, Swings](FNetworkState& State)
				{
					URpgAbilitySystemComponent* AbilitySystem = nullptr;
					const URpgGameplayAbility_Harvest* Swing = FindSwing(State.World, AbilitySystem);
					ASSERT_THAT(IsNotNull(Swing));
					ASSERT_THAT(IsNear(GainPerSwing * Swings, GetFocus(FindCharacter(State.World, SwingerId)), 0.01f));
					ASSERT_THAT(AreEqual(static_cast<uint32>(Swings), Swing->GetAuthorityFocusFillCountForTests()));
					ASSERT_THAT(IsNear(HostFocus, GetFocus(FindCharacter(State.World, SwingerId, true)), 0.01f));
				})
			.UntilClient(Describe(FString::Printf(TEXT("Swing %d: the client's own focus replicated"), Swings)), 0,
				[this, Swings](FNetworkState& State)
				{
					return FMath::IsNearlyEqual(GetFocus(FindLocalCharacter(State.World)), GainPerSwing * Swings, 0.01f);
				},
				NetworkTimeout());
	}

	TEST_METHOD(ClientSwingsFillOnlyItsOwnFocus)
	{
		using namespace RpgHarvestFocusPIETests;

		QueueSwingerReady();
		Network
			.ThenServer(TEXT("The GameFeature gave both player states an empty meter, and the host gains focus directly"),
				[this](FNetworkState& State)
				{
					URpgAbilitySystemComponent* SwingerSystem = nullptr;
					const URpgGameplayAbility_Harvest* Swing = FindSwing(State.World, SwingerSystem);
					ASSERT_THAT(IsNotNull(Swing));
					const ARpgCharacter* Host = FindCharacter(State.World, SwingerId, true);
					URpgAbilitySystemComponent* HostSystem = Host ? Host->GetRpgAbilitySystemComponent() : nullptr;
					ASSERT_THAT(IsNotNull(HostSystem));
					ASSERT_THAT(IsNear(0.0f, GetFocus(FindCharacter(State.World, SwingerId)), 0.01f));
					ASSERT_THAT(IsNear(0.0f, GetFocus(Host), 0.01f));

					// The axe's authored gain, read from the asset rather than frozen here.
					GainPerSwing = Swing->GetFocusGainPerSwing();
					ASSERT_THAT(IsTrue(GainPerSwing > 0.0f && Swing->GetFocusGainEffect() != nullptr));
					const FGameplayEffectSpecHandle HostGain =
						HostSystem->MakeOutgoingSpec(Swing->GetFocusGainEffect(), 1.0f, HostSystem->MakeEffectContext());
					ASSERT_THAT(IsTrue(HostGain.IsValid()));
					HostGain.Data->SetSetByCallerMagnitude(RpgHarvestingMagicGameplayTags::SetByCaller_Harvest_FocusGain, HostFocus);
					HostSystem->ApplyGameplayEffectSpecToSelf(*HostGain.Data);
					ASSERT_THAT(IsNear(HostFocus, GetFocus(Host), 0.01f));
				})
			.UntilClient(TEXT("The client received its own focus set, still empty"), 0,
				[](FNetworkState& State) { return FMath::IsNearlyEqual(GetFocus(FindLocalCharacter(State.World)), 0.0f); },
				NetworkTimeout());

		QueueLandedSwing(1);
		QueueLandedSwing(2);

		Network
			.ThenClient(TEXT("The client's HUD reads its own focus from the stats view model"), 0,
				[this](FNetworkState& State)
				{
					bool bHasFocus = false;
					bool bShowFocus = false;
					float Focus = -1.0f;
					ASSERT_THAT(IsTrue(ReadStatsField(State.World, TEXT("bHasFocus"), bHasFocus) && bHasFocus));
					ASSERT_THAT(IsTrue(ReadStatsField(State.World, TEXT("bShowFocus"), bShowFocus) && bShowFocus));
					ASSERT_THAT(IsTrue(ReadStatsField(State.World, TEXT("Focus"), Focus)));
					ASSERT_THAT(IsNear(GainPerSwing * 2.0f, Focus, 0.01f));
				})
			.ThenClient(TEXT("The host's focus never reaches the client"), 0,
				[this](FNetworkState& State)
				{
					const ARpgCharacter* Host = FindCharacter(State.World, SwingerId, true);
					ASSERT_THAT(IsNotNull(Host));
					const URpgAbilitySystemComponent* HostSystem = Host->GetRpgAbilitySystemComponent();
					ASSERT_THAT(IsNotNull(HostSystem));
					// Without its owner's values the host's set reads its defaults on this client, or the set is absent.
					ASSERT_THAT(IsNear(0.0f, HostSystem->GetNumericAttribute(URpgFocusSet::GetFocusAttribute()), 0.01f));
					TestRunner->AddInfo(FString::Printf(
						TEXT("Client view of the host: focus set %s, focus %.1f of %.1f; the host holds %.1f on the server"),
						HostSystem->HasAttributeSetForAttribute(URpgFocusSet::GetFocusAttribute()) ? TEXT("present") : TEXT("absent"),
						HostSystem->GetNumericAttribute(URpgFocusSet::GetFocusAttribute()),
						HostSystem->GetNumericAttribute(URpgFocusSet::GetMaxFocusAttribute()),
						HostFocus));
				});
	}
};

#endif // ENABLE_PIE_NETWORK_TEST
#endif // WITH_DEV_AUTOMATION_TESTS
