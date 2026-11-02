#include "Network/RpgCombatProjectileNetworkTestTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemGlobals.h"
#include "CQTest.h"
#include "Components/PIENetworkComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameplayCueManager.h"
#include "HAL/PlatformTime.h"
#include "Network/RpgCombatNetworkTestTypes.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgStaminaSet.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/AbilitySystem/Tasks/RpgAbilityTask_ProjectileShot.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"

#if ENABLE_PIE_NETWORK_TEST

namespace RpgCombatProjectilePIETests
{
	constexpr int32 ClientCount = 2;
	constexpr double CueQuietSeconds = 1.0;
	constexpr float MaxSimulatedFrameTime = 1.0f / 60.0f;

	/** Away from the pawns at the origin; the shooter fires along +X at the target 15 m away. */
	const FVector ShooterLocation(0.0f, 1000.0f, 300.0f);
	const FVector TargetLocation(1500.0f, 1000.0f, 300.0f);

	struct FNetworkState : public FBasePIENetworkComponentState
	{
		ARpgCombatNetworkTargetFixture* Target = nullptr;
		ARpgCombatNetworkTargetFixture* Shooter = nullptr;
	};

	/** One impact cue routed on some machine. */
	struct FRoutedCue
	{
		TWeakObjectPtr<UWorld> World;
		FVector Location = FVector::ZeroVector;
	};

	FTimespan NetworkTimeout()
	{
		return FTimespan::FromSeconds(60.0);
	}

	ARpgCombatProjectile* FindProjectile(UWorld* World, const int32 ShotId)
	{
		for (TActorIterator<ARpgCombatProjectile> It(World); It; ++It)
		{
			if (IsValid(*It) && It->GetShotId() == ShotId)
			{
				return *It;
			}
		}
		return nullptr;
	}

	int32 CountProjectiles(UWorld* World)
	{
		int32 Count = 0;
		for (TActorIterator<ARpgCombatProjectile> It(World); It; ++It)
		{
			Count += IsValid(*It) ? 1 : 0;
		}
		return Count;
	}

	TArray<APlayerState*> GetPlayerStates(const FNetworkState& State)
	{
		const AGameStateBase* GameState = State.World ? State.World->GetGameState() : nullptr;
		return GameState ? GameState->PlayerArray : TArray<APlayerState*>();
	}

	bool AllPlayersHavePawns(const FNetworkState& State)
	{
		const TArray<APlayerState*> PlayerStates = GetPlayerStates(State);
		return PlayerStates.Num() == ClientCount + 1 &&
			!PlayerStates.ContainsByPredicate([](const APlayerState* PlayerState) { return !PlayerState || !PlayerState->GetPawn(); });
	}

	bool IsServerReady(const FNetworkState& State)
	{
		if (!IsValid(State.World) || State.World->GetNetMode() != NM_ListenServer || !State.World->AreActorsInitialized())
		{
			return false;
		}

		const AGameStateBase* GameState = State.World->GetGameState();
		const UNetDriver* NetDriver = State.World->GetNetDriver();
		return GameState && GameState->HasMatchStarted() && UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GameState) &&
			NetDriver && NetDriver->ClientConnections.Num() == ClientCount && AllPlayersHavePawns(State);
	}

	bool IsClientReady(const FNetworkState& State)
	{
		if (!IsValid(State.World) || State.World->GetNetMode() != NM_Client || !State.World->AreActorsInitialized())
		{
			return false;
		}

		const AGameStateBase* GameState = State.World->GetGameState();
		const APlayerController* PlayerController = State.World->GetFirstPlayerController();
		return GameState && GameState->HasMatchStarted() && UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GameState) &&
			PlayerController && PlayerController->PlayerState && AllPlayersHavePawns(State);
	}

	/**
	 * Binds every player's pawn as the avatar of its player state's ability system, as PawnExtension does for
	 * characters. A player state is a simulated proxy on clients, and GAS activates nothing on a simulated avatar.
	 */
	void BindPawnAvatars(const FNetworkState& State)
	{
		for (APlayerState* PlayerState : GetPlayerStates(State))
		{
			if (UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PlayerState))
			{
				AbilitySystem->InitAbilityActorInfo(PlayerState, PlayerState->GetPawn());
			}
		}
	}

	/** Launches a slow test projectile from the equipment-free shooter fixture, as an NPC or system launch would. */
	ARpgCombatProjectile* LaunchFromShooter(
		FNetworkState& State,
		const FVector& Origin,
		const int32 ShotId,
		const TSubclassOf<ARpgCombatProjectile> ProjectileClass = ARpgNetworkAutomationTestProjectile::StaticClass())
	{
		URpgAbilitySystemComponent* AbilitySystem = State.Shooter ? State.Shooter->GetRpgAbilitySystemComponent() : nullptr;
		if (!AbilitySystem)
		{
			return nullptr;
		}

		const FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(
			URpgNetworkAutomationProjectileDamageEffect::StaticClass(), 1.0f, AbilitySystem->MakeEffectContext());
		FRpgProjectileLaunchParams Params;
		Params.Origin = Origin;
		Params.Direction = FVector::ForwardVector;
		Params.Damage = ARpgNetworkAutomationTestProjectile::ShotDamage;
		Params.ShotId = ShotId;
		return ARpgCombatProjectile::LaunchWithDamageSpec(State.World, ProjectileClass, Params, Spec, nullptr);
	}

	/** How long each client watches its stopped copy; a copy that kept simulating under gravity would fall about 1.2 m. */
	constexpr double ClientStopWatchSeconds = 0.5;

	/** What one client saw of a shot's stop. */
	struct FClientStop
	{
		FVector ImpactLocation = FVector::ZeroVector;
		double FirstSeenSeconds = -1.0;
		double LastSeenSeconds = -1.0;
		double MaxDrift = 0.0;
	};
}

