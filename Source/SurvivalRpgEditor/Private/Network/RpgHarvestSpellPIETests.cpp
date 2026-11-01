#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"
#include "Network/RpgCombatNetworkTestTypes.h"
#include "Network/RpgLootHarvestNetworkTestTypes.h"

#include "AbilitySystem/Abilities/RpgGameplayAbility_Harvest.h"
#include "AbilitySystemGlobals.h"
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
#include "GameplayCueNotify_Actor.h"
#include "GameplayEffect.h"
#include "GameplayTags/RpgHarvestingMagicGameplayTags.h"
#include "Harvesting/RpgHarvestableInstancesComponent.h"
#include "Harvesting/RpgHarvestInstanceStockComponent.h"
#include "Harvesting/RpgHarvestProfile.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgFocusSet.h"
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

namespace RpgHarvestSpellPIETests
{
	constexpr TCHAR PrototypeExperienceName[] = TEXT("RpgPrototypeExperience");
	constexpr TCHAR PrototypeGameModeClassPath[] = TEXT("/Game/SurvivalRpg/Core/Game/BP_Rpg_GameMode.BP_Rpg_GameMode_C");
	constexpr TCHAR PrototypeCharacterClassPrefix[] = TEXT("/Game/SurvivalRpg/Core/Character/BP_Rpg_Character");
	constexpr TCHAR AxeDefinitionClassPath[] = TEXT("/GF_Harvesting_Magic/Equipment/Tools/ED_Tool_Axe.ED_Tool_Axe_C");
	constexpr TCHAR AxeAbilitySetPath[] = TEXT("/GF_Harvesting_Magic/GAS/AbilitySets/AS_Tool_Axe.AS_Tool_Axe");
	constexpr TCHAR PickaxeDefinitionClassPath[] = TEXT("/GF_Harvesting_Magic/Equipment/Tools/ED_Tool_Pickaxe.ED_Tool_Pickaxe_C");
	constexpr TCHAR PickaxeAbilitySetPath[] = TEXT("/GF_Harvesting_Magic/GAS/AbilitySets/AS_Tool_Pickaxe.AS_Tool_Pickaxe");
	constexpr TCHAR SpellAbilitySetPath[] = TEXT("/GF_Harvesting_Magic/GAS/AbilitySets/AS_HarvestSpell_WideSwing.AS_HarvestSpell_WideSwing");
	constexpr TCHAR SpellEffectClassPath[] =
		TEXT("/GF_Harvesting_Magic/GAS/Effects/GE_HarvestSpell_WideSwing.GE_HarvestSpell_WideSwing_C");

	/** A real 60 Hz server, as in the other swing PIE tests. */
	constexpr float MaxSimulatedFrameTime = 1.0f / 60.0f;

	/** Sections of each trunk, enough for every swing of the test; profiles allow at most 16. */
	constexpr int32 FieldSections = 16;

	/** The trunk the axe strikes and the one behind it, as instance indices of the field. */
	constexpr int32 FrontTrunk = 0;
	constexpr int32 BehindTrunk = 1;

	/** Centre distance from the front trunk to the one behind it; the 100 cm cubes leave a 25 cm gap. */
	constexpr double BehindOffset = 125.0;

	/** Focus the server gives the caster while the spell runs, so a late joiner could notice it if it leaked. */
	constexpr float CasterFocusDuringSpell = 30.0f;

	/** Seconds before the server's spell ends at which the caster starts swinging without pause across the expiry. */
	constexpr float ExpiryLeadSeconds = 1.5f;

	/** Swings the caster keeps going after the spell ended on both machines. */
	constexpr int32 SwingsAfterExpiry = 2;

	/** Seconds the test waits after a result for late commits or replication. */
	constexpr double QuietSeconds = 1.0;

	const FVector CasterLocation(0.0, 0.0, 100.0);
	const FVector HostLocation(-400.0, 300.0, 100.0);
	const FVector LateJoinerLocation(-400.0, -300.0, 100.0);

	struct FNetworkState : public FBasePIENetworkComponentState
	{
		ARpgNetworkAutomationHarvestInstancesFixture* Field = nullptr;
	};

	FTimespan NetworkTimeout()
	{
		return FTimespan::FromSeconds(90.0);
	}

