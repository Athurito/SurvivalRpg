#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"
#include "Network/RpgAbilityCueNetworkTestTypes.h"
#include "Network/RpgCombatNetworkTestTypes.h"

#include "AbilitySystemGlobals.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameplayCueManager.h"
#include "GameplayCueNotify_Actor.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Core/Character/RpgCharacter.h"
#include "SurvivalRpg/Core/Character/RpgDownedComponent.h"
#include "SurvivalRpg/Core/Character/RpgHealthComponent.h"
#include "SurvivalRpg/Core/Game/Experience/RpgExperienceDefinition.h"
#include "SurvivalRpg/Core/Game/Experience/RpgExperienceManagerComponent.h"
#include "SurvivalRpg/Core/Game/RpgGameModeBase.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/Development/RpgDeveloperSettings.h"

#if ENABLE_PIE_NETWORK_TEST

namespace RpgAbilityCueRespawnPIETests
{
	constexpr TCHAR PrototypeExperienceName[] = TEXT("RpgPrototypeExperience");
	constexpr TCHAR PrototypeGameModeClassPath[] = TEXT("/Game/SurvivalRpg/Core/Game/BP_Rpg_GameMode.BP_Rpg_GameMode_C");
	constexpr TCHAR PrototypeCharacterClassPrefix[] = TEXT("/Game/SurvivalRpg/Core/Character/BP_Rpg_Character");

	/** Seconds the test waits after the respawn for late or doubled cue events. */
	constexpr double QuietSeconds = 1.5;

	struct FNetworkState : public FBasePIENetworkComponentState
	{
		TWeakObjectPtr<ARpgCharacter> PawnBeforeDeath;
	};

	FTimespan NetworkTimeout()
	{
		return FTimespan::FromSeconds(90.0);
	}

	/**
	 * The looping cue under test: Wide Swing's, a GameplayCueNotify_Looping that the prototype Experience's harvesting
	 * feature registers. Any looping notify would do; its own content does not matter here.
	 */
	const FGameplayTag& EffectCueTag()
	{
		static const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.Harvesting.Spell.WideSwing"));
		return Tag;
	}

	/**
	 * A cue added outside effects (ActiveGameplayCues); it needs no notify, the test counts its routed events. A death
	 * removes such cues, so it must not come back on the respawned pawn.
	 */
	const FGameplayTag& LooseCueTag()
	{
		static const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.Harvesting.Stride"));
		return Tag;
	}

	ARpgCharacter* FindLocalCharacter(const UWorld* World)
	{
		const APlayerController* PlayerController = IsValid(World) ? World->GetFirstPlayerController() : nullptr;
		return PlayerController ? Cast<ARpgCharacter>(PlayerController->GetPawn()) : nullptr;
	}

	ARpgPlayerState* FindPlayerState(const UWorld* World, const int32 PlayerId)
	{
		const AGameStateBase* GameState = IsValid(World) ? World->GetGameState() : nullptr;
		if (!GameState || PlayerId == INDEX_NONE)
		{
			return nullptr;
		}
		for (APlayerState* PlayerState : GameState->PlayerArray)
		{
			if (PlayerState && PlayerState->GetPlayerId() == PlayerId)
			{
				return Cast<ARpgPlayerState>(PlayerState);
			}
		}
		return nullptr;
	}