NETWORK_TEST_CLASS(CombatProjectilePIE, "SurvivalRpg.Network")
{
	using FNetworkState = RpgCombatProjectilePIETests::FNetworkState;
	using FRoutedCue = RpgCombatProjectilePIETests::FRoutedCue;
	using FClientStop = RpgCombatProjectilePIETests::FClientStop;

	FRpgScopedNetworkTestFrameTimeLimit FrameTimeLimit;
	FPIENetworkComponent<FNetworkState> Network{
		TestRunner,
		TestCommandBuilder,
		bInitializing};
	FPacketSimulationSettings PacketSettings;
	FGameplayTag ImpactCueTag;
	TArray<FRoutedCue> RoutedCues;
	TMap<TWeakObjectPtr<UWorld>, FClientStop> ClientStops;
	FDelegateHandle CueRoutedHandle;
	double ImpactTime = -1.0;
	bool bProjectileCausedHit = false;
	int32 ShooterPlayerId = INDEX_NONE;
	int32 ServerShotId = 0;
	double DrawStartSeconds = 0.0;
	double DrawStartWorldSeconds = 0.0;
	TArray<TSharedPtr<FString>> StepDescriptions;

	/** Keeps a built step description alive for the whole test; steps store only the pointer. */
	const TCHAR* Describe(const FString& Description)
	{
		TSharedPtr<FString> Stored = MakeShared<FString>(Description);
		const TCHAR* Result = **Stored;
		StepDescriptions.Add(MoveTemp(Stored));
		return Result;
	}

	BEFORE_EACH()
	{
		RoutedCues.Reset();
		ClientStops.Reset();
		StepDescriptions.Reset();
		ImpactTime = -1.0;
		bProjectileCausedHit = false;
		ShooterPlayerId = INDEX_NONE;

		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && IsValid(Context.World()))
			{
				TestRunner->AddError(TEXT("Projectile PIE automation requires all existing PIE sessions to be stopped first."));
				return;
			}
		}

		ImpactCueTag = GetDefault<ARpgNetworkAutomationTestProjectile>()->GetImpactCueTagForTests();
		ASSERT_THAT(IsTrue(ImpactCueTag.IsValid()));
		FrameTimeLimit.Start(RpgCombatProjectilePIETests::MaxSimulatedFrameTime);

		// Every executed impact cue on every machine of this one-process session.
		if (UGameplayCueManager* CueManager = UAbilitySystemGlobals::Get().GetGameplayCueManager())
		{
			CueRoutedHandle = CueManager->OnGameplayCueRouted().AddLambda(
				[this](AActor* Target, FGameplayTag Tag, EGameplayCueEvent::Type Event,
					const FGameplayCueParameters& Parameters, EGameplayCueExecutionOptions)
				{
					if (Event == EGameplayCueEvent::Executed && Tag == ImpactCueTag)
					{
						RoutedCues.Add({Target ? Target->GetWorld() : nullptr, Parameters.Location});
					}
				});
		}

		PacketSettings = FPacketSimulationSettings();
		PacketSettings.PktLag = 60;
		PacketSettings.PktLagVariance = 10;
		FNetworkComponentBuilder<FNetworkState>()
			.WithClients(RpgCombatProjectilePIETests::ClientCount)
			.AsListenServer()
			.WithPacketSimulationSettings(&PacketSettings)
			.WithGameInstanceClass(UGameInstance::StaticClass())
			.WithGameMode(ARpgNetworkAutomationProjectileGameMode::StaticClass())
			.Build(Network);
	}

	AFTER_EACH()
	{
		if (UGameplayCueManager* CueManager = UAbilitySystemGlobals::Get().GetGameplayCueManager())
		{
			CueManager->OnGameplayCueRouted().Remove(CueRoutedHandle);
		}
	}

	int32 CountImpactCues(const UWorld* World) const
	{
		int32 Count = 0;
		for (const FRoutedCue& Cue : RoutedCues)
		{
			Count += Cue.World.Get() == World ? 1 : 0;
		}
		return Count;
	}

	/** Server and clients see the target, and the shooter fixture where a test needs one, in faction 1. */
	void QueueFixtures(const bool bWithShooter)
	{
		using namespace RpgCombatProjectilePIETests;

		Network
			.UntilServer(TEXT("Listen server has both clients, their pawns and the RPG game state"),
				[](FNetworkState& State) { return IsServerReady(State); }, NetworkTimeout())
			.UntilClients(TEXT("Both clients see every player and the RPG game state"),
				[](FNetworkState& State) { return IsClientReady(State); }, NetworkTimeout())
			.SpawnAndReplicate<ARpgCombatNetworkTargetFixture, &FNetworkState::Target>(
				[](ARpgCombatNetworkTargetFixture& Target) { Target.SetActorLocation(TargetLocation); },
				NetworkTimeout());

		if (bWithShooter)
		{
			Network.SpawnAndReplicate<ARpgCombatNetworkTargetFixture, &FNetworkState::Shooter>(
				[](ARpgCombatNetworkTargetFixture& Shooter)
				{
					Shooter.SetActorLocation(ShooterLocation);
					Shooter.SetGenericTeamId(FGenericTeamId(1));
				},
				NetworkTimeout());
		}
	}

	/**
	 * After the server saw the hit: every client shows the projectile stopped at the impact and keeping still, exactly
	 * one damage, exactly one impact cue on every machine, then no projectile.
	 */
	void QueueSingleHitAndCue(const TCHAR* Shot, const int32 ShotId)
	{
		using namespace RpgCombatProjectilePIETests;

		Network
			.UntilServer(Describe(FString::Printf(TEXT("%s hits the target on the server"), Shot)),
				[this](FNetworkState& State)
				{
					if (State.Target && State.Target->GetHealthDropCount() > 0 && ImpactTime < 0.0)
					{
						// The projectile lingers after its impact, so the causer is still alive here.
						ImpactTime = FPlatformTime::Seconds();
						ClientStops.Reset();
						const AActor* Causer = State.Target->GetLastDamageCauser();
						bProjectileCausedHit = Causer && Causer->IsA<ARpgCombatProjectile>();
					}
					return ImpactTime >= 0.0;
				},
				NetworkTimeout())
			.UntilClients(Describe(FString::Printf(TEXT("%s stops on every client, which watches its stopped copy"), Shot)),
				[this, ShotId](FNetworkState& State) { return WatchClientStop(State, ShotId); }, NetworkTimeout())
			.ThenClients(Describe(FString::Printf(TEXT("%s stopped at the impact on every client and stayed there"), Shot)),
				[this](FNetworkState& State) { AssertClientStop(State); })
			.UntilServer(TEXT("Late duplicates have time to arrive"),
				[this](FNetworkState&) { return FPlatformTime::Seconds() - ImpactTime >= CueQuietSeconds; },
				NetworkTimeout())
			.ThenServer(Describe(FString::Printf(TEXT("%s damaged the target exactly once, caused by the projectile"), Shot)),
				[this](FNetworkState& State)
				{
					ASSERT_THAT(IsTrue(bProjectileCausedHit));
					ASSERT_THAT(IsNotNull(State.Target));
					ASSERT_THAT(IsTrue(State.Target->GetHealthDropCount() == 1));
					ASSERT_THAT(IsTrue(State.Target->GetLastHealthBeforeDamage() > State.Target->GetLastHealthAfterDamage()));
				})
			.ThenServer(TEXT("The host played the impact cue once at the target"),
				[this](FNetworkState& State) { AssertSingleImpactCue(State); })
			.ThenClients(TEXT("Every client played the impact cue once at the target"),
				[this](FNetworkState& State) { AssertSingleImpactCue(State); })
			.UntilServer(TEXT("The stopped projectile goes on the server"),
				[](FNetworkState& State) { return CountProjectiles(State.World) == 0; }, NetworkTimeout())
			.UntilClients(TEXT("The stopped projectile goes on every client"),
				[](FNetworkState& State) { return CountProjectiles(State.World) == 0; }, NetworkTimeout());
	}

	/**
	 * Records this client's copy of the shot once it reports a stop, and how far it drifts from the replicated stop
	 * location while the client watches it. Done after the watch time, or when the copy is gone after its stop was seen.
	 */
	bool WatchClientStop(FNetworkState& State, const int32 ShotId)
	{
		using namespace RpgCombatProjectilePIETests;

		const ARpgCombatProjectile* Projectile = FindProjectile(State.World, ShotId);
		FClientStop* Stop = ClientStops.Find(State.World);
		if (!Projectile || Projectile->GetImpactResult() == ERpgProjectileImpactResult::None)
		{
			return Stop && !Projectile;
		}

		const double Now = FPlatformTime::Seconds();
		if (!Stop)
		{
			Stop = &ClientStops.Add(State.World);
			Stop->ImpactLocation = Projectile->GetImpactLocationForTests();
			Stop->FirstSeenSeconds = Now;
		}
		Stop->LastSeenSeconds = Now;
		Stop->MaxDrift = FMath::Max(Stop->MaxDrift, FVector::Dist(Projectile->GetActorLocation(), Stop->ImpactLocation));
		return Now - Stop->FirstSeenSeconds >= ClientStopWatchSeconds;
	}

	void AssertClientStop(FNetworkState& State)
	{
		using namespace RpgCombatProjectilePIETests;

		const FClientStop* Stop = ClientStops.Find(State.World);
		ASSERT_THAT(IsNotNull(Stop));
		ASSERT_THAT(IsTrue(FVector::Dist(Stop->ImpactLocation, TargetLocation) < 100.0));
		ASSERT_THAT(IsTrue(Stop->LastSeenSeconds - Stop->FirstSeenSeconds >= ClientStopWatchSeconds * 0.8));
		ASSERT_THAT(IsTrue(Stop->MaxDrift < 1.0));
	}

	void AssertSingleImpactCue(FNetworkState& State)
	{
		ASSERT_THAT(IsTrue(CountImpactCues(State.World) == 1));
		for (const FRoutedCue& Cue : RoutedCues)
		{
			if (Cue.World.Get() == State.World)
			{
				ASSERT_THAT(IsTrue(FVector::Dist(Cue.Location, RpgCombatProjectilePIETests::TargetLocation) < 100.0));
			}
		}
	}

	TEST_METHOD(ReplicatedFlightHitsOnceWithOneImpactCuePerMachine)
	{
		using namespace RpgCombatProjectilePIETests;
		constexpr int32 ShotId = 1;

		QueueFixtures(true);
		Network
			.ThenServer(TEXT("The equipment-free shooter launches a slow projectile at the target"),
				[this](FNetworkState& State)
				{
					ASSERT_THAT(IsNotNull(State.Target));
					State.Target->ResetDamageObservations();
					ASSERT_THAT(IsNotNull(LaunchFromShooter(State, ShooterLocation + FVector(50.0f, 0.0f, 0.0f), ShotId)));
				})
			.UntilClients(TEXT("Every client sees the projectile in flight, with its launch point for the launch sound"),
				[](FNetworkState& State)
				{
					const ARpgCombatProjectile* Projectile = FindProjectile(State.World, ShotId);
					return Projectile && Projectile->GetImpactResult() == ERpgProjectileImpactResult::None &&
						Projectile->GetActorLocation().X > ShooterLocation.X + 300.0f &&
						Projectile->GetLaunchOrigin().Equals(ShooterLocation + FVector(50.0f, 0.0f, 0.0f), 1.0f);
				},
				NetworkTimeout());
		QueueSingleHitAndCue(TEXT("The replicated projectile"), ShotId);
	}

	TEST_METHOD(PointBlankAndOrphanedProjectilesStillHitOnce)
	{
		using namespace RpgCombatProjectilePIETests;

		QueueFixtures(true);
		Network
			.ThenServer(TEXT("The shooter launches a falling projectile right in front of the target"),
				[this](FNetworkState& State)
				{
					ASSERT_THAT(IsNotNull(State.Target));
					State.Target->ResetDamageObservations();
					// It hits before it first replicates, so every client receives it already stopped.
					ASSERT_THAT(IsNotNull(LaunchFromShooter(State, TargetLocation - FVector(40.0f, 0.0f, 0.0f), 2,
						ARpgNetworkAutomationFallingTestProjectile::StaticClass())));
				});
		QueueSingleHitAndCue(TEXT("The point-blank projectile"), 2);

		Network
			.ThenServer(TEXT("A slow projectile flies while its shooter is removed"),
				[this](FNetworkState& State)
				{
					RoutedCues.Reset();
					ImpactTime = -1.0;
					bProjectileCausedHit = false;
					State.Target->ResetDamageObservations();
					ASSERT_THAT(IsNotNull(LaunchFromShooter(State, ShooterLocation + FVector(50.0f, 0.0f, 0.0f), 3)));
					State.Shooter->Destroy();
					State.Shooter = nullptr;
				});
		QueueSingleHitAndCue(TEXT("The orphaned projectile"), 3);
	}

	TEST_METHOD(PredictedAbilityLaunchesWithoutEquipment)
	{
		using namespace RpgCombatProjectilePIETests;

		QueueFixtures(false);
		Network
			.ThenClients(TEXT("Clients bind each player's pawn as its ability system avatar"),
				[](FNetworkState& State) { BindPawnAvatars(State); })
			.ThenServer(TEXT("The server binds avatars and grants every player the projectile ability"),
				[this](FNetworkState& State)
				{
					ASSERT_THAT(IsNotNull(State.Target));
					State.Target->ResetDamageObservations();
					BindPawnAvatars(State);
					for (APlayerState* PlayerState : GetPlayerStates(State))
					{
						UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PlayerState);
						ASSERT_THAT(IsNotNull(AbilitySystem));
						AbilitySystem->GiveAbility(FGameplayAbilitySpec(URpgNetworkAutomationProjectileAbility::StaticClass()));
					}
				})
			.UntilClient(TEXT("The first client owns the projectile ability"), 0,
				[](FNetworkState& State)
				{
					const APlayerController* PlayerController = State.World->GetFirstPlayerController();
					const UAbilitySystemComponent* AbilitySystem = PlayerController
						? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PlayerController->PlayerState)
						: nullptr;
					return AbilitySystem && AbilitySystem->FindAbilitySpecFromClass(URpgNetworkAutomationProjectileAbility::StaticClass());
				},
				NetworkTimeout())
			.ThenClient(TEXT("The first client predicts the ability"), 0,
				[this](FNetworkState& State)
				{
					const APlayerController* PlayerController = State.World->GetFirstPlayerController();
					ASSERT_THAT(IsTrue(PlayerController && PlayerController->PlayerState));
					ShooterPlayerId = PlayerController->PlayerState->GetPlayerId();
					UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PlayerController->PlayerState);
					const FGameplayAbilitySpec* Spec = AbilitySystem
						? AbilitySystem->FindAbilitySpecFromClass(URpgNetworkAutomationProjectileAbility::StaticClass())
						: nullptr;
					ASSERT_THAT(IsNotNull(Spec));
					ASSERT_THAT(IsTrue(AbilitySystem->TryActivateAbility(Spec->Handle)));
				});
		QueueSingleHitAndCue(TEXT("The ability's projectile"), URpgNetworkAutomationProjectileAbility::ShotId);
		Network.ThenServer(TEXT("The predicting player is the damage's instigator"),
			[this](FNetworkState& State)
			{
				const APlayerState* Instigator = Cast<APlayerState>(State.Target->GetLastDamageInstigator());
				ASSERT_THAT(IsTrue(Instigator && Instigator->GetPlayerId() == ShooterPlayerId));
			});
	}

	static float GetStamina(const APlayerState* PlayerState)
	{
		const UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PlayerState);
		return AbilitySystem ? AbilitySystem->GetNumericAttribute(URpgStaminaSet::GetStaminaAttribute()) : -1.0f;
	}

	static int32 GetArrows(const APlayerState* PlayerState)
	{
		const URpgInventoryManagerComponent* Inventory = PlayerState ? PlayerState->FindComponentByClass<URpgInventoryManagerComponent>() : nullptr;
		return Inventory ? Inventory->GetTotalItemCountByDefinition(URpgNetworkAutomationArrowDefinition::StaticClass()) : -1;
	}

	static constexpr int32 StartingArrows = 5;
	static constexpr float StartingStamina = 100.0f;

	static URpgNetworkAutomationShotAbility* FindShotAbility(const FNetworkState& State)
	{
		const APlayerController* PlayerController = State.World->GetFirstPlayerController();
		const UAbilitySystemComponent* AbilitySystem = PlayerController
			? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PlayerController->PlayerState)
			: nullptr;
		const FGameplayAbilitySpec* Spec = AbilitySystem ? AbilitySystem->FindAbilitySpecFromClass(URpgNetworkAutomationShotAbility::StaticClass()) : nullptr;
		return Spec ? Cast<URpgNetworkAutomationShotAbility>(Spec->GetPrimaryInstance()) : nullptr;
	}

	/** Gives every player stamina, test arrows and the shot ability, then starts the first client's draw. */
	void QueueClientDraw()
	{
		using namespace RpgCombatProjectilePIETests;
		QueueFixtures(false);
		Network
			.ThenClients(TEXT("Clients bind each player's pawn as its ability system avatar"),
				[](FNetworkState& State) { BindPawnAvatars(State); })
			.ThenServer(TEXT("The server gives every player stamina, five test arrows and the shot ability"),
				[this](FNetworkState& State)
				{
					BindPawnAvatars(State);
					for (APlayerState* PlayerState : GetPlayerStates(State))
					{
						UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PlayerState);
						URpgInventoryManagerComponent* Inventory = PlayerState ? PlayerState->FindComponentByClass<URpgInventoryManagerComponent>() : nullptr;
						ASSERT_THAT(IsNotNull(AbilitySystem));
						ASSERT_THAT(IsNotNull(Inventory));
						AbilitySystem->AddAttributeSetSubobject(NewObject<URpgStaminaSet>(PlayerState));
						AbilitySystem->SetNumericAttributeBase(URpgStaminaSet::GetMaxStaminaAttribute(), StartingStamina);
						AbilitySystem->SetNumericAttributeBase(URpgStaminaSet::GetStaminaAttribute(), StartingStamina);
						ASSERT_THAT(IsNotNull(Inventory->AddItemDefinition(URpgNetworkAutomationArrowDefinition::StaticClass(), StartingArrows)));
						AbilitySystem->GiveAbility(FGameplayAbilitySpec(URpgNetworkAutomationShotAbility::StaticClass()));
					}
				})
			.UntilClient(TEXT("The first client owns the shot, its stamina and its arrows"), 0,
				[](FNetworkState& State)
				{
					const APlayerController* PlayerController = State.World->GetFirstPlayerController();
					const APlayerState* PlayerState = PlayerController ? PlayerController->PlayerState : nullptr;
					const UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PlayerState);
					return AbilitySystem && AbilitySystem->FindAbilitySpecFromClass(URpgNetworkAutomationShotAbility::StaticClass())
						&& GetStamina(PlayerState) == StartingStamina && GetArrows(PlayerState) == StartingArrows;
				},
				NetworkTimeout())
			.ThenClient(TEXT("The first client starts drawing"), 0,
				[this](FNetworkState& State)
				{
					const APlayerController* PlayerController = State.World->GetFirstPlayerController();
					ASSERT_THAT(IsTrue(PlayerController && PlayerController->PlayerState));
					ShooterPlayerId = PlayerController->PlayerState->GetPlayerId();
					UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PlayerController->PlayerState);
					const FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecFromClass(URpgNetworkAutomationShotAbility::StaticClass());
					ASSERT_THAT(IsNotNull(Spec));
					ASSERT_THAT(IsTrue(AbilitySystem->TryActivateAbility(Spec->Handle)));
					DrawStartSeconds = FPlatformTime::Seconds();
					DrawStartWorldSeconds = State.World->GetTimeSeconds();
				});
	}

	TEST_METHOD(ClientShotLaunchesOneArrowAndPaysOnce)
	{
		using namespace RpgCombatProjectilePIETests;
		QueueClientDraw();
		Network
			.UntilClient(TEXT("The first client holds the full draw"), 0,
				// Game time, which the per-frame step cap can slow below real time under load.
				[this](FNetworkState& State) { return State.World->GetTimeSeconds() - DrawStartWorldSeconds >= 0.7; },
				NetworkTimeout())
			.ThenClient(TEXT("The first client releases"), 0,
				[this](FNetworkState& State)
				{
					const URpgNetworkAutomationShotAbility* Ability = FindShotAbility(State);
					ASSERT_THAT(IsTrue(Ability && Ability->ShotTask));
					Ability->ShotTask->ReleaseShot();
					ASSERT_THAT(IsTrue(Ability->ReleasedCount == 1));
				})
			.UntilServer(TEXT("The server launches the arrow"),
				[this](FNetworkState& State)
				{
					for (TActorIterator<ARpgCombatProjectile> It(State.World); It; ++It)
					{
						ServerShotId = It->GetShotId();
						return true;
					}
					return false;
				},
				NetworkTimeout())
			.UntilServer(TEXT("Late duplicates have time to arrive"),
				[this](FNetworkState&) { return FPlatformTime::Seconds() - DrawStartSeconds >= 2.5; },
				NetworkTimeout())
			.ThenServer(TEXT("The server launched one arrow, consumed one and paid the release once"),
				[this](FNetworkState& State)
				{
					ASSERT_THAT(IsTrue(CountProjectiles(State.World) == 1));
					ASSERT_THAT(IsTrue(ServerShotId != 0));
					for (const APlayerState* PlayerState : GetPlayerStates(State))
					{
						const bool bShooter = PlayerState->GetPlayerId() == ShooterPlayerId;
						ASSERT_THAT(IsTrue(GetArrows(PlayerState) == (bShooter ? StartingArrows - 1 : StartingArrows)));
						ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(GetStamina(PlayerState),
							bShooter ? StartingStamina - URpgNetworkAutomationShotAbility::ReleaseCost : StartingStamina)));
					}
				})
			.UntilClients(TEXT("Every client sees the arrow"),
				[this](FNetworkState& State) { return FindProjectile(State.World, ServerShotId) != nullptr; },
				NetworkTimeout());
	}

	TEST_METHOD(ServerTimesOutAHeldShotWithoutCost)
	{
		using namespace RpgCombatProjectilePIETests;
		QueueClientDraw();
		Network
			.UntilClient(TEXT("The server ends the unreleased draw after its hold limit, also on the client"), 0,
				[](FNetworkState& State)
				{
					const URpgNetworkAutomationShotAbility* Ability = FindShotAbility(State);
					return Ability && !Ability->IsActive();
				},
				NetworkTimeout())
			.ThenClient(TEXT("The client never released"), 0,
				[this](FNetworkState& State)
				{
					const URpgNetworkAutomationShotAbility* Ability = FindShotAbility(State);
					ASSERT_THAT(IsTrue(Ability && Ability->ReleasedCount == 0));
					ASSERT_THAT(IsTrue(State.World->GetTimeSeconds() - DrawStartWorldSeconds
						>= URpgNetworkAutomationShotAbility::MakeShotDefinition().MaxHoldSeconds - 0.5));
				})
			.ThenServer(TEXT("The timed-out draw launched nothing and cost nothing"),
				[this](FNetworkState& State)
				{
					ASSERT_THAT(IsTrue(CountProjectiles(State.World) == 0));
					for (const APlayerState* PlayerState : GetPlayerStates(State))
					{
						ASSERT_THAT(IsTrue(GetArrows(PlayerState) == StartingArrows));
						ASSERT_THAT(IsTrue(FMath::IsNearlyEqual(GetStamina(PlayerState), StartingStamina)));
					}
				});
	}
};

#endif // ENABLE_PIE_NETWORK_TEST
#endif // WITH_DEV_AUTOMATION_TESTS