	const FGameplayTag& SpellActiveTag()
	{
		static const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TEXT("Harvest.Spell.Active"));
		return Tag;
	}

	const FGameplayTag& SpellCueTag()
	{
		static const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.Harvesting.Spell.WideSwing"));
		return Tag;
	}

	const FGameplayTag& SpectralHitCueTag()
	{
		static const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.Harvesting.ToolHit.Spectral"));
		return Tag;
	}

	ARpgCharacter* FindLocalCharacter(const UWorld* World)
	{
		const APlayerController* PlayerController = IsValid(World) ? World->GetFirstPlayerController() : nullptr;
		return PlayerController ? Cast<ARpgCharacter>(PlayerController->GetPawn()) : nullptr;
	}

	/** The character of the player with PlayerId in World, or null. */
	ARpgCharacter* FindCharacter(UWorld* World, const int32 PlayerId)
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

	URpgAbilitySystemComponent* FindAbilitySystem(UWorld* World, const int32 PlayerId)
	{
		const ARpgCharacter* Character = FindCharacter(World, PlayerId);
		return Character ? Character->GetRpgAbilitySystemComponent() : nullptr;
	}

	/** Focus of PlayerId's ability system on this machine, or a negative value without the set. */
	float GetFocus(UWorld* World, const int32 PlayerId)
	{
		const URpgAbilitySystemComponent* AbilitySystem = FindAbilitySystem(World, PlayerId);
		return AbilitySystem && AbilitySystem->HasAttributeSetForAttribute(URpgFocusSet::GetFocusAttribute())
			? AbilitySystem->GetNumericAttribute(URpgFocusSet::GetFocusAttribute())
			: -1.0f;
	}

	/** Active Wide Swing effects of AbilitySystem on this machine, and how many of them this machine predicted. */
	int32 CountSpellEffects(const UAbilitySystemComponent* AbilitySystem, int32* OutPredicted = nullptr)
	{
		const UClass* EffectClass = LoadClass<UGameplayEffect>(nullptr, SpellEffectClassPath);
		int32 Count = 0;
		int32 Predicted = 0;
		if (AbilitySystem && EffectClass)
		{
			for (const FActiveGameplayEffect& Effect : &AbilitySystem->GetActiveGameplayEffects())
			{
				if (Effect.Spec.Def && Effect.Spec.Def->GetClass() == EffectClass)
				{
					++Count;
					Predicted += Effect.PredictionKey.WasLocallyGenerated() ? 1 : 0;
				}
			}
		}
		if (OutPredicted)
		{
			*OutPredicted = Predicted;
		}
		return Count;
	}

	/** Whether the looping Wide Swing cue shows on Character in its world: the cue is active and its notify actor exists. */
	bool ShowsSpellCue(const ARpgCharacter* Character)
	{
		const URpgAbilitySystemComponent* AbilitySystem = Character ? Character->GetRpgAbilitySystemComponent() : nullptr;
		if (!AbilitySystem || !AbilitySystem->IsGameplayCueActive(SpellCueTag()))
		{
			return false;
		}
		for (TActorIterator<AGameplayCueNotify_Actor> It(Character->GetWorld()); It; ++It)
		{
			if (It->GameplayCueTag == SpellCueTag() && It->GetOwner() == Character)
			{
				return true;
			}
		}
		return false;
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

	/** Loads the field on one machine: two 100 cm cubes with the harvestable collision, one behind the other along +X. */
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
		Field->GetHarvestableInstances()->AddInstance(FTransform(FVector(BehindOffset, 0.0, 0.0)));
		Field->FinishSpawning(FTransform(Location));
		return Field;
	}

	int32 GetRemainingSections(const FNetworkState& State, const int32 Trunk)
	{
		return State.Field ? State.Field->GetHarvestableInstances()->GetRemainingSections(Trunk) : INDEX_NONE;
	}
}