	/** The pawn the player possesses on this machine, never a corpse that still carries the old player state. */
	ARpgCharacter* FindPlayerCharacter(const UWorld* World, const int32 PlayerId)
	{
		const ARpgPlayerState* PlayerState = FindPlayerState(World, PlayerId);
		return PlayerState ? PlayerState->GetPawn<ARpgCharacter>() : nullptr;
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

	/** Whether Character is a ready prototype pawn whose player state's ability system uses it as avatar. */
	bool IsBoundPrototypeCharacter(const ARpgCharacter* Character)
	{
		const USkeletalMeshComponent* Mesh = IsValid(Character) ? Character->GetMesh() : nullptr;
		const URpgAbilitySystemComponent* AbilitySystem = Mesh ? Character->GetRpgAbilitySystemComponent() : nullptr;
		return AbilitySystem && IsValid(Mesh->GetAnimInstance()) &&
			Character->GetClass()->GetPathName().StartsWith(PrototypeCharacterClassPrefix) &&
			IsValid(Character->GetPlayerState()) && AbilitySystem->GetAvatarActor() == Character;
	}

	/** Cue actors for EffectCueTag in Character's world that Character owns. */
	int32 CountCueActors(const ARpgCharacter* Character)
	{
		int32 Count = 0;
		for (TActorIterator<AGameplayCueNotify_Actor> It(Character->GetWorld()); It; ++It)
		{
			Count += It->GameplayCueTag == EffectCueTag() && It->GetOwner() == Character ? 1 : 0;
		}
		return Count;
	}
}

NETWORK_TEST_CLASS(AbilityCueRespawnPIE, "SurvivalRpg.Network")
{
	using FNetworkState = RpgAbilityCueRespawnPIETests::FNetworkState;

	FRpgScopedPIEWorldSaveIsolation WorldSaveIsolation;
	FRpgScopedNetworkTestGround Ground;
	FPIENetworkComponent<FNetworkState> Network{TestRunner, TestCommandBuilder, bInitializing};
	FPrimaryAssetId OriginalExperienceOverride;
	UClass* PrototypeGameModeClass = nullptr;

	/** One process holds every machine: the subject's player id and every WhileActive event routed on any machine. */
	int32 SubjectId = INDEX_NONE;
	double ResultTime = -1.0;
	struct FRoutedWhileActive
	{
		TWeakObjectPtr<UWorld> World;
		TWeakObjectPtr<AActor> Target;
		FGameplayTag Tag;
	};
	TArray<FRoutedWhileActive> Routed;
	FDelegateHandle CueRoutedHandle;

	/** WhileActive events of Tag routed to Character on its machine. */
	int32 CountWhileActive(const ARpgCharacter* Character, const FGameplayTag& Tag) const
	{
		int32 Count = 0;
		for (const FRoutedWhileActive& Event : Routed)
		{
			Count += Event.Target.Get() == Character && Event.World.Get() == Character->GetWorld() && Event.Tag == Tag ? 1 : 0;
		}
		return Count;
	}

	/**
	 * The effect's cue plays its WhileActive once on the respawned Character and shows one looping cue actor there. The
	 * death removed the loose cue (URpgPawnExtensionComponent::UninitializeAbilitySystem), so it does not return.
	 */
	void ExpectCuesOnceOn(const ARpgCharacter* Character, const TCHAR* Machine)
	{
		using namespace RpgAbilityCueRespawnPIETests;
		ASSERT_THAT(IsNotNull(Character));
		TestRunner->AddInfo(FString::Printf(TEXT("%s: effect cue WhileActive %d, loose cue WhileActive %d, cue actors %d on %s"),
			Machine, CountWhileActive(Character, EffectCueTag()), CountWhileActive(Character, LooseCueTag()),
			CountCueActors(Character), *GetNameSafe(Character)));
		ASSERT_THAT(AreEqual(1, CountWhileActive(Character, EffectCueTag())));
		ASSERT_THAT(AreEqual(0, CountWhileActive(Character, LooseCueTag())));
		ASSERT_THAT(AreEqual(1, CountCueActors(Character)));
	}

	BEFORE_EACH()
	{
		using namespace RpgAbilityCueRespawnPIETests;
		SubjectId = INDEX_NONE;
		ResultTime = -1.0;
		Routed.Reset();

		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && IsValid(Context.World()))
			{
				TestRunner->AddError(TEXT("Ability cue respawn PIE automation requires all existing PIE sessions to be stopped first."));
				return;
			}
		}

		URpgDeveloperSettings* DeveloperSettings = GetMutableDefault<URpgDeveloperSettings>();
		OriginalExperienceOverride = DeveloperSettings->ExperienceOverride;
		WorldSaveIsolation.Start();
		Ground.Start();
		PrototypeGameModeClass = LoadClass<ARpgGameModeBase>(nullptr, PrototypeGameModeClassPath);
		ASSERT_THAT(IsNotNull(PrototypeGameModeClass));
		DeveloperSettings->ExperienceOverride =
			FPrimaryAssetId(URpgExperienceDefinition::StaticClass()->GetFName(), PrototypeExperienceName);