NETWORK_TEST_CLASS(HarvestSpellPIE, "SurvivalRpg.Network")
{
	using FNetworkState = RpgHarvestSpellPIETests::FNetworkState;

	FRpgScopedPIEWorldSaveIsolation WorldSaveIsolation;
	FRpgScopedNetworkTestGround Ground;
	FRpgScopedNetworkTestFrameTimeLimit FrameTimeLimit;
	FPIENetworkComponent<FNetworkState> Network{TestRunner, TestCommandBuilder, bInitializing};
	FPacketSimulationSettings PacketSettings;
	FPrimaryAssetId OriginalExperienceOverride;
	UClass* PrototypeGameModeClass = nullptr;
	TArray<TSharedPtr<FString>> StepDescriptions;

	/** One process holds every machine, so these are shared between the server and client steps. */
	int32 CasterId = INDEX_NONE;
	FGameplayAbilitySpecHandle SwingHandle;
	FGameplayAbilitySpecHandle SpellHandle;
	FVector FieldLocation = FVector::ZeroVector;
	TWeakObjectPtr<UWorld> ServerWorld;
	TWeakObjectPtr<UWorld> CasterWorld;
	float GainPerSwing = 0.0f;
	float MaxFocus = 0.0f;
	double ResultTime = -1.0;

	/** Per machine, the SwingTargets each swing of the caster captured, in swing order (index = swing start count - 1). */
	TArray<int32> ServerCaptures;
	TArray<int32> ClientCaptures;
	uint32 ServerCapturedStarts = 0;
	uint32 ClientCapturedStarts = 0;

	/** Overlap of the predicted spell effect and its server copy on the caster's client, sampled every frame. */
	int32 MaxClientSpellEffects = 0;
	int32 OverlapFrames = 0;
	float MaxClientTunedTargets = 0.0f;

	/** Owner-predicted hit cues and server copies the caster absorbed before the first wide swing. */
	uint32 OwnerPredictionBaseline = 0;
	int32 CasterAbsorbedBaseline = 0;
	TMap<TWeakObjectPtr<UWorld>, int32> SpectralHits;
	FDelegateHandle CueRoutedHandle;

	/** What the late joiner saw last of the caster's spell, logged when it changes. */
	FString LateJoinerSeen;

	/** Swings each machine had started when it first saw the spell gone. */
	int32 ServerStartsAtExpiry = INDEX_NONE;
	int32 ClientStartsAtClientExpiry = INDEX_NONE;

	/** Whether the caster swings the pickaxe at a vein-high field instead of the axe at a trunk-high one. */
	bool bPickaxe = false;

	const TCHAR* Describe(const FString& Description)
	{
		TSharedPtr<FString> Stored = MakeShared<FString>(Description);
		const TCHAR* Result = **Stored;
		StepDescriptions.Add(MoveTemp(Stored));
		return Result;
	}

	/** The caster's tool swing on World and its ability system, or null. */
	URpgGameplayAbility_Harvest* FindSwing(UWorld* World, URpgAbilitySystemComponent*& OutAbilitySystem) const
	{
		OutAbilitySystem = RpgHarvestSpellPIETests::FindAbilitySystem(World, CasterId);
		FGameplayAbilitySpec* Spec = OutAbilitySystem ? OutAbilitySystem->FindAbilitySpecFromHandle(SwingHandle) : nullptr;
		return Spec ? Cast<URpgGameplayAbility_Harvest>(Spec->GetPrimaryInstance()) : nullptr;
	}

	/** Whether the caster's swing on World is idle: granted, instanced, not running and without window state. */
	bool IsSwingIdle(UWorld* World) const
	{
		URpgAbilitySystemComponent* AbilitySystem = nullptr;
		const URpgGameplayAbility_Harvest* Swing = FindSwing(World, AbilitySystem);
		const FGameplayAbilitySpec* Spec = AbilitySystem ? AbilitySystem->FindAbilitySpecFromHandle(SwingHandle) : nullptr;
		return Swing && Spec && !Spec->IsActive() && !Swing->HasToolSwingResidualStateForTests();
	}

	/** Records the SwingTargets each new swing captured on World into Captures. */
	void RecordCaptures(UWorld* World, TArray<int32>& Captures, uint32& CapturedStarts) const
	{
		URpgAbilitySystemComponent* AbilitySystem = nullptr;
		const URpgGameplayAbility_Harvest* Swing = FindSwing(World, AbilitySystem);
		if (!Swing || Swing->GetToolSwingStartCountForTests() == CapturedStarts)
		{
			return;
		}
		CapturedStarts = Swing->GetToolSwingStartCountForTests();
		Captures.SetNum(CapturedStarts);
		Captures[CapturedStarts - 1] = Swing->GetLastCapturedSwingTargetsForTests();
	}

	/** The SwingTargets the caster's client tunes for its swing right now, through every source, active effects included. */
	float GetClientTunedTargets(UWorld* World) const
	{
		URpgAbilitySystemComponent* AbilitySystem = nullptr;
		FindSwing(World, AbilitySystem);
		const FGameplayAbilitySpec* Spec = AbilitySystem ? AbilitySystem->FindAbilitySpecFromHandle(SwingHandle) : nullptr;
		return Spec && AbilitySystem->AbilityActorInfo.IsValid()
			? URpgGameplayAbility::GetTunedValueForSpec(
				*Spec, *AbilitySystem->AbilityActorInfo, RpgHarvestingMagicGameplayTags::Ability_Tuning_Harvest_SwingTargets, 1.0f)
			: 0.0f;
	}

	BEFORE_EACH()
	{
		using namespace RpgHarvestSpellPIETests;
		StepDescriptions.Reset();
		CasterId = INDEX_NONE;
		SwingHandle = FGameplayAbilitySpecHandle();
		SpellHandle = FGameplayAbilitySpecHandle();
		ServerWorld.Reset();
		CasterWorld.Reset();
		ServerStartsAtExpiry = INDEX_NONE;
		ClientStartsAtClientExpiry = INDEX_NONE;
		bPickaxe = false;
		ServerCaptures.Reset();
		ClientCaptures.Reset();
		ServerCapturedStarts = 0;
		ClientCapturedStarts = 0;
		MaxClientSpellEffects = 0;
		OverlapFrames = 0;
		MaxClientTunedTargets = 0.0f;
		SpectralHits.Reset();

		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && IsValid(Context.World()))
			{
				TestRunner->AddError(TEXT("Harvest spell PIE automation requires all existing PIE sessions to be stopped first."));
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

		// Every spectral tool hit routed on every machine of this one-process session, local predictions included.
		if (UGameplayCueManager* CueManager = UAbilitySystemGlobals::Get().GetGameplayCueManager())
		{
			CueRoutedHandle = CueManager->OnGameplayCueRouted().AddLambda(
				[this](AActor* Target, const FGameplayTag Tag, const EGameplayCueEvent::Type Event,
					const FGameplayCueParameters& Parameters, EGameplayCueExecutionOptions)
				{
					if (Event == EGameplayCueEvent::Executed && Tag.MatchesTag(RpgHarvestSpellPIETests::SpectralHitCueTag()) && Target)
					{
						++SpectralHits.FindOrAdd(Target->GetWorld());
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

	/** The remote client stands in its lane with the axe in its main hand; two trunks wait in a row within its swing. */
	void QueueCasterReady()
	{
		using namespace RpgHarvestSpellPIETests;

		Network
			.ThenServer(TEXT("Listen server uses its own nonpersistent test GameMode instance"),
				[this](FNetworkState& State)
				{
					ASSERT_THAT(IsTrue(WorldSaveIsolation.IsWorldIsolated(State.World)));
					ServerWorld = State.World;
				})
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
			.ThenClient(TEXT("The client is the caster and looks along its lane"), 0,
				[this](FNetworkState& State)
				{
					ARpgCharacter* Character = FindLocalCharacter(State.World);
					ASSERT_THAT(IsTrue(Character && Character->GetPlayerState()));
					CasterId = Character->GetPlayerState()->GetPlayerId();
					CasterWorld = State.World;
					if (APlayerController* PlayerController = State.World->GetFirstPlayerController())
					{
						PlayerController->SetControlRotation(FRotator::ZeroRotator);
					}
				})
			.ThenServer(TEXT("Place the caster in its lane and the host aside, and equip the axe"),
				[this](FNetworkState& State)
				{
					TSubclassOf<URpgEquipmentDefinition> AxeDefinition =
						LoadClass<URpgEquipmentDefinition>(nullptr, bPickaxe ? PickaxeDefinitionClassPath : AxeDefinitionClassPath);
					const URpgAbilitySet* AxeAbilities = LoadObject<URpgAbilitySet>(nullptr, bPickaxe ? PickaxeAbilitySetPath : AxeAbilitySetPath);
					ASSERT_THAT(IsTrue(AxeDefinition && AxeAbilities));
					for (TActorIterator<ARpgCharacter> It(State.World); It; ++It)
					{
						ARpgCharacter* Character = *It;
						const bool bCaster = Character->GetPlayerState() && Character->GetPlayerState()->GetPlayerId() == CasterId;
						Character->TeleportTo(bCaster ? CasterLocation : HostLocation, FRotator::ZeroRotator);
						Character->GetCharacterMovement()->StopMovementImmediately();
						Character->ForceNetUpdate();
						if (!bCaster)
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
			.ThenClient(TEXT("The caster keeps looking along its lane after the teleport"), 0,
				[](FNetworkState& State)
				{
					if (APlayerController* PlayerController = State.World->GetFirstPlayerController())
					{
						PlayerController->SetControlRotation(FRotator::ZeroRotator);
					}
				})
			.UntilServer(TEXT("The caster stands grounded in its lane with the axe in hand"),
				[this](FNetworkState& State)
				{
					const ARpgCharacter* Character = FindCharacter(State.World, CasterId);
					const URpgEquipmentManagerComponent* EquipmentManager =
						Character ? Character->FindComponentByClass<URpgEquipmentManagerComponent>() : nullptr;
					const URpgEquipmentInstance* Axe = EquipmentManager ? EquipmentManager->GetEquipmentInstanceInSlot(ERpgEquipmentSlot::MainHand) : nullptr;
					return Character && Axe && !Axe->GetSpawnedActors().IsEmpty() &&
						FVector::Dist2D(Character->GetActorLocation(), CasterLocation) < 5.0 &&
						Character->GetCharacterMovement()->IsMovingOnGround() && IsSwingIdle(State.World);
				},
				NetworkTimeout())
			.ThenServer(TEXT("The server loads the field: a trunk where the axe strikes, about 1.1 m ahead at hip height, and one behind it"),
				[this](FNetworkState& State)
				{
					const ARpgCharacter* Character = FindCharacter(State.World, CasterId);
					ASSERT_THAT(IsNotNull(Character));
					ASSERT_THAT(IsNotNull(URpgHarvestInstanceStockComponent::FindForWorld(State.World)));
					const double FeetZ = Character->GetActorLocation().Z - Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
					// The pick comes down into a vein on the ground; the axe strikes a trunk at hip height.
					FieldLocation = bPickaxe
						? FVector(Character->GetActorLocation().X + 110.0, Character->GetActorLocation().Y, FeetZ + 50.0)
						: FVector(Character->GetActorLocation().X + 115.0, Character->GetActorLocation().Y, FeetZ + 115.0);
					State.Field = LoadField(State.World, FieldLocation);
					ASSERT_THAT(IsNotNull(State.Field));
					ASSERT_THAT(AreEqual(FieldSections, GetRemainingSections(State, FrontTrunk)));
					ASSERT_THAT(AreEqual(FieldSections, GetRemainingSections(State, BehindTrunk)));
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
				});
	}

	/** The server teaches the caster Wide Swing as its learned node does and fills its meter to one swing short of full. */
	void QueueSpellLearned()
	{
		using namespace RpgHarvestSpellPIETests;

		Network
			.ThenServer(TEXT("The caster learns Wide Swing and holds a meter one swing short of full"),
				[this](FNetworkState& State)
				{
					URpgAbilitySystemComponent* AbilitySystem = nullptr;
					const URpgGameplayAbility_Harvest* Swing = FindSwing(State.World, AbilitySystem);
					const ARpgCharacter* Character = FindCharacter(State.World, CasterId);
					const URpgEquipmentManagerComponent* EquipmentManager =
						Character ? Character->FindComponentByClass<URpgEquipmentManagerComponent>() : nullptr;
					URpgEquipmentInstance* Axe = EquipmentManager ? EquipmentManager->GetEquipmentInstanceInSlot(ERpgEquipmentSlot::MainHand) : nullptr;
					const URpgAbilitySet* SpellAbilities = LoadObject<URpgAbilitySet>(nullptr, SpellAbilitySetPath);
					ASSERT_THAT(IsTrue(Swing && Axe && SpellAbilities));

					// The node grants its set from the tool in use and Harvest.Spell.Known, which the gain requires and
					// only the server checks.
					FRpgAbilitySet_GrantedHandles Granted;
					SpellAbilities->GiveToAbilitySystem(AbilitySystem, &Granted, Axe);
					AbilitySystem->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("Harvest.Spell.Known")));
					const FGameplayTag SpellId = FGameplayTag::RequestGameplayTag(TEXT("Ability.Harvesting.Spell.WideSwing"));
					for (const FGameplayAbilitySpec& Spec : AbilitySystem->GetActivatableAbilities())
					{
						if (Spec.GetDynamicSpecSourceTags().HasTagExact(SpellId))
						{
							SpellHandle = Spec.Handle;
						}
					}
					ASSERT_THAT(IsTrue(SpellHandle.IsValid()));

					// The axe's authored gain, read from the asset rather than frozen here.
					GainPerSwing = Swing->GetFocusGainPerSwing();
					MaxFocus = AbilitySystem->GetNumericAttribute(URpgFocusSet::GetMaxFocusAttribute());
					ASSERT_THAT(IsTrue(GainPerSwing > 0.0f && MaxFocus > GainPerSwing));
					AbilitySystem->SetNumericAttributeBase(URpgFocusSet::GetFocusAttribute(), MaxFocus - GainPerSwing);
				})
			.UntilClient(TEXT("The client has Wide Swing and its meter one swing short of full"), 0,
				[this](FNetworkState& State)
				{
					const URpgAbilitySystemComponent* AbilitySystem = FindAbilitySystem(State.World, CasterId);
					return AbilitySystem && AbilitySystem->FindAbilitySpecFromHandle(SpellHandle) &&
						FMath::IsNearlyEqual(GetFocus(State.World, CasterId), MaxFocus - GainPerSwing, 0.01f);
				},
				NetworkTimeout())
			.ThenClient(TEXT("A press before the meter is full fails for its cost and predicts nothing"), 0,
				[this](FNetworkState& State)
				{
					URpgAbilitySystemComponent* AbilitySystem = FindAbilitySystem(State.World, CasterId);
					ASSERT_THAT(IsNotNull(AbilitySystem));
					ASSERT_THAT(IsFalse(AbilitySystem->TryActivateAbility(SpellHandle)));
					ASSERT_THAT(AreEqual(0, CountSpellEffects(AbilitySystem)));
				})
			.ThenClient(TEXT("The client swings once more"), 0,
				[this](FNetworkState& State)
				{
					URpgAbilitySystemComponent* AbilitySystem = nullptr;
					ASSERT_THAT(IsNotNull(FindSwing(State.World, AbilitySystem)));
					ASSERT_THAT(IsTrue(AbilitySystem->TryActivateAbility(SwingHandle)));
				})
			.UntilServer(TEXT("The server's single-target swing took a section of the front trunk only"),
				[this](FNetworkState& State)
				{
					RecordCaptures(State.World, ServerCaptures, ServerCapturedStarts);
					return GetRemainingSections(State, FrontTrunk) == FieldSections - 1 && IsSwingIdle(State.World);
				},
				NetworkTimeout())
			.UntilClient(TEXT("The client's meter replicated full"), 0,
				[this](FNetworkState& State)
				{
					RecordCaptures(State.World, ClientCaptures, ClientCapturedStarts);
					return FMath::IsNearlyEqual(GetFocus(State.World, CasterId), MaxFocus, 0.01f) && IsSwingIdle(State.World);
				},
				NetworkTimeout())
			.ThenServer(TEXT("The plain swing captured one target on both machines and left the behind trunk whole"),
				[this](FNetworkState& State)
				{
					ASSERT_THAT(AreEqual(FieldSections, GetRemainingSections(State, BehindTrunk)));
					ASSERT_THAT(AreEqual(1, ServerCaptures.Num()));
					ASSERT_THAT(AreEqual(1, ClientCaptures.Num()));
					ASSERT_THAT(AreEqual(1, ServerCaptures[0]));
					ASSERT_THAT(AreEqual(1, ClientCaptures[0]));
				});
	}

	TEST_METHOD(PickaxeStrikeUnderTheSpellReachesTheVeinBehind)
	{
		using namespace RpgHarvestSpellPIETests;

		bPickaxe = true;
		QueueCasterReady();
		Network
			.ThenServer(TEXT("The server puts Wide Swing on the caster"),
				[this](FNetworkState& State)
				{
					URpgAbilitySystemComponent* AbilitySystem = FindAbilitySystem(State.World, CasterId);
					const TSubclassOf<UGameplayEffect> SpellEffect = LoadClass<UGameplayEffect>(nullptr, SpellEffectClassPath);
					ASSERT_THAT(IsTrue(AbilitySystem && SpellEffect));
					const FGameplayEffectSpecHandle Spell = AbilitySystem->MakeOutgoingSpec(SpellEffect, 1.0f, AbilitySystem->MakeEffectContext());
					ASSERT_THAT(IsTrue(AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spell.Data).IsValid()));
				})
			.UntilClient(TEXT("The caster has the running spell"), 0,
				[this](FNetworkState& State)
				{
					const URpgAbilitySystemComponent* AbilitySystem = FindAbilitySystem(State.World, CasterId);
					return AbilitySystem && AbilitySystem->HasMatchingGameplayTag(SpellActiveTag());
				},
				NetworkTimeout())
			.ThenClient(TEXT("The caster strikes once"), 0,
				[this](FNetworkState& State)
				{
					URpgAbilitySystemComponent* AbilitySystem = nullptr;
					ASSERT_THAT(IsNotNull(FindSwing(State.World, AbilitySystem)));
					ASSERT_THAT(IsTrue(AbilitySystem->TryActivateAbility(SwingHandle)));
				})
			.UntilServer(TEXT("The server's strike ended"),
				[this](FNetworkState& State)
				{
					RecordCaptures(State.World, ServerCaptures, ServerCapturedStarts);
					if (ServerCapturedStarts < 1 || !IsSwingIdle(State.World))
					{
						return false;
					}
					ResultTime = State.World->GetTimeSeconds();
					return true;
				},
				NetworkTimeout())
			.UntilServer(TEXT("Late commits had time to arrive"),
				[this](FNetworkState& State) { return State.World->GetTimeSeconds() - ResultTime >= QuietSeconds; },
				NetworkTimeout())
			.ThenServer(TEXT("The widened strike took the vein in front and the one behind it"),
				[this](FNetworkState& State)
				{
					TestRunner->AddInfo(FString::Printf(
						TEXT("Pickaxe under Wide Swing: %d swing targets captured; front vein %d of %d sections left, vein behind %d"),
						ServerCaptures.IsEmpty() ? 0 : ServerCaptures.Last(),
						GetRemainingSections(State, FrontTrunk), FieldSections, GetRemainingSections(State, BehindTrunk)));
					ASSERT_THAT(IsTrue(!ServerCaptures.IsEmpty() && ServerCaptures.Last() > 1));
					ASSERT_THAT(AreEqual(FieldSections - 1, GetRemainingSections(State, FrontTrunk)));
					ASSERT_THAT(AreEqual(FieldSections - 1, GetRemainingSections(State, BehindTrunk)));
				});
	}

	TEST_METHOD(ClientCastPredictsWidensSwingsShowsEverywhereAndExpires)
	{
		using namespace RpgHarvestSpellPIETests;

		QueueCasterReady();
		QueueSpellLearned();

		// Q and the next swing in the same frame: the reliable activation RPC puts the spell before the swing on the server.
		Network
			.ThenClient(TEXT("The client presses Q and swings in the same frame; the spell is predicted before the server knows it"), 0,
				[this](FNetworkState& State)
				{
					URpgAbilitySystemComponent* AbilitySystem = nullptr;
					const URpgGameplayAbility_Harvest* Swing = FindSwing(State.World, AbilitySystem);
					ASSERT_THAT(IsNotNull(Swing));
					OwnerPredictionBaseline = Swing->GetOwnerPredictedToolCueCountForTests();
					CasterAbsorbedBaseline = RpgCombatImpact::GetAbsorbedImpactCountForTests(State.World);
					ASSERT_THAT(IsTrue(AbilitySystem->TryActivateAbility(SpellHandle)));

					int32 Predicted = 0;
					ASSERT_THAT(AreEqual(1, CountSpellEffects(AbilitySystem, &Predicted)));
					ASSERT_THAT(AreEqual(1, Predicted));
					ASSERT_THAT(IsTrue(AbilitySystem->HasMatchingGameplayTag(SpellActiveTag())));
					ASSERT_THAT(IsNear(0.0f, GetFocus(State.World, CasterId), 0.01f));
					ASSERT_THAT(IsTrue(GetClientTunedTargets(State.World) > 1.0f));
					// The server has not received the activation yet.
					ASSERT_THAT(AreEqual(0, CountSpellEffects(FindAbilitySystem(ServerWorld.Get(), CasterId))));

					ASSERT_THAT(IsTrue(AbilitySystem->TryActivateAbility(SwingHandle)));
				})
			.UntilClient(TEXT("The server confirms the spell, and the client's prediction gives way to the server's copy"), 0,
				[this](FNetworkState& State)
				{
					const URpgAbilitySystemComponent* AbilitySystem = FindAbilitySystem(State.World, CasterId);
					RecordCaptures(State.World, ClientCaptures, ClientCapturedStarts);
					RecordCaptures(ServerWorld.Get(), ServerCaptures, ServerCapturedStarts);
					int32 Predicted = 0;
					const int32 Effects = CountSpellEffects(AbilitySystem, &Predicted);
					MaxClientSpellEffects = FMath::Max(MaxClientSpellEffects, Effects);
					OverlapFrames += Effects > 1 ? 1 : 0;
					MaxClientTunedTargets = FMath::Max(MaxClientTunedTargets, GetClientTunedTargets(State.World));
					return Effects == 1 && Predicted == 0;
				},
				NetworkTimeout())
			.UntilServer(TEXT("The server's wide swing took a section of the front trunk and of the one behind it"),
				[this](FNetworkState& State)
				{
					RecordCaptures(State.World, ServerCaptures, ServerCapturedStarts);
					RecordCaptures(CasterWorld.Get(), ClientCaptures, ClientCapturedStarts);
					if (GetRemainingSections(State, FrontTrunk) != FieldSections - 2 ||
						GetRemainingSections(State, BehindTrunk) != FieldSections - 1 || !IsSwingIdle(State.World))
					{
						return false;
					}
					ResultTime = State.World->GetTimeSeconds();
					return true;
				},
				NetworkTimeout())
			.UntilServer(TEXT("Late commits and cue copies had time to arrive"),
				[this](FNetworkState& State) { return State.World->GetTimeSeconds() - ResultTime >= QuietSeconds; },
				NetworkTimeout())
			.ThenServer(TEXT("Both machines captured the widened swing, and the host played both spectral hits"),
				[this](FNetworkState& State)
				{
					ASSERT_THAT(AreEqual(2, ServerCaptures.Num()));
					ASSERT_THAT(IsTrue(ServerCaptures[1] >= 2));
					ASSERT_THAT(AreEqual(FieldSections - 2, GetRemainingSections(State, FrontTrunk)));
					ASSERT_THAT(AreEqual(FieldSections - 1, GetRemainingSections(State, BehindTrunk)));
					ASSERT_THAT(AreEqual(2, SpectralHits.FindRef(State.World)));
					TestRunner->AddInfo(FString::Printf(
						TEXT("Cast edge: the client held up to %d spell effects (%d frames with two), and tuned up to %.0f swing targets"),
						MaxClientSpellEffects, OverlapFrames, MaxClientTunedTargets));
				})
			.UntilClient(TEXT("The caster shows both stock changes"), 0,
				[this](FNetworkState& State)
				{
					RecordCaptures(State.World, ClientCaptures, ClientCapturedStarts);
					return GetRemainingSections(State, FrontTrunk) == FieldSections - 2 &&
						GetRemainingSections(State, BehindTrunk) == FieldSections - 1;
				},
				NetworkTimeout())
			.ThenClient(TEXT("The caster captured the same widened swing as the server, predicted both spectral hits and played each once"), 0,
				[this](FNetworkState& State)
				{
					URpgAbilitySystemComponent* AbilitySystem = nullptr;
					const URpgGameplayAbility_Harvest* Swing = FindSwing(State.World, AbilitySystem);
					ASSERT_THAT(IsNotNull(Swing));
					const uint32 Predicted = Swing->GetOwnerPredictedToolCueCountForTests() - OwnerPredictionBaseline;
					const int32 Absorbed = RpgCombatImpact::GetAbsorbedImpactCountForTests(State.World) - CasterAbsorbedBaseline;
					const int32 Routed = SpectralHits.FindRef(State.World);
					TestRunner->AddInfo(FString::Printf(
						TEXT("Caster: %u hits predicted, %d spectral hits routed, %d server copies absorbed"), Predicted, Routed, Absorbed));
					ASSERT_THAT(AreEqual(2, ClientCaptures.Num()));
					ASSERT_THAT(AreEqual(ServerCaptures[1], ClientCaptures[1]));
					ASSERT_THAT(AreEqual(2u, Predicted));
					ASSERT_THAT(AreEqual(2, Routed - Absorbed));
				});

		// The host sees the caster's spell; a player who joins while it runs sees it too, but never the caster's focus.
		Network
			.UntilServer(TEXT("The host sees the running spell and its looping cue on the caster"),
				[this](FNetworkState& State)
				{
					const ARpgCharacter* Caster = FindCharacter(State.World, CasterId);
					const URpgAbilitySystemComponent* AbilitySystem = Caster ? Caster->GetRpgAbilitySystemComponent() : nullptr;
					return AbilitySystem && AbilitySystem->HasMatchingGameplayTag(SpellActiveTag()) && ShowsSpellCue(Caster);
				},
				NetworkTimeout())
			.ThenServer(TEXT("The server gives the caster focus the late joiner must not see"),
				[this](FNetworkState& State)
				{
					URpgAbilitySystemComponent* AbilitySystem = FindAbilitySystem(State.World, CasterId);
					ASSERT_THAT(IsNotNull(AbilitySystem));
					AbilitySystem->SetNumericAttributeBase(URpgFocusSet::GetFocusAttribute(), CasterFocusDuringSpell);
				})
			.ThenClientJoins(NetworkTimeout())
			.UntilServer(TEXT("Late join establishes the second connection"),
				[](FNetworkState& State) { return IsPrototypeExperienceReady(State.World, 2); },
				NetworkTimeout())
			.ThenServer(TEXT("The late joiner stands aside, out of the caster's lane"),
				[this](FNetworkState& State)
				{
					for (TActorIterator<ARpgCharacter> It(State.World); It; ++It)
					{
						const APlayerState* PlayerState = It->GetPlayerState();
						if (PlayerState && PlayerState->GetPlayerId() != CasterId && !It->IsLocallyControlled())
						{
							It->TeleportTo(LateJoinerLocation, FRotator::ZeroRotator);
							It->ForceNetUpdate();
						}
					}
				})
			.UntilClient(TEXT("The late joiner sees the caster's running spell and its looping cue"), 1,
				[this](FNetworkState& State)
				{
					const ARpgCharacter* Caster = FindCharacter(State.World, CasterId);
					const URpgAbilitySystemComponent* AbilitySystem = Caster ? Caster->GetRpgAbilitySystemComponent() : nullptr;
					int32 CueActors = 0;
					FString CueOwners;
					for (TActorIterator<AGameplayCueNotify_Actor> It(State.World); It; ++It)
					{
						if (It->GameplayCueTag == SpellCueTag())
						{
							++CueActors;
							CueOwners += GetNameSafe(It->GetOwner()) + TEXT(" ");
						}
					}
					const FString Seen = FString::Printf(
						TEXT("Late joiner: own pawn ready %d, caster %s, avatar %s, spell tag %d, cue active %d, cue actors %d (owners %s)"),
						IsPrototypeCharacterReady(FindLocalCharacter(State.World)) ? 1 : 0, *GetNameSafe(Caster),
						AbilitySystem ? *GetNameSafe(AbilitySystem->GetAvatarActor()) : TEXT("none"),
						AbilitySystem && AbilitySystem->HasMatchingGameplayTag(SpellActiveTag()) ? 1 : 0,
						AbilitySystem && AbilitySystem->IsGameplayCueActive(SpellCueTag()) ? 1 : 0, CueActors, *CueOwners);
					if (Seen != LateJoinerSeen)
					{
						LateJoinerSeen = Seen;
						TestRunner->AddInfo(Seen);
					}
					return IsPrototypeCharacterReady(FindLocalCharacter(State.World)) && AbilitySystem &&
						AbilitySystem->HasMatchingGameplayTag(SpellActiveTag()) && ShowsSpellCue(Caster);
				},
				NetworkTimeout())
			.ThenClient(TEXT("The late joiner never receives the caster's focus or its spell effect"), 1,
				[this](FNetworkState& State)
				{
					const URpgAbilitySystemComponent* AbilitySystem = FindAbilitySystem(State.World, CasterId);
					ASSERT_THAT(IsNotNull(AbilitySystem));
					ASSERT_THAT(IsNear(0.0f, AbilitySystem->GetNumericAttribute(URpgFocusSet::GetFocusAttribute()), 0.01f));
					// Mixed replication sends other players only the spell's tags and cue.
					ASSERT_THAT(AreEqual(0, CountSpellEffects(AbilitySystem)));
					ASSERT_THAT(IsNear(CasterFocusDuringSpell, GetFocus(ServerWorld.Get(), CasterId), 0.01f));
				});

		// Expiry: the caster swings without pause from shortly before the server's spell ends until after it ended on both
		// machines. Each swing's captured targets show how many swings the owner and the server disagree at that edge.
		Network
			.UntilServer(TEXT("The server's spell nears its end"),
				[this](FNetworkState& State)
				{
					const URpgAbilitySystemComponent* AbilitySystem = FindAbilitySystem(State.World, CasterId);
					float Remaining = 0.0f;
					float Duration = 0.0f;
					return AbilitySystem &&
						(!AbilitySystem->GetEffectTimeRemainingAndDurationForTag(SpellActiveTag(), Remaining, Duration) ||
							Remaining <= ExpiryLeadSeconds);
				},
				NetworkTimeout())
			.UntilClient(TEXT("The caster swings across the expiry until two swings started after the spell ended on both machines"), 0,
				[this](FNetworkState& State)
				{
					URpgAbilitySystemComponent* AbilitySystem = nullptr;
					const URpgGameplayAbility_Harvest* Swing = FindSwing(State.World, AbilitySystem);
					URpgAbilitySystemComponent* ServerAbilitySystem = FindAbilitySystem(ServerWorld.Get(), CasterId);
					if (!Swing || !ServerAbilitySystem)
					{
						return false;
					}
					RecordCaptures(State.World, ClientCaptures, ClientCapturedStarts);
					RecordCaptures(ServerWorld.Get(), ServerCaptures, ServerCapturedStarts);
					if (ServerStartsAtExpiry == INDEX_NONE && !ServerAbilitySystem->HasMatchingGameplayTag(SpellActiveTag()))
					{
						ServerStartsAtExpiry = static_cast<int32>(ServerCapturedStarts);
					}
					if (ClientStartsAtClientExpiry == INDEX_NONE && !AbilitySystem->HasMatchingGameplayTag(SpellActiveTag()))
					{
						ClientStartsAtClientExpiry = static_cast<int32>(ClientCapturedStarts);
					}
					const bool bDone = ServerStartsAtExpiry != INDEX_NONE && ClientStartsAtClientExpiry != INDEX_NONE &&
						static_cast<int32>(ClientCapturedStarts) >= FMath::Max(ServerStartsAtExpiry, ClientStartsAtClientExpiry) + SwingsAfterExpiry &&
						static_cast<int32>(ServerCapturedStarts) >= static_cast<int32>(ClientCapturedStarts);
					if (!bDone && IsSwingIdle(State.World) && IsSwingIdle(ServerWorld.Get()))
					{
						AbilitySystem->TryActivateAbility(SwingHandle);
					}
					return bDone && IsSwingIdle(State.World) && IsSwingIdle(ServerWorld.Get());
				},
				NetworkTimeout())
			.ThenServer(TEXT("The expiry swings ended"),
				[this](FNetworkState& State) { ResultTime = State.World->GetTimeSeconds(); })
			.UntilServer(TEXT("The last expiry swing's commit had time to arrive"),
				[this](FNetworkState& State) { return State.World->GetTimeSeconds() - ResultTime >= QuietSeconds; },
				NetworkTimeout())
			.ThenServer(TEXT("The owner and the server disagree on at most one swing at the expiry, and the last swing is single-target"),
				[this](FNetworkState& State)
				{
					ASSERT_THAT(AreEqual(ServerCaptures.Num(), ClientCaptures.Num()));
					int32 Mismatches = 0;
					FString Captures;
					for (int32 Index = 0; Index < ServerCaptures.Num(); ++Index)
					{
						Mismatches += ServerCaptures[Index] != ClientCaptures[Index] ? 1 : 0;
						Captures += FString::Printf(TEXT(" %d/%d"), ClientCaptures[Index], ServerCaptures[Index]);
					}
					TestRunner->AddInfo(FString::Printf(
						TEXT("Swing targets captured per swing, client/server:%s; the spell ended on the server before swing %d and on the client before swing %d"),
						*Captures, ServerStartsAtExpiry + 1, ClientStartsAtClientExpiry + 1));
					ASSERT_THAT(IsTrue(Mismatches <= 1));
					ASSERT_THAT(IsFalse(FindAbilitySystem(State.World, CasterId)->HasMatchingGameplayTag(SpellActiveTag())));
					// Every swing the server started after its spell ended is single-target again, the last one on both machines.
					for (int32 Index = ServerStartsAtExpiry; Index < ServerCaptures.Num(); ++Index)
					{
						ASSERT_THAT(AreEqual(1, ServerCaptures[Index]));
					}
					ASSERT_THAT(AreEqual(1, ClientCaptures.Last()));
				})
			.UntilClients(TEXT("Every client sees the spell and its looping cue end"),
				[this](FNetworkState& State)
				{
					const ARpgCharacter* Caster = FindCharacter(State.World, CasterId);
					const URpgAbilitySystemComponent* AbilitySystem = Caster ? Caster->GetRpgAbilitySystemComponent() : nullptr;
					return AbilitySystem && !AbilitySystem->HasMatchingGameplayTag(SpellActiveTag()) &&
						!AbilitySystem->IsGameplayCueActive(SpellCueTag());
				},
				NetworkTimeout());
	}
};

#endif // ENABLE_PIE_NETWORK_TEST
#endif // WITH_DEV_AUTOMATION_TESTS