		// The effect's cue is set on its class default, which every machine of this one-process session shares.
		GetMutableDefault<URpgNetworkAutomationLoopingCueEffect>()->GameplayCues = {FGameplayEffectCue(EffectCueTag(), 0.0f, 0.0f)};
		if (UGameplayCueManager* CueManager = UAbilitySystemGlobals::Get().GetGameplayCueManager())
		{
			CueRoutedHandle = CueManager->OnGameplayCueRouted().AddLambda(
				[this](AActor* Target, const FGameplayTag Tag, const EGameplayCueEvent::Type Event,
					const FGameplayCueParameters&, EGameplayCueExecutionOptions)
				{
					if (Event == EGameplayCueEvent::WhileActive && Target)
					{
						Routed.Add({Target->GetWorld(), Target, Tag});
					}
				});
		}

		FNetworkComponentBuilder<FNetworkState>()
			.WithClients(2)
			.AsListenServer()
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
		GetMutableDefault<URpgNetworkAutomationLoopingCueEffect>()->GameplayCues.Reset();
		GetMutableDefault<URpgDeveloperSettings>()->ExperienceOverride = OriginalExperienceOverride;
	}

	TEST_METHOD(EffectCueShowsOnceOnTheRespawnedPawnEverywhere)
	{
		using namespace RpgAbilityCueRespawnPIETests;

		Network
			.ThenServer(TEXT("Listen server uses its own nonpersistent test GameMode instance"),
				[this](FNetworkState& State) { ASSERT_THAT(IsTrue(WorldSaveIsolation.IsWorldIsolated(State.World))); })
			.UntilServer(TEXT("Prototype Experience loads with both clients"),
				[](FNetworkState& State) { return IsPrototypeExperienceReady(State.World, 2); },
				NetworkTimeout())
			.UntilClients(TEXT("Both clients own a bound Prototype pawn"),
				[](FNetworkState& State)
				{
					return IsPrototypeExperienceReady(State.World, 2) && IsBoundPrototypeCharacter(FindLocalCharacter(State.World));
				},
				NetworkTimeout())
			.ThenClient(TEXT("The first client is the subject"), 0,
				[this](FNetworkState& State)
				{
					const ARpgCharacter* Character = FindLocalCharacter(State.World);
					ASSERT_THAT(IsTrue(Character && Character->GetPlayerState()));
					SubjectId = Character->GetPlayerState()->GetPlayerId();
				})
			.UntilClient(TEXT("The second client sees the subject as a bound simulated proxy"), 1,
				[this](FNetworkState& State)
				{
					const ARpgCharacter* Subject = FindPlayerCharacter(State.World, SubjectId);
					return IsBoundPrototypeCharacter(Subject) && Subject->GetLocalRole() == ROLE_SimulatedProxy;
				},
				NetworkTimeout())
			.ThenServer(TEXT("The server applies a death-surviving effect with a looping cue and adds a loose cue"),
				[this](FNetworkState& State)
				{
					const ARpgCharacter* Subject = FindPlayerCharacter(State.World, SubjectId);
					URpgAbilitySystemComponent* AbilitySystem = Subject ? Subject->GetRpgAbilitySystemComponent() : nullptr;
					ASSERT_THAT(IsNotNull(AbilitySystem));
					ASSERT_THAT(IsTrue(AbilitySystem->ApplyGameplayEffectToSelf(
						GetDefault<URpgNetworkAutomationLoopingCueEffect>(), 1.0f, AbilitySystem->MakeEffectContext()).IsValid()));
					AbilitySystem->AddGameplayCue(LooseCueTag());
				})
			.UntilServer(TEXT("The host shows the looping cue and the loose cue on the subject"),
				[this](FNetworkState& State)
				{
					const ARpgCharacter* Subject = FindPlayerCharacter(State.World, SubjectId);
					return Subject && CountCueActors(Subject) == 1 && CountWhileActive(Subject, LooseCueTag()) == 1;
				},
				NetworkTimeout())
			.UntilClients(TEXT("The owner and the proxy show the looping cue on the subject"),
				[this](FNetworkState& State)
				{
					const ARpgCharacter* Subject = FindPlayerCharacter(State.World, SubjectId);
					return Subject && CountCueActors(Subject) == 1 && CountWhileActive(Subject, LooseCueTag()) == 1;
				},
				NetworkTimeout())
			.ThenClients(TEXT("Each client records the subject's pawn before death"),
				[this](FNetworkState& State) { State.PawnBeforeDeath = FindPlayerCharacter(State.World, SubjectId); })
			.ThenServer(TEXT("The subject dies through the authoritative health and downed components"),
				[this](FNetworkState& State)
				{
					ARpgCharacter* Subject = FindPlayerCharacter(State.World, SubjectId);
					URpgHealthComponent* Health = URpgHealthComponent::FindHealthComponent(Subject);
					ASSERT_THAT(IsNotNull(Health));
					State.PawnBeforeDeath = Subject;
					Health->DamageSelfDestruct(false);
					if (URpgDownedComponent* Downed = URpgDownedComponent::FindDownedComponent(Subject); Downed && Downed->IsDowned())
					{
						Downed->ForceDeathFromDowned();
					}
				})
			.UntilClient(TEXT("The owner may respawn"), 0,
				[this](FNetworkState& State)
				{
					const ARpgPlayerState* PlayerState = FindPlayerState(State.World, SubjectId);
					return PlayerState && PlayerState->IsWaitingForRespawn() && PlayerState->CanRespawnNow();
				},
				NetworkTimeout())
			.ThenClient(TEXT("The owner requests its respawn"), 0,
				[this](FNetworkState& State)
				{
					ARpgPlayerController* Controller = Cast<ARpgPlayerController>(State.World->GetFirstPlayerController());
					ASSERT_THAT(IsNotNull(Controller));
					Controller->RequestRespawn();
				})
			.UntilServer(TEXT("The server's subject is a new bound pawn"),
				[this](FNetworkState& State)
				{
					const ARpgCharacter* Subject = FindPlayerCharacter(State.World, SubjectId);
					return IsBoundPrototypeCharacter(Subject) && Subject != State.PawnBeforeDeath.Get();
				},
				NetworkTimeout())
			.UntilClients(TEXT("Each client binds the respawned pawn"),
				[this](FNetworkState& State)
				{
					const ARpgCharacter* Subject = FindPlayerCharacter(State.World, SubjectId);
					return IsBoundPrototypeCharacter(Subject) && Subject != State.PawnBeforeDeath.Get();
				},
				NetworkTimeout())
			.ThenServer(TEXT("Late or doubled cue events get time to arrive"),
				[this](FNetworkState& State) { ResultTime = State.World->GetTimeSeconds(); })
			.UntilServer(TEXT("The quiet time passed"),
				[this](FNetworkState& State) { return State.World->GetTimeSeconds() - ResultTime >= QuietSeconds; },
				NetworkTimeout())
			.ThenServer(TEXT("The server shows each cue once on the respawned pawn"),
				[this](FNetworkState& State) { ExpectCuesOnceOn(FindPlayerCharacter(State.World, SubjectId), TEXT("Server")); })
			.ThenClient(TEXT("The owner shows each cue once on its respawned pawn"), 0,
				[this](FNetworkState& State) { ExpectCuesOnceOn(FindPlayerCharacter(State.World, SubjectId), TEXT("Owner")); })
			.ThenClient(TEXT("The proxy shows each cue once on the respawned pawn"), 1,
				[this](FNetworkState& State) { ExpectCuesOnceOn(FindPlayerCharacter(State.World, SubjectId), TEXT("Proxy")); });
	}
};

#endif // ENABLE_PIE_NETWORK_TEST
#endif // WITH_DEV_AUTOMATION_TESTS
