// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "SurvivalRpg/Camera/RpgCameraMode.h"
#include "Components/PIENetworkComponent.h"
#include "Network/RpgCombatNetworkTestTypes.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/OverlapResult.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "InputActionValue.h"
#include "InputKeyEventArgs.h"
#include "Editor/UnrealEdEngine.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "UnrealEdGlobals.h"
#include "UObject/StrongObjectPtr.h"
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "MotionWarpingComponent.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Animation/RpgAnimInstance.h"
#include "SurvivalRpg/Core/Character/RpgCharacter.h"
#include "SurvivalRpg/Core/Character/RpgCharacterMovementComponent.h"
#include "SurvivalRpg/Core/Character/RpgDownedComponent.h"
#include "SurvivalRpg/Core/Character/RpgHealthComponent.h"
#include "SurvivalRpg/Core/Character/RpgPawnData.h"
#include "SurvivalRpg/Core/Character/RpgPawnGameplayComponent.h"
#include "SurvivalRpg/Core/Game/Experience/RpgExperienceDefinition.h"
#include "SurvivalRpg/Core/Game/Experience/RpgExperienceManagerComponent.h"
#include "SurvivalRpg/Core/Game/RpgGameModeBase.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/Development/RpgDeveloperSettings.h"
#include "SurvivalRpg/Traversal/RpgGameplayAbility_Mantle.h"
#include "SurvivalRpg/Traversal/RpgTraversalQueryComponent.h"

namespace RpgMantleIntegrationTests
{
	constexpr TCHAR ExperiencePath[] = TEXT("/Game/SurvivalRpg/System/Experiences/RpgGaspMantleExperience.RpgGaspMantleExperience_C");
	constexpr TCHAR PawnDataPath[] = TEXT("/Game/SurvivalRpg/Characters/GASP/CMC/RPG/Traversal/DA_PawnData_GaspMantle.DA_PawnData_GaspMantle");
	constexpr TCHAR PawnClassPath[] = TEXT("/Game/SurvivalRpg/Characters/GASP/CMC/RPG/Traversal/BP_RpgGasp_MantleCharacter.BP_RpgGasp_MantleCharacter_C");
	constexpr TCHAR AbilityClassPath[] = TEXT("/Game/SurvivalRpg/Characters/GASP/CMC/RPG/Traversal/GA_RpgGasp_Mantle.GA_RpgGasp_Mantle_C");
	constexpr TCHAR ObstacleClassPath[] = TEXT("/Game/SurvivalRpg/Characters/GASP/CMC/RPG/Traversal/BP_RpgMantleObstacle.BP_RpgMantleObstacle_C");
	constexpr TCHAR GameModePath[] = TEXT("/Game/SurvivalRpg/Maps/Test/GaspMantle/BP_Rpg_GaspMantleTestGameMode.BP_Rpg_GaspMantleTestGameMode_C");
	constexpr TCHAR CmcPawnDataPath[] = TEXT("/Game/SurvivalRpg/Characters/GASP/CMC/RPG/DA_PawnData_GaspCMC.DA_PawnData_GaspCMC");

	FPrimaryAssetId ExperienceId()
	{
		return FPrimaryAssetId(URpgExperienceDefinition::StaticClass()->GetFName(), TEXT("RpgGaspMantleExperience"));
	}

	const URpgGameplayAbility_Mantle* AbilityDefinition()
	{
		UClass* Class = LoadClass<URpgGameplayAbility_Mantle>(nullptr, AbilityClassPath);
		return Class ? Class->GetDefaultObject<URpgGameplayAbility_Mantle>() : nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRpgMantleCompositionTest,
	"SurvivalRpg.GASP.Mantle.AssetComposition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRpgMantleCompositionTest::RunTest(const FString& Parameters)
{
	using namespace RpgMantleIntegrationTests;
	const UClass* ExperienceClass = LoadClass<URpgExperienceDefinition>(nullptr, ExperiencePath);
	const URpgExperienceDefinition* Experience = ExperienceClass ? ExperienceClass->GetDefaultObject<URpgExperienceDefinition>() : nullptr;
	const URpgPawnData* PawnData = LoadObject<URpgPawnData>(nullptr, PawnDataPath);
	const URpgPawnData* CmcPawnData = LoadObject<URpgPawnData>(nullptr, CmcPawnDataPath);
	const UClass* PawnClass = LoadClass<ARpgCharacter>(nullptr, PawnClassPath);
	const UClass* ObstacleClass = LoadClass<AActor>(nullptr, ObstacleClassPath);
	const UClass* GameModeClass = LoadClass<ARpgGameModeBase>(nullptr, GameModePath);
	const URpgGameplayAbility_Mantle* Ability = AbilityDefinition();
	if (!TestNotNull(TEXT("Mantle Experience loads"), Experience)
		|| !TestNotNull(TEXT("Mantle PawnData loads"), PawnData)
		|| !TestNotNull(TEXT("Existing CMC PawnData loads"), CmcPawnData)
		|| !TestNotNull(TEXT("Mantle pawn is an RPG character"), PawnClass)
		|| !TestNotNull(TEXT("Prepared obstacle Blueprint loads"), ObstacleClass)
		|| !TestNotNull(TEXT("Isolated test GameMode loads"), GameModeClass)
		|| !TestNotNull(TEXT("Concrete mantle ability loads"), Ability))
	{
		return false;
	}
	TestTrue(TEXT("Experience selects the isolated mantle PawnData"), Experience->DefaultPawnData == PawnData);
	TestTrue(TEXT("Mantle pawn extends the accepted CMC pawn"), PawnClass->IsChildOf(CmcPawnData->PawnClass));
	TestTrue(TEXT("PawnData selects the isolated mantle pawn"), PawnData->PawnClass.Get() == PawnClass);
	TestTrue(TEXT("The traversal camera uses the RPG camera mechanism"),
		PawnData->DefaultCameraMode && PawnData->DefaultCameraMode->IsChildOf(URpgCameraMode::StaticClass()));
	TestTrue(TEXT("Existing input and RPG inventory composition are preserved"),
		PawnData->InputConfig && PawnData->InputConfig == CmcPawnData->InputConfig
		&& PawnData->InventoryLayoutDefinition == CmcPawnData->InventoryLayoutDefinition);
	for (const URpgAbilitySet* ExistingSet : CmcPawnData->AbilitySets)
	{
		TestTrue(TEXT("Existing CMC startup grants remain composed"), PawnData->AbilitySets.Contains(ExistingSet));
	}
	TestTrue(TEXT("Prepared obstacles are available to remote prediction"), ObstacleClass->GetDefaultObject<AActor>()->GetIsReplicated());
	TestTrue(TEXT("Mantle uses GAS local prediction"), Ability->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::LocalPredicted);
	const UCharacterMovementComponent* MovementDefaults = PawnClass->GetDefaultObject<ARpgCharacter>()->GetCharacterMovement();
	TestTrue(TEXT("Movement can replicate the simulated-proxy mantle collision lease"), MovementDefaults->GetIsReplicated());
	TestFalse(TEXT("Mantle keeps authoritative movement error checks enabled"), MovementDefaults->bIgnoreClientMovementErrorChecksAndCorrection);
	TestFalse(TEXT("Mantle does not accept unchecked client positions"), MovementDefaults->bServerAcceptClientAuthoritativePosition);
	const ARpgCharacter* Pawn = PawnClass->GetDefaultObject<ARpgCharacter>();
	const ARpgCharacter* CmcPawn = CmcPawnData->PawnClass->GetDefaultObject<ARpgCharacter>();
	TestTrue(TEXT("The accepted CMC gameplay mesh and AnimBP remain in use"),
		Pawn->GetMesh()->GetSkeletalMeshAsset() == CmcPawn->GetMesh()->GetSkeletalMeshAsset()
		&& Pawn->GetMesh()->GetAnimClass() == CmcPawn->GetMesh()->GetAnimClass());
	// The concrete chooser selects the montage at runtime. The PIE contract checks the selected asset and its root motion.
	const ARpgGameModeBase* GameMode = GameModeClass->GetDefaultObject<ARpgGameModeBase>();
	const ARpgGameModeBase* Baseline = GetDefault<ARpgGameModeBase>();
	TestFalse(TEXT("Mantle demonstration map disables disk persistence"), GameMode->bEnableDiskPersistence);
	TestTrue(TEXT("Test slots and offline profile are isolated from production"),
		GameMode->WorldSaveSlotName != Baseline->WorldSaveSlotName
		&& GameMode->WorldSaveBackupSlotName != Baseline->WorldSaveBackupSlotName
		&& GameMode->WorldSaveRecoverySlotName != Baseline->WorldSaveRecoverySlotName
		&& GameMode->OfflineProfileKey != Baseline->OfflineProfileKey);
	return true;
}

#if ENABLE_PIE_NETWORK_TEST

namespace RpgMantleIntegrationTests
{
	/** CQTest can retain raw world pointers after an external automation stop has torn its PIE worlds down. */
	bool IsActiveTestWorld(const UWorld* World)
	{
		if (!World || !GEngine) return false;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			// Compare pointer identity before touching World or constructing a weak UObject pointer from it.
			UWorld* ContextWorld = Context.World();
			if (Context.WorldType == EWorldType::PIE && ContextWorld == World)
			{
				return IsValid(ContextWorld) && !ContextWorld->bIsTearingDown && !ContextWorld->IsBeingCleanedUp();
			}
		}
		return false;
	}

	/** Installed before temporary PIE worlds initialize, so neither loading nor teardown can touch user saves. */
	class FScopedSaveIsolation final
	{
	public:
		~FScopedSaveIsolation() { FGameModeEvents::OnGameModeInitializedEvent().Remove(Handle); }
		void Start()
		{
			Prefix = TEXT("SurvivalRpg_MantleAutomation_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
			Handle = FGameModeEvents::OnGameModeInitializedEvent().AddRaw(this, &FScopedSaveIsolation::OnInitialized);
		}
		bool IsIsolated(UWorld* World) const
		{
			if (!IsActiveTestWorld(World)) return false;
			if (World->GetNetMode() == NM_Client) return World->GetAuthGameMode() == nullptr;
			const ARpgGameModeBase* GameMode = World->GetAuthGameMode<ARpgGameModeBase>();
			return GameMode && !GameMode->bEnableDiskPersistence && GameMode->WorldSaveSlotName.StartsWith(Prefix)
				&& GameMode->WorldSaveBackupSlotName == GameMode->WorldSaveSlotName + TEXT("_Backup")
				&& GameMode->WorldSaveRecoverySlotName == GameMode->WorldSaveSlotName + TEXT("_Recovery")
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

	/** Applies latency only to this fixture's existing PIE drivers and restores every driver before teardown. */
	class FScopedLatency final
	{
	public:
		~FScopedLatency() { Stop(); }
		bool Start(int32 Milliseconds)
		{
			Stop();
#if DO_ENABLE_NET_TEST
			if (!GEngine) return false;
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				UWorld* World = Context.WorldType == EWorldType::PIE ? Context.World() : nullptr;
				UNetDriver* Driver = IsActiveTestWorld(World) ? World->GetNetDriver() : nullptr;
				if (!Driver) continue;
				OriginalSettings.Emplace(Driver, Driver->PacketSimulationSettings);
				FPacketSimulationSettings Settings;
				Settings.PktLag = Milliseconds;
				Driver->SetPacketSimulationSettings(Settings);
			}
			return OriginalSettings.Num() >= 3;
#else
			return false;
#endif
		}
		void Stop()
		{
#if DO_ENABLE_NET_TEST
			for (const auto& Entry : OriginalSettings)
			{
				if (Entry.Key.IsValid()) Entry.Key->SetPacketSimulationSettings(Entry.Value);
			}
			OriginalSettings.Reset();
#endif
		}
	private:
#if DO_ENABLE_NET_TEST
		TArray<TPair<TWeakObjectPtr<UNetDriver>, FPacketSimulationSettings>> OriginalSettings;
#endif
	};

	ARpgCharacter* LocalCharacter(UWorld* World)
	{
		const APlayerController* Controller = IsActiveTestWorld(World) ? World->GetFirstPlayerController() : nullptr;
		return Controller ? Cast<ARpgCharacter>(Controller->GetPawn()) : nullptr;
	}
	ARpgPlayerState* FindPlayerState(UWorld* World, int32 PlayerId)
	{
		const AGameStateBase* GameState = IsActiveTestWorld(World) ? World->GetGameState() : nullptr;
		if (!GameState) return nullptr;
		for (APlayerState* PlayerState : GameState->PlayerArray)
		{
			if (PlayerState && PlayerState->GetPlayerId() == PlayerId) return Cast<ARpgPlayerState>(PlayerState);
		}
		return nullptr;
	}
	ARpgCharacter* FindCharacter(UWorld* World, int32 PlayerId)
	{
		const ARpgPlayerState* PlayerState = FindPlayerState(World, PlayerId);
		return PlayerState ? PlayerState->GetPawn<ARpgCharacter>() : nullptr;
	}
	FGameplayAbilitySpec* MantleSpec(ARpgCharacter* Character)
	{
		URpgAbilitySystemComponent* ASC = Character ? Character->GetRpgAbilitySystemComponent() : nullptr;
		if (!ASC) return nullptr;
		for (FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			if (!Spec.PendingRemove && Spec.Ability && Spec.Ability->IsA<URpgGameplayAbility_Mantle>()) return &Spec;
		}
		return nullptr;
	}
	UPrimitiveComponent* ObstacleInWorld(UWorld* World)
	{
		if (!IsActiveTestWorld(World)) return nullptr;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->GetClass()->GetPathName() != ObstacleClassPath) continue;
			TInlineComponentArray<UPrimitiveComponent*> Components(*It);
			for (UPrimitiveComponent* Component : Components)
			{
				if (Component && Component->IsQueryCollisionEnabled() && !Component->IsSimulatingPhysics()
					&& Component->GetCollisionResponseToChannel(ECC_GameTraceChannel1) == ECR_Block) return Component;
			}
		}
		return nullptr;
	}
	bool HasTraversalCandidate(const ARpgCharacter* Character)
	{
		const URpgGameplayAbility_Mantle* Definition = AbilityDefinition();
		FRpgTraversalQueryResult Candidate;
		return Character && Definition && Definition->FindTraversalCandidate(*Character, Candidate)
			&& Candidate.HitComponent == ObstacleInWorld(Character->GetWorld());
	}
	bool QueryLandingPosition(const ARpgCharacter& Character, FVector& OutPosition)
	{
		const URpgGameplayAbility_Mantle* Definition = AbilityDefinition();
		FRpgTraversalQueryResult Candidate;
		if (!Definition || !Definition->FindTraversalCandidate(Character, Candidate)
			|| !Definition->GetMantleLandingLocation(Character, Candidate, OutPosition)) return false;
		OutPosition.Z += Character.GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.5;
		return true;
	}
	bool Ready(UWorld* World, ARpgCharacter* Character)
	{
		if (!IsActiveTestWorld(World)) return false;
		const AGameStateBase* GameState = World->GetGameState();
		const URpgExperienceManagerComponent* Manager = GameState ? GameState->FindComponentByClass<URpgExperienceManagerComponent>() : nullptr;
		return World && World->GetNetDriver() && Manager && Manager->IsExperienceLoaded()
			&& Manager->GetCurrentExperienceChecked()->GetPrimaryAssetId() == ExperienceId()
			&& Character && Character->GetClass()->GetPathName() == PawnClassPath && Character->GetPlayerState()
			&& Character->GetRpgAbilitySystemComponent() && Character->GetMesh()->GetAnimInstance()
			&& Character->GetMesh()->GetAnimInstance()->IsA<URpgAnimInstance>()
			&& Character->FindComponentByClass<UMotionWarpingComponent>()
			&& Character->FindComponentByClass<URpgTraversalQueryComponent>();
	}
	bool Grounded(const ARpgCharacter* Character)
	{
		return Character && Character->GetCharacterMovement()->IsMovingOnGround() && Character->GetVelocity().Size2D() < 5.0;
	}
	// The original standing query sweeps 75 cm with a 30 cm radius; this idealized lifecycle fixture starts inside that reach.
	FVector EntryPosition(const ARpgCharacter& Character, const UPrimitiveComponent& Obstacle, float Distance = 100.0f)
	{
		const FBox Bounds = Obstacle.Bounds.GetBox();
		FVector Position(Bounds.Min.X - Distance, Bounds.GetCenter().Y, 0.0);
		Position.Z = Character.GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.5;
		return Position;
	}
	FVector LandingPosition(const ARpgCharacter& Character, const UPrimitiveComponent& Obstacle)
	{
		const FBox Bounds = Obstacle.Bounds.GetBox();
		return FVector(Bounds.Min.X + 50.0, Bounds.GetCenter().Y,
			Bounds.Max.Z + Character.GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.5);
	}
	bool HasCleanMantleState(ARpgCharacter* Character)
	{
		if (!Character) return false;
		const FGameplayAbilitySpec* Spec = MantleSpec(Character);
		const UMotionWarpingComponent* Warping = Character->FindComponentByClass<UMotionWarpingComponent>();
		if (!Warping || (Spec && Spec->IsActive()) || Warping->FindWarpTarget(TEXT("FrontLedge"))
			|| Warping->FindWarpTarget(TEXT("BackLedge")) || Warping->FindWarpTarget(TEXT("BackFloor"))) return false;
		const URpgAbilitySystemComponent* ASC = Character->GetRpgAbilitySystemComponent();
		const UAnimInstance* Animation = Character->GetMesh()->GetAnimInstance();
		const URpgTraversalQueryComponent* Query = Character->FindComponentByClass<URpgTraversalQueryComponent>();
		if (!Query) return false;
		// Death may synchronously take over the same gameplay slot. Require every allowed traversal montage to stop,
		// while permitting the successor ability's montage to play and supply its own legitimate root motion.
		if (Animation)
		{
			for (const FRpgTraversalAnimationEntry& Entry : Query->AllowedMantleAnimations)
			{
				if (Entry.Montage && Animation->Montage_IsPlaying(Entry.Montage)) return false;
			}
		}
		const UGameplayAbility* AnimatingAbility = ASC ? ASC->GetAnimatingAbility() : nullptr;
		if (AnimatingAbility && AnimatingAbility->IsA<URpgGameplayAbility_Mantle>()) return false;
		const UPrimitiveComponent* Obstacle = ObstacleInWorld(Character->GetWorld());
		const URpgCharacterMovementComponent* Movement = Cast<URpgCharacterMovementComponent>(Character->GetCharacterMovement());
		return Movement && !Movement->GetMantleCollisionComponent()
			&& (!Obstacle || !Character->GetCapsuleComponent()->GetMoveIgnoreComponents().Contains(Obstacle))
			&& Character->GetCharacterMovement()->MovementMode != MOVE_Flying;
	}
	bool CompletedMantle(UWorld* World, int32 PlayerId)
	{
		ARpgCharacter* Character = FindCharacter(World, PlayerId);
		const UPrimitiveComponent* Obstacle = ObstacleInWorld(World);
		if (!Obstacle || !Grounded(Character) || !HasCleanMantleState(Character)) return false;
		const FBox Bounds = Obstacle->Bounds.GetBox();
		const FVector Position = Character->GetActorLocation();
		const double FeetZ = Position.Z - Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		return Character->GetCharacterMovement()->CurrentFloor.HitResult.GetComponent() == Obstacle
			&& Position.X > Bounds.Min.X && Position.X < Bounds.Max.X && Position.Y > Bounds.Min.Y && Position.Y < Bounds.Max.Y
			&& FMath::Abs(FeetZ - Bounds.Max.Z) < 8.0;
	}
	bool ReadyAtEntry(UWorld* World, int32 PlayerId)
	{
		ARpgCharacter* Character = FindCharacter(World, PlayerId);
		const UPrimitiveComponent* Obstacle = ObstacleInWorld(World);
		if (!Character) return false;
		const AController* Controller = Character->GetController();
		const bool bRequiresController = Character->HasAuthority() || Character->GetLocalRole() == ROLE_AutonomousProxy;
		const bool bViewAligned = !bRequiresController || (Controller
			&& FMath::Abs(FRotator::NormalizeAxis(Controller->GetControlRotation().Yaw)) < 1.0);
		return Ready(World, Character) && Grounded(Character) && HasCleanMantleState(Character) && Obstacle
			&& bViewAligned && FMath::Abs(FRotator::NormalizeAxis(Character->GetActorRotation().Yaw)) < 1.0
			&& Character->GetActorLocation().Equals(EntryPosition(*Character, *Obstacle), 12.0) && HasTraversalCandidate(Character);
	}
	void PressJump(ARpgCharacter* Character)
	{
		if (URpgPawnGameplayComponent* Input = URpgPawnGameplayComponent::FindPawnGameplayComponent(Character))
		{
			Input->Input_Jump(FInputActionValue(true));
		}
	}
	void ReleaseJump(ARpgCharacter* Character)
	{
		if (URpgPawnGameplayComponent* Input = URpgPawnGameplayComponent::FindPawnGameplayComponent(Character))
		{
			Input->Input_StopJump(FInputActionValue(false));
		}
	}
	void PositionAtEntry(UWorld* World, int32 PlayerId)
	{
		ARpgCharacter* Character = FindCharacter(World, PlayerId);
		UPrimitiveComponent* Obstacle = ObstacleInWorld(World);
		if (!Character || !Obstacle || !Character->HasAuthority()) return;
		const FVector Requested = EntryPosition(*Character, *Obstacle);
		const bool bTeleported = Character->TeleportTo(Requested, FRotator::ZeroRotator);
		if (!bTeleported || !Character->GetActorLocation().Equals(Requested, 1.0))
		{
			UE_LOG(LogTemp, Display, TEXT("RpgMantleFixturePlacement success=%d requested=%s actual=%s"),
				bTeleported, *Requested.ToCompactString(), *Character->GetActorLocation().ToCompactString());
		}
		if (APlayerController* Controller = Cast<APlayerController>(Character->GetController()))
		{
			// This standing fixture specifies a forward-facing entry. A pawn teleport alone leaves the owner's
			// view heading unchanged, so its next FaceRotation can undo the authoritative entry rotation.
			Controller->SetControlRotation(FRotator::ZeroRotator);
			Controller->ClientSetRotation(FRotator::ZeroRotator, true);
		}
		Character->GetCharacterMovement()->StopMovementImmediately();
		Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		Character->ForceNetUpdate();
	}

	struct FObservation
	{
		FVector StartLocation = FVector::ZeroVector;
		float MaxHeight = 0.0f;
		float MaxDistance = 0.0f;
		float FirstMontagePosition = -1.0f;
		float LastMontagePosition = -1.0f;
		int32 Activations = 0;
		int32 Commits = 0;
		int32 Ends = 0;
		int32 Cancellations = 0;
		double StartTime = 0.0;
		FString LastSnapshot;
		bool bWasMontagePlaying = false;
		bool bReportedAfterFiveSeconds = false;
		bool bRootMotion = false;
		bool bFlying = false;
		bool bCollisionLease = false;
		bool bDisabledCorrection = false;
	};

	/** Samples all worlds concurrently so short montage windows cannot be missed by sequential latent assertions. */
	class FScopedObservations final
	{
	public:
		~FScopedObservations() { Stop(); }
		void Start(int32 InPlayerId)
		{
			Stop();
			Records.Reset();
			PlayerId = InPlayerId;
			++ObservationId;
			if (!GEngine) return;
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				UWorld* World = Context.World();
				ARpgCharacter* Character = Context.WorldType == EWorldType::PIE ? FindCharacter(World, PlayerId) : nullptr;
				if (!Character) continue;
				FObservation& Record = Records.FindOrAdd(World);
				Record.StartLocation = Character->GetActorLocation();
				Record.StartTime = World->GetTimeSeconds();
				ReportTransforms(TEXT("before_activation"), *Character);
				if (URpgAbilitySystemComponent* ASC = Character->GetRpgAbilitySystemComponent())
				{
					FSubscriptions Subscription;
					Subscription.ASC = ASC;
					Subscription.Activated = ASC->AbilityActivatedCallbacks.AddLambda([this, World](UGameplayAbility* Ability)
					{
						if (IsActiveTestWorld(World) && Ability && Ability->IsA<URpgGameplayAbility_Mantle>()) ++Records.FindOrAdd(World).Activations;
					});
					Subscription.Committed = ASC->AbilityCommittedCallbacks.AddLambda([this, World](UGameplayAbility* Ability)
					{
						if (IsActiveTestWorld(World) && Ability && Ability->IsA<URpgGameplayAbility_Mantle>()) ++Records.FindOrAdd(World).Commits;
					});
					Subscription.Ended = ASC->OnAbilityEnded.AddLambda([this, World](const FAbilityEndedData& Data)
					{
						if (IsActiveTestWorld(World) && Data.AbilityThatEnded && Data.AbilityThatEnded->IsA<URpgGameplayAbility_Mantle>())
						{
							FObservation& EndRecord = Records.FindOrAdd(World);
							++EndRecord.Ends;
							EndRecord.Cancellations += Data.bWasCancelled ? 1 : 0;
						}
					});
					Subscriptions.Add(Subscription);
				}
			}
			TickHandle = FWorldDelegates::OnWorldTickEnd.AddRaw(this, &FScopedObservations::OnTick);
		}
		void Stop()
		{
			if (TickHandle.IsValid())
			{
				for (const auto& Entry : Records) Report(TEXT("observation_end"), Entry.Value);
			}
			FWorldDelegates::OnWorldTickEnd.Remove(TickHandle);
			TickHandle.Reset();
			for (const auto& Subscription : Subscriptions)
			{
				if (Subscription.ASC.IsValid())
				{
					Subscription.ASC->AbilityActivatedCallbacks.Remove(Subscription.Activated);
					Subscription.ASC->AbilityCommittedCallbacks.Remove(Subscription.Committed);
					Subscription.ASC->OnAbilityEnded.Remove(Subscription.Ended);
				}
			}
			Subscriptions.Reset();
		}
		const FObservation* Find(UWorld* World) const { return IsActiveTestWorld(World) ? Records.Find(World) : nullptr; }
		bool SawRootMotion(UWorld* World) const
		{
			const FObservation* Record = Find(World);
			return Record && Record->bRootMotion && Record->bFlying && Record->bCollisionLease && Record->MaxHeight > 25.0f
				&& Record->MaxDistance > 40.0f && Record->LastMontagePosition > Record->FirstMontagePosition + 0.1f;
		}
	private:
		void ReportTransforms(const TCHAR* Phase, const ARpgCharacter& Character) const
		{
			const UCharacterMovementComponent* Movement = Character.GetCharacterMovement();
			const USkeletalMeshComponent* Mesh = Character.GetMesh();
			const AController* Controller = Character.GetController();
			const UPrimitiveComponent* Obstacle = ObstacleInWorld(Character.GetWorld());
			const FBox Bounds = Obstacle ? Obstacle->Bounds.GetBox() : FBox(ForceInit);
			const UMotionWarpingComponent* Warping = Character.FindComponentByClass<UMotionWarpingComponent>();
			const FMotionWarpingTarget* Warp = Warping ? Warping->FindWarpTarget(TEXT("FrontLedge")) : nullptr;
			const URpgAbilitySystemComponent* ASC = Character.GetRpgAbilitySystemComponent();
			const URpgGameplayAbility_Mantle* Ability = ASC ? Cast<URpgGameplayAbility_Mantle>(ASC->GetAnimatingAbility()) : nullptr;
			// Read only existing transforms and the active proposal; querying again would change the pose-search history.
			UE_LOG(LogTemp, Display, TEXT("RpgMantleFixtureTransforms observation=%d phase=%s role=%d position=%s actorRotation=%s controlRotation=%s meshWorldRotation=%s meshRelativeRotation=%s velocity=%s acceleration=%s controllerYaw=%d physicsRotationDuringRootMotion=%d obstacleMin=%s obstacleMax=%s obstacleRotation=%s montage=%s start=%.4f hasWarp=%d warpPosition=%s warpRotation=%s"),
				ObservationId, Phase, static_cast<int32>(Character.GetLocalRole()), *Character.GetActorLocation().ToCompactString(),
				*Character.GetActorRotation().ToCompactString(), Controller ? *Controller->GetControlRotation().ToCompactString() : TEXT("None"),
				Mesh ? *Mesh->GetComponentRotation().ToCompactString() : TEXT("None"), Mesh ? *Mesh->GetRelativeRotation().ToCompactString() : TEXT("None"),
				*Character.GetVelocity().ToCompactString(), Movement ? *Movement->GetCurrentAcceleration().ToCompactString() : TEXT("None"),
				Character.bUseControllerRotationYaw, Movement && Movement->bAllowPhysicsRotationDuringAnimRootMotion,
				*Bounds.Min.ToCompactString(), *Bounds.Max.ToCompactString(), Obstacle ? *Obstacle->GetComponentRotation().ToCompactString() : TEXT("None"),
				*GetPathNameSafe(Ability ? Ability->Montage.Get() : nullptr), Ability ? Ability->StartTimeSeconds : -1.0f,
				Warp != nullptr, Warp ? *Warp->GetLocation().ToCompactString() : TEXT("None"), Warp ? *Warp->Rotator().ToCompactString() : TEXT("None"));
		}
		static void Report(const TCHAR* Reason, const FObservation& Record)
		{
			// One parseable line per transition/wait diagnostic, including a cached last sample if PIE was already torn down.
			UE_LOG(LogTemp, Display, TEXT("RpgMantleTestObservation {\"reason\":\"%s\",\"sample\":%s,\"observed\":{\"activations\":%d,\"commits\":%d,\"ends\":%d,\"cancellations\":%d,\"maxHeight\":%.3f,\"maxDistance\":%.3f,\"firstMontagePosition\":%.3f,\"lastMontagePosition\":%.3f,\"rootMotion\":%d,\"flying\":%d,\"collisionLease\":%d,\"disabledCorrection\":%d}}"),
				Reason, Record.LastSnapshot.IsEmpty() ? TEXT("null") : *Record.LastSnapshot,
				Record.Activations, Record.Commits, Record.Ends, Record.Cancellations, Record.MaxHeight, Record.MaxDistance,
				Record.FirstMontagePosition, Record.LastMontagePosition, Record.bRootMotion, Record.bFlying,
				Record.bCollisionLease, Record.bDisabledCorrection);
		}

		void OnTick(UWorld* World, ELevelTick TickType, float DeltaSeconds)
		{
			if (!IsActiveTestWorld(World)) return;
			FObservation* Record = Records.Find(World);
			ARpgCharacter* Character = Record ? FindCharacter(World, PlayerId) : nullptr;
			if (!Character) return;
			const URpgCharacterMovementComponent* Movement = Cast<URpgCharacterMovementComponent>(Character->GetCharacterMovement());
			Record->bDisabledCorrection |= Character->GetCharacterMovement()->bIgnoreClientMovementErrorChecksAndCorrection
				|| Character->GetCharacterMovement()->bServerAcceptClientAuthoritativePosition;
			Record->MaxHeight = FMath::Max(Record->MaxHeight, static_cast<float>(Character->GetActorLocation().Z - Record->StartLocation.Z));
			Record->MaxDistance = FMath::Max(Record->MaxDistance, static_cast<float>(FVector::Dist2D(Record->StartLocation, Character->GetActorLocation())));
			UAnimInstance* Animation = Character->GetMesh()->GetAnimInstance();
			UAnimMontage* Montage = Animation ? Animation->GetCurrentActiveMontage() : nullptr;
			const bool bMontagePlaying = Animation && Montage && Animation->Montage_IsPlaying(Montage);
			if (bMontagePlaying)
			{
				const float Position = Animation->Montage_GetPosition(Montage);
				if (Record->FirstMontagePosition < 0.0f) Record->FirstMontagePosition = Position;
				Record->LastMontagePosition = Position;
				Record->bRootMotion |= Character->IsPlayingRootMotion();
				Record->bFlying |= Character->GetCharacterMovement()->MovementMode == MOVE_Flying;
				const UPrimitiveComponent* Obstacle = ObstacleInWorld(World);
				Record->bCollisionLease |= Movement && Obstacle && Movement->GetIsReplicated()
					&& Movement->GetMantleCollisionComponent() == Obstacle
					&& Character->GetCapsuleComponent()->GetMoveIgnoreComponents().Contains(Obstacle);
			}
			const UPrimitiveComponent* Obstacle = ObstacleInWorld(World);
			const FVector Actual = Character->GetActorLocation();
			const FVector Expected = Obstacle ? LandingPosition(*Character, *Obstacle) : FVector::ZeroVector;
			const FGameplayAbilitySpec* Spec = MantleSpec(Character);
			const URpgAbilitySystemComponent* ASC = Character->GetRpgAbilitySystemComponent();
			const UMotionWarpingComponent* Warping = Character->FindComponentByClass<UMotionWarpingComponent>();
			Record->LastSnapshot = FString::Printf(TEXT("{\"observation\":%d,\"world\":\"%s\",\"netMode\":%d,\"role\":%d,\"elapsed\":%.3f,\"position\":[%.3f,%.3f,%.3f],\"expectedLanding\":[%.3f,%.3f,%.3f],\"landingDistance\":%.3f,\"mode\":%d,\"speed2D\":%.3f,\"grounded\":%d,\"clean\":%d,\"specActive\":%d,\"animationPlaying\":%d,\"montagePosition\":%.3f,\"animMontage\":\"%s\",\"ascMontage\":\"%s\",\"lease\":\"%s\",\"warpTarget\":%d}"),
				ObservationId, *World->GetPathName().ReplaceCharWithEscapedChar(), static_cast<int32>(World->GetNetMode()),
				static_cast<int32>(Character->GetLocalRole()), World->GetTimeSeconds() - Record->StartTime,
				Actual.X, Actual.Y, Actual.Z, Expected.X, Expected.Y, Expected.Z, FVector::Dist(Actual, Expected),
				static_cast<int32>(Character->GetCharacterMovement()->MovementMode), Character->GetVelocity().Size2D(),
				Grounded(Character), HasCleanMantleState(Character), Spec && Spec->IsActive(), bMontagePlaying,
				Animation && Montage ? Animation->Montage_GetPosition(Montage) : -1.0f,
				*GetPathNameSafe(Animation ? Animation->GetCurrentActiveMontage() : nullptr).ReplaceCharWithEscapedChar(),
				*GetPathNameSafe(ASC ? ASC->GetCurrentMontage() : nullptr).ReplaceCharWithEscapedChar(),
				*GetPathNameSafe(Movement ? Movement->GetMantleCollisionComponent() : nullptr).ReplaceCharWithEscapedChar(),
				Warping && Warping->FindWarpTarget(TEXT("FrontLedge")));
			if (!Record->bWasMontagePlaying && bMontagePlaying)
			{
				Report(TEXT("montage_started"), *Record);
				ReportTransforms(TEXT("montage_started"), *Character);
			}
			if (Record->bWasMontagePlaying && !bMontagePlaying) Report(TEXT("montage_stopped"), *Record);
			Record->bWasMontagePlaying = bMontagePlaying;
			if (!Record->bReportedAfterFiveSeconds && World->GetTimeSeconds() - Record->StartTime >= 5.0)
			{
				Record->bReportedAfterFiveSeconds = true;
				Report(TEXT("five_second_checkpoint"), *Record);
			}
		}
		struct FSubscriptions
		{
			TWeakObjectPtr<URpgAbilitySystemComponent> ASC;
			FDelegateHandle Activated;
			FDelegateHandle Committed;
			FDelegateHandle Ended;
		};
		int32 PlayerId = INDEX_NONE;
		int32 ObservationId = 0;
		TMap<TWeakObjectPtr<UWorld>, FObservation> Records;
		TArray<FSubscriptions> Subscriptions;
		FDelegateHandle TickHandle;
	};

	struct FState : FBasePIENetworkComponentState
	{
		ARpgCombatNetworkFloorFixture* Floor = nullptr;
		TWeakObjectPtr<ARpgCharacter> PawnBeforeDeath;
		TWeakObjectPtr<URpgAbilitySystemComponent> PersistentASC;
		TWeakObjectPtr<AActor> LandingBlocker;
		FTransform DestroyedObstacleTransform = FTransform::Identity;
	};

	AActor* SpawnPreparedObstacle(UWorld* World, const FTransform& Transform)
	{
		if (!IsActiveTestWorld(World)) return nullptr;
		UClass* ObstacleClass = LoadClass<AActor>(nullptr, ObstacleClassPath);
		if (!ObstacleClass) return nullptr;
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		return World->SpawnActor<AActor>(ObstacleClass, Transform, Params);
	}

	void ReportEntryBlockers(UWorld* World, const ARpgCharacter& Character, const UPrimitiveComponent& Obstacle)
	{
		if (!IsActiveTestWorld(World)) return;
		const UCapsuleComponent* Capsule = Character.GetCapsuleComponent();
		TArray<FOverlapResult> Overlaps;
		World->OverlapMultiByChannel(Overlaps, EntryPosition(Character, Obstacle), Capsule->GetComponentQuat(),
			Capsule->GetCollisionObjectType(), FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()),
			FCollisionQueryParams(SCENE_QUERY_STAT(RpgMantleFixtureEntry), false, &Character),
			FCollisionResponseParams(Capsule->GetCollisionResponseToChannels()));
		for (const FOverlapResult& Overlap : Overlaps)
		{
			if (Overlap.bBlockingHit)
			{
				UE_LOG(LogTemp, Display, TEXT("RpgMantleFixtureEntryBlocker actor=%s component=%s"),
					*GetPathNameSafe(Overlap.GetActor()), *GetPathNameSafe(Overlap.GetComponent()));
			}
		}
	}

	/** Creates temporary authority-only obstruction geometry for a synchronous server validation check. */
	AActor* SpawnBlocker(UWorld* World, const FVector& Location, const FVector& Extent)
	{
		if (!IsActiveTestWorld(World)) return nullptr;
		AActor* Actor = World->SpawnActor<AActor>();
		if (!Actor) return nullptr;
		UBoxComponent* Box = NewObject<UBoxComponent>(Actor);
		Actor->SetRootComponent(Box);
		Actor->AddInstanceComponent(Box);
		Box->InitBoxExtent(Extent);
		Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Box->SetCollisionResponseToAllChannels(ECR_Block);
		Box->SetWorldLocation(Location);
		Box->SetMobility(EComponentMobility::Static);
		Box->RegisterComponent();
		return Actor;
	}
	ARpgCharacter* RespawnedCharacter(const FState& State, int32 PlayerId)
	{
		ARpgPlayerState* PlayerState = FindPlayerState(State.World, PlayerId);
		ARpgCharacter* Character = FindCharacter(State.World, PlayerId);
		const URpgHealthComponent* Health = URpgHealthComponent::FindHealthComponent(Character);
		URpgAbilitySystemComponent* ASC = Character ? Character->GetRpgAbilitySystemComponent() : nullptr;
		return Ready(State.World, Character) && Character != State.PawnBeforeDeath.Get() && Health && Health->GetHealth() > 0.0f
			&& !Health->IsDeadOrDying() && PlayerState && !PlayerState->IsWaitingForRespawn()
			&& ASC == State.PersistentASC.Get() && ASC == PlayerState->GetRpgAbilitySystemComponent()
			&& ASC->GetAvatarActor() == Character && HasCleanMantleState(Character) ? Character : nullptr;
	}
	FTimespan Timeout() { return FTimespan::FromSeconds(90.0); }
}

NETWORK_TEST_CLASS(GaspMantleExperiencePIE, "SurvivalRpg.GASP.Mantle")
{
	using FState = RpgMantleIntegrationTests::FState;
	RpgMantleIntegrationTests::FScopedSaveIsolation Isolation;
	RpgMantleIntegrationTests::FScopedObservations Observations;
	RpgMantleIntegrationTests::FScopedLatency Latency;
	FPIENetworkComponent<FState> Network{TestRunner, TestCommandBuilder, bInitializing};
	FPrimaryAssetId OriginalExperience;
	bool bConfigured = false;
	int32 SubjectId = INDEX_NONE;

	BEFORE_EACH()
	{
		using namespace RpgMantleIntegrationTests;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && IsValid(Context.World()))
			{
				TestRunner->AddError(TEXT("Mantle automation refuses to interrupt an existing PIE session."));
				return;
			}
		}
		UClass* GameModeClass = LoadClass<ARpgGameModeBase>(nullptr, GameModePath);
		ASSERT_THAT(IsNotNull(GameModeClass));
		ASSERT_THAT(IsNotNull(AbilityDefinition()));
		if (!GameModeClass || !AbilityDefinition()) return;
		Isolation.Start();
		OriginalExperience = GetDefault<URpgDeveloperSettings>()->ExperienceOverride;
		GetMutableDefault<URpgDeveloperSettings>()->ExperienceOverride = ExperienceId();
		bConfigured = true;
		FNetworkComponentBuilder<FState>().WithClients(1).AsListenServer()
			.WithGameInstanceClass(FSoftClassPath(TEXT("/Game/SurvivalRpg/Core/Game/BP_Rpg_GameInstance.BP_Rpg_GameInstance_C")))
			.WithGameMode(GameModeClass).Build(Network);
	}
	AFTER_EACH()
	{
		Observations.Stop();
		Latency.Stop();
		if (bConfigured) GetMutableDefault<URpgDeveloperSettings>()->ExperienceOverride = OriginalExperience;
	}

	TEST_METHOD(AuthoritativeValidationRootMotionLateJoinAndLifecycle)
	{
		using namespace RpgMantleIntegrationTests;
		if (!bConfigured) return;
		Network.ThenServer(TEXT("Save isolation precedes InitGame"), [this](FState& State)
			{ ASSERT_THAT(IsTrue(Isolation.IsIsolated(State.World))); })
			.SpawnAndReplicate<ARpgCombatNetworkFloorFixture, &FState::Floor>(Timeout())
			.UntilClient(TEXT("Owner receives the mantle Experience and startup grant"), 0, [](FState& State)
				{ return Ready(State.World, LocalCharacter(State.World)) && Grounded(LocalCharacter(State.World)) && MantleSpec(LocalCharacter(State.World)); }, Timeout())
			.ThenClient(TEXT("Capture the real autonomous pawn identity"), 0, [this](FState& State)
			{
				ARpgCharacter* Character = LocalCharacter(State.World);
				if (!Character) return;
				ASSERT_THAT(IsTrue(Character->GetLocalRole() == ROLE_AutonomousProxy));
				SubjectId = Character->GetPlayerState()->GetPlayerId();
			})
			.ThenServer(TEXT("Spawn authored prepared geometry and move players to isolated fixture positions"), [this](FState& State)
			{
				if (!IsActiveTestWorld(State.World)) return;
				UClass* ObstacleClass = LoadClass<AActor>(nullptr, ObstacleClassPath);
				ASSERT_THAT(IsNotNull(ObstacleClass));
				if (!ObstacleClass) return;
				FActorSpawnParameters Params;
				Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				const FTransform ObstacleTransform(FRotator::ZeroRotator, FVector(1000.0, -200.0, 0.0), FVector(4.0, 4.0, 1.0));
				AActor* Obstacle = State.World->SpawnActor<AActor>(ObstacleClass, ObstacleTransform, Params);
				ASSERT_THAT(IsNotNull(Obstacle));
				ASSERT_THAT(IsNotNull(ObstacleInWorld(State.World)));
				int32 ParkingIndex = 0;
				for (TActorIterator<ARpgCharacter> It(State.World); It; ++It)
				{
					It->TeleportTo(FVector(-1000.0, 2000.0 + 1000.0 * ParkingIndex++, 100.0), FRotator::ZeroRotator);
					It->GetCharacterMovement()->StopMovementImmediately();
					It->ForceNetUpdate();
				}
				PositionAtEntry(State.World, SubjectId);
			})
			.UntilClient(TEXT("Prepared source cube and authoritative entry position reach the owner"), 0, [this](FState& State)
				{ return ReadyAtEntry(State.World, SubjectId); }, Timeout())
			.ThenServer(TEXT("Server rejects unmarked, distant, excessive-height and obstructed landing proposals"), [this](FState& State)
			{
				ARpgCharacter* Character = FindCharacter(State.World, SubjectId);
				UPrimitiveComponent* Obstacle = ObstacleInWorld(State.World);
				const URpgGameplayAbility_Mantle* Definition = AbilityDefinition();
				ASSERT_THAT(IsNotNull(Character));
				ASSERT_THAT(IsNotNull(Obstacle));
				if (!Character || !Obstacle || !Definition) return;
				FRpgTraversalQueryResult Candidate;
				ASSERT_THAT(IsTrue(Definition->FindTraversalCandidate(*Character, Candidate)));
				ASSERT_THAT(IsTrue(Candidate.HitComponent == Obstacle));
				ASSERT_THAT(IsNotNull(Candidate.ChosenMontage.Get()));
				if (Candidate.ChosenMontage)
				{
					ASSERT_THAT(IsTrue(Candidate.ChosenMontage->HasRootMotion()));
					ASSERT_THAT(IsTrue(Candidate.ChosenMontage->IsValidSlot(TEXT("DefaultSlot"))));
					ASSERT_THAT(IsTrue(Candidate.ChosenMontage->GetPathName().StartsWith(TEXT("/Game/SurvivalRpg/"))));
					ASSERT_THAT(IsTrue(Candidate.ChosenMontage->GetSkeleton() == Character->GetMesh()->GetSkeletalMeshAsset()->GetSkeleton()));
				}
				FVector SelectedLanding = FVector::ZeroVector;
				const bool bHasSelectedLanding = QueryLandingPosition(*Character, SelectedLanding);
				ASSERT_THAT(IsTrue(bHasSelectedLanding));
				if (!bHasSelectedLanding) return;
				const FTransform Entry = Character->GetActorTransform();
				Character->SetActorLocation(Entry.GetLocation() - FVector(600.0, 0.0, 0.0));
				ASSERT_THAT(IsFalse(HasTraversalCandidate(Character)));
				Character->SetActorLocation(Entry.GetLocation() - FVector(0.0, 0.0, 350.0));
				ASSERT_THAT(IsFalse(HasTraversalCandidate(Character)));
				Character->SetActorTransform(Entry);
				AActor* Blocker = SpawnBlocker(State.World, SelectedLanding, FVector(35.0, 35.0, 60.0));
				ASSERT_THAT(IsNotNull(Blocker));
				ASSERT_THAT(IsFalse(HasTraversalCandidate(Character)));
				if (Blocker) Blocker->Destroy();
				Blocker = SpawnBlocker(State.World, Entry.GetLocation() + FVector(65.0, 0.0, 0.0), FVector(12.0, 100.0, 100.0));
				ASSERT_THAT(IsNotNull(Blocker));
				ASSERT_THAT(IsFalse(HasTraversalCandidate(Character)));
				if (Blocker) Blocker->Destroy();
				ASSERT_THAT(IsTrue(HasTraversalCandidate(Character)));
				Character->TeleportTo(EntryPosition(*Character, *Obstacle, 500.0f), FRotator::ZeroRotator);
				Character->GetCharacterMovement()->StopMovementImmediately();
				Character->ForceNetUpdate();
			})
			.UntilClient(TEXT("Owner is grounded outside the prepared entry"), 0, [](FState& State)
				{ ARpgCharacter* Character = LocalCharacter(State.World); return Grounded(Character) && !HasTraversalCandidate(Character); }, Timeout())
			.ThenClient(TEXT("The contextual jump button falls back to ordinary jumping without a candidate"), 0, [](FState& State)
				{ PressJump(LocalCharacter(State.World)); })
			.UntilServer(TEXT("Fallback jump reaches authoritative CMC without starting mantle"), [this](FState& State)
			{
				ARpgCharacter* Character = FindCharacter(State.World, SubjectId);
				return Character && Character->GetCharacterMovement()->IsFalling() && HasCleanMantleState(Character);
			}, Timeout())
			.ThenClient(TEXT("Release ordinary jump input"), 0, [](FState& State) { ReleaseJump(LocalCharacter(State.World)); })
			.UntilServer(TEXT("Fallback jump lands normally"), [this](FState& State) { return Grounded(FindCharacter(State.World, SubjectId)); }, Timeout())
			.ThenServer(TEXT("Set up a standing prepared mantle"), [this](FState& State) { PositionAtEntry(State.World, SubjectId); })
			.UntilClient(TEXT("Owner and server entry geometry agree before activation"), 0, [this](FState& State)
				{ return ReadyAtEntry(State.World, SubjectId); }, Timeout())
			.ThenServer(TEXT("Block landing only on authority to exercise a rejected client proposal"), [this](FState& State)
			{
				ARpgCharacter* Character = FindCharacter(State.World, SubjectId);
				const UPrimitiveComponent* Obstacle = ObstacleInWorld(State.World);
				if (!Character || !Obstacle) return;
				FVector SelectedLanding = FVector::ZeroVector;
				const bool bHasSelectedLanding = QueryLandingPosition(*Character, SelectedLanding);
				ASSERT_THAT(IsTrue(bHasSelectedLanding));
				if (!bHasSelectedLanding) return;
				State.LandingBlocker = SpawnBlocker(State.World, SelectedLanding, FVector(35.0, 35.0, 60.0));
				ASSERT_THAT(IsTrue(State.LandingBlocker.IsValid()));
			})
			.ThenClient(TEXT("Owner predicts its locally clear entry through ordinary jump input"), 0, [this](FState& State)
			{
				ARpgCharacter* Character = LocalCharacter(State.World);
				if (!Character) return;
				ASSERT_THAT(IsTrue(HasTraversalCandidate(Character)));
				Observations.Start(SubjectId);
				PressJump(Character);
				ASSERT_THAT(IsTrue(MantleSpec(Character)->IsActive()));
				ReleaseJump(Character);
			})
			.UntilServer(TEXT("Server receives the correlated request and rejects obstructed landing before root motion"), [this](FState& State)
			{
				const FObservation* Record = Observations.Find(State.World);
				return Record && Record->Activations == 1 && !Record->bRootMotion && !Record->bFlying
					&& HasCleanMantleState(FindCharacter(State.World, SubjectId));
			}, Timeout())
			.UntilClient(TEXT("Rejected prediction releases montage, warp target and collision then reconciles"), 0, [this](FState& State)
			{
				ARpgCharacter* Character = FindCharacter(State.World, SubjectId);
				return Grounded(Character) && HasCleanMantleState(Character)
					&& ObstacleInWorld(State.World)
					&& Character->GetActorLocation().Equals(EntryPosition(*Character, *ObstacleInWorld(State.World)), 20.0);
			}, Timeout())
			.ThenServer(TEXT("Remove the test obstruction and restore the valid standing entry"), [this](FState& State)
			{
				if (State.LandingBlocker.IsValid()) State.LandingBlocker->Destroy();
				State.LandingBlocker.Reset();
				PositionAtEntry(State.World, SubjectId);
			})
			.UntilClient(TEXT("Owner has the valid prepared entry after rejection"), 0, [this](FState& State)
				{ return ReadyAtEntry(State.World, SubjectId); }, Timeout())
			.ThenClient(TEXT("Predict mantle through contextual jump and consume repeated presses"), 0, [this](FState& State)
			{
				ARpgCharacter* Character = LocalCharacter(State.World);
				if (!Character) return;
				Observations.Start(SubjectId);
				PressJump(Character);
				ASSERT_THAT(IsTrue(MantleSpec(Character)->IsActive()));
				for (int32 Repeat = 0; Repeat < 8; ++Repeat)
				{
					PressJump(Character);
					ASSERT_THAT(IsFalse(Character->bPressedJump));
				}
				ReleaseJump(Character);
			})
			.UntilServer(TEXT("Server independently accepts and completes the root-motion mantle"), [this](FState& State)
				{ return CompletedMantle(State.World, SubjectId) && Observations.SawRootMotion(State.World); }, Timeout())
			.UntilClient(TEXT("Predicted owner converges on the authored landing and cleans GAS/CMC state"), 0, [this](FState& State)
				{ return CompletedMantle(State.World, SubjectId) && Observations.SawRootMotion(State.World); }, Timeout())
			.ThenServer(TEXT("Repeated input created exactly one authoritative activation"), [this](FState& State)
				{ ASSERT_THAT(IsTrue(Observations.Find(State.World) && Observations.Find(State.World)->Activations == 1)); })
			.ThenClient(TEXT("Repeated input did not restart the predicted montage"), 0, [this](FState& State)
				{ ASSERT_THAT(IsTrue(Observations.Find(State.World) && Observations.Find(State.World)->Activations == 1)); })
			.ThenClientJoins()
			.UntilClient(TEXT("Late join reconstructs the landed simulated proxy and prepared obstacle"), 1, [this](FState& State)
			{
				ARpgCharacter* Character = FindCharacter(State.World, SubjectId);
				return Isolation.IsIsolated(State.World) && Ready(State.World, Character)
					&& Character->GetLocalRole() == ROLE_SimulatedProxy && CompletedMantle(State.World, SubjectId);
			}, Timeout())
			.ThenServer(TEXT("Prepare a mantle observed by the late-joined client"), [this](FState& State) { PositionAtEntry(State.World, SubjectId); })
			.UntilClient(TEXT("Owner is ready for the cancellation case"), 0, [this](FState& State) { return ReadyAtEntry(State.World, SubjectId); }, Timeout())
			.UntilClient(TEXT("Observer has received the reset entry position"), 1, [this](FState& State) { return ReadyAtEntry(State.World, SubjectId); }, Timeout())
			.ThenClient(TEXT("Start another predicted mantle through jump"), 0, [this](FState& State)
				{ Observations.Start(SubjectId); PressJump(LocalCharacter(State.World)); ReleaseJump(LocalCharacter(State.World)); })
			.UntilServer(TEXT("Authority is moving under the active mantle montage"), [this](FState& State)
			{
				ARpgCharacter* Character = FindCharacter(State.World, SubjectId);
				return Character && MantleSpec(Character) && MantleSpec(Character)->IsActive()
					&& Observations.SawRootMotion(State.World);
			}, Timeout())
			.ThenServer(TEXT("Cancel the active ability through authoritative GAS"), [this](FState& State)
			{
				ARpgCharacter* Character = FindCharacter(State.World, SubjectId);
				if (!Character || !MantleSpec(Character)) return;
				Character->GetRpgAbilitySystemComponent()->CancelAbilityHandle(MantleSpec(Character)->Handle);
			})
			.UntilServer(TEXT("Cancellation restores authority collision, warp ownership and CMC"), [this](FState& State)
				{ return HasCleanMantleState(FindCharacter(State.World, SubjectId)) && Grounded(FindCharacter(State.World, SubjectId)); }, Timeout())
			.UntilClient(TEXT("Cancellation reconciles the predicted owner without residual root motion"), 0, [this](FState& State)
				{ return HasCleanMantleState(FindCharacter(State.World, SubjectId)) && Grounded(FindCharacter(State.World, SubjectId)); }, Timeout())
			.ThenClient(TEXT("Turn the ordinary view after cancellation released traversal rotation"), 0, [](FState& State)
			{
				ARpgCharacter* Character = LocalCharacter(State.World);
				if (AController* Controller = Character ? Character->GetController() : nullptr)
				{
					Controller->SetControlRotation(FRotator(0.0, 35.0, 0.0));
				}
			})
			.UntilClient(TEXT("Normal pawn facing follows the view again after cancellation"), 0, [this](FState& State)
			{
				ARpgCharacter* Character = FindCharacter(State.World, SubjectId);
				return Character && HasCleanMantleState(Character)
					&& FMath::Abs(FMath::FindDeltaAngleDegrees(Character->GetActorRotation().Yaw, 35.0)) < 1.0;
			}, Timeout())
			.ThenServer(TEXT("Prepare a mantle interrupted by final death"), [this](FState& State) { PositionAtEntry(State.World, SubjectId); })
			.UntilClient(TEXT("Owner can restart after cancellation"), 0, [this](FState& State) { return ReadyAtEntry(State.World, SubjectId); }, Timeout())
			.ThenClients(TEXT("Remember persistent ASC and avatar before death"), [this](FState& State)
			{
				ARpgCharacter* Character = FindCharacter(State.World, SubjectId);
				if (!Character) return;
				State.PawnBeforeDeath = Character;
				State.PersistentASC = Character->GetRpgAbilitySystemComponent();
			})
			.ThenClient(TEXT("Activate the mantle that will be interrupted by death"), 0, [this](FState& State)
				{ Observations.Start(SubjectId); PressJump(LocalCharacter(State.World)); ReleaseJump(LocalCharacter(State.World)); })
			.UntilServer(TEXT("Death case has an active authoritative mantle"), [this](FState& State)
			{
				ARpgCharacter* Character = FindCharacter(State.World, SubjectId);
				return Character && MantleSpec(Character) && MantleSpec(Character)->IsActive()
					&& Character->GetCharacterMovement()->MovementMode == MOVE_Flying;
			}, Timeout())
			.ThenServer(TEXT("Enter final death through existing health and downed lifecycle"), [this](FState& State)
			{
				ARpgCharacter* Character = FindCharacter(State.World, SubjectId);
				if (!Character) return;
				State.PawnBeforeDeath = Character;
				State.PersistentASC = Character->GetRpgAbilitySystemComponent();
				URpgHealthComponent::FindHealthComponent(Character)->DamageSelfDestruct(false);
				if (URpgDownedComponent* Downed = URpgDownedComponent::FindDownedComponent(Character); Downed && Downed->IsDowned())
				{
					Downed->ForceDeathFromDowned();
				}
				ASSERT_THAT(IsTrue(HasCleanMantleState(Character)));
				ASSERT_THAT(IsTrue(Character->GetCharacterMovement()->MovementMode == MOVE_None));
			})
			.UntilClient(TEXT("Owner receives final death and the elapsed respawn delay"), 0, [this](FState& State)
			{
				const ARpgPlayerState* PlayerState = FindPlayerState(State.World, SubjectId);
				return PlayerState && PlayerState->IsWaitingForRespawn() && PlayerState->CanRespawnNow();
			}, Timeout())
			.ThenClient(TEXT("Request respawn once through the real controller RPC"), 0, [this](FState& State)
			{
				if (!IsActiveTestWorld(State.World)) return;
				ARpgPlayerController* Controller = Cast<ARpgPlayerController>(State.World->GetFirstPlayerController());
				ASSERT_THAT(IsNotNull(Controller));
				if (Controller) Controller->RequestRespawn();
			})
			.UntilClient(TEXT("Respawn rebinds persistent GAS to a new healthy mantle pawn"), 0, [this](FState& State)
				{ return RespawnedCharacter(State, SubjectId) && Grounded(RespawnedCharacter(State, SubjectId)) && MantleSpec(RespawnedCharacter(State, SubjectId)); }, Timeout())
			.UntilServer(TEXT("Authority also owns the clean new avatar"), [this](FState& State)
				{ return RespawnReady(State); }, Timeout())
			.UntilClient(TEXT("Late-joined observer receives the new healthy avatar"), 1, [this](FState& State)
				{ return RespawnedCharacter(State, SubjectId) != nullptr; }, Timeout())
			.ThenServer(TEXT("Prepare a fresh lane for the respawned character while preserving the corpse"), [this](FState& State)
			{
				ARpgCharacter* Character = FindCharacter(State.World, SubjectId);
				UPrimitiveComponent* OldObstacle = ObstacleInWorld(State.World);
				if (!Character || !OldObstacle) return;
				ReportEntryBlockers(State.World, *Character, *OldObstacle);
				FTransform NewLane = OldObstacle->GetOwner()->GetActorTransform();
				NewLane.AddToTranslation(FVector(0.0, 2000.0, 0.0));
				ASSERT_THAT(IsTrue(OldObstacle->GetOwner()->Destroy()));
				ASSERT_THAT(IsNotNull(SpawnPreparedObstacle(State.World, NewLane)));
				PositionAtEntry(State.World, SubjectId);
			})
			.UntilClient(TEXT("Respawned owner has a valid entry and a new mantle grant"), 0, [this](FState& State) { return ReadyAtEntry(State.World, SubjectId); }, Timeout())
			.UntilClient(TEXT("Observer receives the final entry position"), 1, [this](FState& State) { return ReadyAtEntry(State.World, SubjectId); }, Timeout())
			.ThenClient(TEXT("Mantle after respawn with 75 ms outgoing packet latency on each PIE driver"), 0, [this](FState& State)
			{
				if (!IsActiveTestWorld(State.World)) return;
				ASSERT_THAT(IsTrue(Latency.Start(75)));
				Observations.Start(SubjectId);
				PressJump(LocalCharacter(State.World));
				ReleaseJump(LocalCharacter(State.World));
			})
			.UntilServer(TEXT("Respawned authority completes another real root-motion mantle"), [this](FState& State)
				{ return CompletedMantle(State.World, SubjectId) && Observations.SawRootMotion(State.World); }, Timeout())
			.UntilClient(TEXT("Respawned autonomous owner completes and reconciles"), 0, [this](FState& State)
				{ return CompletedMantle(State.World, SubjectId) && Observations.SawRootMotion(State.World); }, Timeout())
			.UntilClient(TEXT("Simulated proxy receives advancing montage, root motion and final landing"), 1, [this](FState& State)
				{ return CompletedMantle(State.World, SubjectId) && Observations.SawRootMotion(State.World); }, Timeout())
			.ThenClients(TEXT("Prediction and proxy movement retained normal server correction throughout mantle"), [this](FState& State)
				{ ASSERT_THAT(IsTrue(Observations.Find(State.World) && !Observations.Find(State.World)->bDisabledCorrection)); })
			.ThenServer(TEXT("User saves remain isolated through completion"), [this](FState& State)
			{
				ASSERT_THAT(IsTrue(Isolation.IsIsolated(State.World)));
				ASSERT_THAT(IsTrue(Observations.Find(State.World) && !Observations.Find(State.World)->bDisabledCorrection));
				Latency.Stop();
			})
			.ThenServer(TEXT("Prepare an obstacle-destruction interruption after restoring packet settings"), [this](FState& State)
				{ PositionAtEntry(State.World, SubjectId); })
			.UntilClient(TEXT("Owner is ready at the prepared entry before obstacle destruction"), 0, [this](FState& State)
				{ return ReadyAtEntry(State.World, SubjectId); }, Timeout())
			.ThenClient(TEXT("Start a real predicted mantle whose obstacle will be removed"), 0, [this](FState& State)
			{
				if (!IsActiveTestWorld(State.World)) return;
				Observations.Start(SubjectId);
				PressJump(LocalCharacter(State.World));
				ReleaseJump(LocalCharacter(State.World));
			})
			.UntilServer(TEXT("Authority acquired the active mantle and collision lease before destruction"), [this](FState& State)
			{
				ARpgCharacter* Character = FindCharacter(State.World, SubjectId);
				const URpgCharacterMovementComponent* Movement = Character ? Cast<URpgCharacterMovementComponent>(Character->GetCharacterMovement()) : nullptr;
				return Movement && Movement->MovementMode == MOVE_Flying && Movement->GetMantleCollisionComponent()
					&& MantleSpec(Character) && MantleSpec(Character)->IsActive();
			}, Timeout())
			.ThenServer(TEXT("Destroy the prepared obstacle while the server mantle owns its collision lease"), [this](FState& State)
			{
				UPrimitiveComponent* Obstacle = ObstacleInWorld(State.World);
				ASSERT_THAT(IsNotNull(Obstacle));
				if (!Obstacle) return;
				AActor* ObstacleActor = Obstacle->GetOwner();
				State.DestroyedObstacleTransform = ObstacleActor->GetActorTransform();
				ASSERT_THAT(IsTrue(ObstacleActor->Destroy()));
			})
			.UntilServer(TEXT("Destroyed obstacle releases authority ability, warp and stale collision lease"), [this](FState& State)
			{
				ARpgCharacter* Character = FindCharacter(State.World, SubjectId);
				return !ObstacleInWorld(State.World) && Grounded(Character) && HasCleanMantleState(Character);
			}, Timeout())
			.UntilClient(TEXT("Owner reconciles obstacle destruction and returns to grounded movement"), 0, [this](FState& State)
			{
				ARpgCharacter* Character = FindCharacter(State.World, SubjectId);
				return !ObstacleInWorld(State.World) && Grounded(Character) && HasCleanMantleState(Character);
			}, Timeout())
			.UntilClient(TEXT("Simulated observer also releases the destroyed obstacle lease"), 1, [this](FState& State)
			{
				ARpgCharacter* Character = FindCharacter(State.World, SubjectId);
				return !ObstacleInWorld(State.World) && Grounded(Character) && HasCleanMantleState(Character);
			}, Timeout())
			.ThenServer(TEXT("Spawn replacement authored geometry at the same prepared entry"), [this](FState& State)
			{
				if (!IsActiveTestWorld(State.World)) return;
				ASSERT_THAT(IsNotNull(SpawnPreparedObstacle(State.World, State.DestroyedObstacleTransform)));
				PositionAtEntry(State.World, SubjectId);
			})
			.UntilClient(TEXT("Owner can validate the replacement obstacle after releasing the destroyed lease"), 0, [this](FState& State)
				{ return ReadyAtEntry(State.World, SubjectId); }, Timeout())
			.ThenServer(TEXT("Save isolation remains intact after obstacle replacement"), [this](FState& State)
				{ ASSERT_THAT(IsTrue(Isolation.IsIsolated(State.World))); });
	}

	bool RespawnReady(FState& State) const
	{
		ARpgCharacter* Character = RpgMantleIntegrationTests::RespawnedCharacter(State, SubjectId);
		return Character && Character->HasAuthority() && RpgMantleIntegrationTests::Grounded(Character);
	}
};

/** Exercises the saved map and physical key mapping, independently of the idealized network fixture above. */
NETWORK_TEST_CLASS(GaspMantleAuthoredMapPIE, "SurvivalRpg.GASP.Mantle")
{
	using FIsolation = RpgMantleIntegrationTests::FScopedSaveIsolation;
	FIsolation Isolation;
	TStrongObjectPtr<ULevelEditorPlaySettings> PlaySettings;
	FPrimaryAssetId OriginalExperience;
	TWeakObjectPtr<UWorld> ServerWorld;
	TWeakObjectPtr<UWorld> ClientWorld;
	TWeakObjectPtr<UWorld> ObserverWorld;
	TWeakObjectPtr<APlayerController> LookInputController;
	TWeakObjectPtr<UPrimitiveComponent> Obstacle;
	FDelegateHandle TickHandle;
	struct FHandoffObservation
	{
		TWeakObjectPtr<URpgAbilitySystemComponent> ASC;
		FDelegateHandle EndedHandle;
		FVector Location = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		FVector Acceleration = FVector::ZeroVector;
		double Time = -1.0;
		bool bCancelled = false;
		bool bContinuedMoving = false;
	};
	FHandoffObservation OwnerHandoff;
	FHandoffObservation ServerHandoff;
	struct FFacingObservation
	{
		TWeakObjectPtr<UAnimMontage> Montage;
		float LateWarpTime = -1.0f;
		float FinalWarpEndTime = -1.0f;
		float WarpYaw = 0.0f;
		float BestLateWarpError = 180.0f;
		float LateActorYaw = 0.0f;
		float LateControlYaw = 0.0f;
		float MaximumPostWarpError = 0.0f;
		int32 PostWarpSamples = 0;
		bool bHasWarpTarget = false;
		bool bSawRootMotionLease = false;
		bool bLanded = false;
		bool bRestoredFacing = false;
	};
	FFacingObservation OwnerFacing;
	FFacingObservation ServerFacing;
	FFacingObservation ObserverFacing;
	FVector SpawnLocation = FVector::ZeroVector;
	FBox ObstacleBounds{ForceInit};
	double AttemptStart = 0.0;
	double ContactStart = -1.0;
	float LateralOffset = 0.0f;
	float AngledApproachYaw = 0.0f;
	float SpacePressActorYaw = 0.0f;
	float MaximumControlYawError = 0.0f;
	float MaximumApproachSpeed = 0.0f;
	float SpacePressDistance = 0.0f;
	float SpacePressSpeed = 0.0f;
	float SpacePressLateralOffset = 0.0f;
	float FirstOwnerMontageTime = -1.0f;
	float LastOwnerMontageTime = -1.0f;
	float FirstServerMontageTime = -1.0f;
	float LastServerMontageTime = -1.0f;
	int32 SubjectId = INDEX_NONE;
	bool bOwnsSession = false;
	bool bConfigured = false;
	bool bDriving = false;
	bool bPressedSpace = false;
	bool bSawFallbackJump = false;
	bool bSawOwnerRootMotion = false;
	bool bSawServerRootMotion = false;
	bool bSawOwnerLease = false;
	bool bSawServerLease = false;
	bool bStopAtContact = false;
	bool bHoldBeforeReach = false;
	bool bHoldMovementThroughHandoff = false;
	bool bReleasedJump = false;
	bool bReleasedMovement = false;
	bool bReportedWait = false;
	bool bUseHost = false;
	bool bSetAngledHeading = false;

	BEFORE_EACH()
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && IsValid(Context.World()))
			{
				TestRunner->AddError(TEXT("Authored-map mantle test refuses to interrupt an existing PIE session."));
				return;
			}
		}
		// Register before RequestPlaySession: InitGame may load persistence before a pawn or a test tick exists.
		Isolation.Start();
		OriginalExperience = GetDefault<URpgDeveloperSettings>()->ExperienceOverride;
		GetMutableDefault<URpgDeveloperSettings>()->ExperienceOverride = FPrimaryAssetId();
		bConfigured = true;
		TestCommandBuilder.OnTearDown(TEXT("Release injected keys and close only the authored-map test session"), [this]() { Cleanup(); });
	}
	AFTER_EACH() { Cleanup(); }

	TEST_METHOD(RunningApproachUsesAuthoredMapAndEnhancedInput) { QueueApproach(0.0f, false, false); }
	TEST_METHOD(OffsetApproachTraversesAwayFromTheOldCenterStripe) { QueueApproach(110.0f, false, false); }
	TEST_METHOD(CollisionContactStillAllowsContextualTraversal) { QueueApproach(0.0f, true, false); }
	TEST_METHOD(HeldJumpRetriesAfterTheInitialOrdinaryJump) { QueueApproach(0.0f, false, true); }
	TEST_METHOD(ListenServerHostRunsAndTraverses) { QueueApproach(0.0f, false, false, true); }
	TEST_METHOD(HeldMovementContinuesAcrossMantleHandoff) { QueueApproach(0.0f, false, false, false, true); }
	TEST_METHOD(ListenServerHostMaintainsMovementAcrossMantleHandoff) { QueueApproach(0.0f, false, false, true, true); }
	TEST_METHOD(PositiveAngledApproachAlignsBodyWithoutTurningView) { QueueApproach(-130.0f, false, false, false, true, 35.0f); }
	TEST_METHOD(NegativeAngledApproachAlignsBodyWithoutTurningView) { QueueApproach(130.0f, false, false, false, true, -35.0f); }
	TEST_METHOD(ListenServerAngledApproachAlignsBodyWithoutTurningView) { QueueApproach(-130.0f, false, false, true, true, 35.0f); }

	void QueueApproach(float InLateralOffset, bool bInStopAtContact, bool bInHoldBeforeReach, bool bInUseHost = false,
		bool bInHoldMovementThroughHandoff = false, float InAngledApproachYaw = 0.0f)
	{
		if (!bConfigured) return;
		LateralOffset = InLateralOffset;
		bStopAtContact = bInStopAtContact;
		bHoldBeforeReach = bInHoldBeforeReach;
		bUseHost = bInUseHost;
		bHoldMovementThroughHandoff = bInHoldMovementThroughHandoff;
		AngledApproachYaw = InAngledApproachYaw;
		TestCommandBuilder
			.Do(TEXT("Play the saved mantle map with its own GameMode and Experience"), [this]()
			{
				PlaySettings.Reset(NewObject<ULevelEditorPlaySettings>());
				PlaySettings->SetPlayNetMode(EPlayNetMode::PIE_ListenServer);
				PlaySettings->SetPlayNumberOfClients(AngledApproachYaw != 0.0f ? (bUseHost ? 2 : 3) : (bUseHost ? 1 : 2));
				PlaySettings->SetRunUnderOneProcess(true);
				PlaySettings->bLaunchSeparateServer = false;
				PlaySettings->GameGetsMouseControl = false;
				FRequestPlaySessionParams Params;
				Params.EditorPlaySettings = PlaySettings.Get();
				Params.GlobalMapOverride = TEXT("/Game/SurvivalRpg/Maps/Test/Lvl_RpgGaspMantle");
				Params.bAllowOnlineSubsystem = false;
				// No pawn placement, GameMode or Experience override: composition and PlayerStarts come from the saved map.
				bOwnsSession = true;
				GUnrealEd->RequestPlaySession(Params);
				GUnrealEd->StartQueuedPlaySessionRequest();
			})
			.Until(TEXT("The authored map grants traversal and initializes the locally controlled player's actual input"), [this]()
			{
				FindWorlds();
				ARpgCharacter* Character = Owner();
				const URpgPawnGameplayComponent* Input = URpgPawnGameplayComponent::FindPawnGameplayComponent(Character);
				if (!RpgMantleIntegrationTests::Ready(InputWorld(), Character)
					|| !RpgMantleIntegrationTests::Grounded(Character) || !Input || !Input->IsReadyToBindInputs()
					|| !RpgMantleIntegrationTests::MantleSpec(Character)) return false;
				SubjectId = Character->GetPlayerState()->GetPlayerId();
				return RpgMantleIntegrationTests::Ready(ServerWorld.Get(), Authority())
					&& RpgMantleIntegrationTests::MantleSpec(Authority()) && Isolation.IsIsolated(ServerWorld.Get())
					&& (AngledApproachYaw == 0.0f || (RpgMantleIntegrationTests::Ready(ObserverWorld.Get(), Observer())
						&& Observer()->GetLocalRole() == ROLE_SimulatedProxy));
			}, FTimespan::FromSeconds(60.0))
			.Then(TEXT("Choose the authored obstacle in the player's lane and begin real W input"), [this]()
			{
				ARpgCharacter* Character = Owner();
				if (!Character) return;
				if (APlayerController* Controller = Cast<APlayerController>(Character->GetController()))
				{
					// Exclude physical mouse/stick look from this scripted approach. Direct camera/ability
					// SetControlRotation calls remain observable by the unchanged view-preservation checks.
					Controller->SetIgnoreLookInput(true);
					LookInputController = Controller;
				}
				SpawnLocation = Character->GetActorLocation();
				FindLaneObstacle(*Character);
				ASSERT_THAT(IsTrue(Obstacle.IsValid()));
				if (!Obstacle.IsValid()) return;
				ASSERT_THAT(IsTrue(ObstacleBounds.Min.X - SpawnLocation.X > 600.0));
				ASSERT_THAT(IsTrue(SpawnLocation.Y + LateralOffset > ObstacleBounds.Min.Y + 50.0
					&& SpawnLocation.Y + LateralOffset < ObstacleBounds.Max.Y - 50.0));
				AttemptStart = InputWorld()->GetTimeSeconds();
				if (bHoldMovementThroughHandoff)
				{
					SubscribeHandoff(Character, OwnerHandoff);
					SubscribeHandoff(Authority(), ServerHandoff);
				}
				bDriving = true;
				TickHandle = FWorldDelegates::OnWorldTickEnd.AddRaw(this, &GaspMantleAuthoredMapPIE::OnTick);
				Key(EKeys::W, true);
			})
			.Until(TEXT("Mapped Space starts advancing local and authoritative traversal root motion"), [this]()
			{
				return bSawOwnerRootMotion && bSawServerRootMotion && bSawOwnerLease && bSawServerLease
					&& LastOwnerMontageTime > FirstOwnerMontageTime + 0.1f
					&& LastServerMontageTime > FirstServerMontageTime + 0.1f;
			}, FTimespan::FromSeconds(18.0))
			.Until(TEXT("The local owner and authority finish above the actual cube and release traversal state"), [this]()
			{
				return FinishedOnObstacle(Owner()) && FinishedOnObstacle(Authority());
			}, FTimespan::FromSeconds(10.0))
			.Until(TEXT("Held movement continues on the owner and authority after traversal releases control"), [this]()
			{
				return !bHoldMovementThroughHandoff
					|| (OwnerHandoff.bContinuedMoving && ServerHandoff.bContinuedMoving && bReleasedMovement);
			}, FTimespan::FromSeconds(3.0))
			.Until(TEXT("Angled traversal lands on every peer and ordinary facing resumes without changing the view"), [this]()
			{
				return AngledApproachYaw == 0.0f || (OwnerFacing.bLanded && ServerFacing.bLanded && ObserverFacing.bLanded
					&& OwnerFacing.bRestoredFacing && ServerFacing.bRestoredFacing && ObserverFacing.bRestoredFacing);
			}, FTimespan::FromSeconds(5.0))
			.Then(TEXT("The approach covered player input, speed, geometry and cleanup rather than forced candidate placement"), [this]()
			{
				ASSERT_THAT(IsTrue(MaximumApproachSpeed > 100.0f));
				ASSERT_THAT(IsTrue(bPressedSpace));
				if (!bStopAtContact) ASSERT_THAT(IsTrue(SpacePressSpeed > 100.0f));
				if (bStopAtContact) ASSERT_THAT(IsTrue(ContactStart >= 0.0));
				if (bHoldBeforeReach) ASSERT_THAT(IsTrue(bSawFallbackJump));
				if (LateralOffset != 0.0f && AngledApproachYaw == 0.0f) ASSERT_THAT(IsTrue(FMath::Abs(SpacePressLateralOffset) > 80.0f));
				if (AngledApproachYaw != 0.0f)
				{
					ASSERT_THAT(IsTrue(FMath::Abs(SpacePressActorYaw) >= 25.0f));
					ASSERT_THAT(IsTrue(MaximumControlYawError < 1.0f));
					ASSERT_THAT(IsTrue(OwnerFacing.PostWarpSamples > 0 && ServerFacing.PostWarpSamples > 0));
					for (const FFacingObservation* Facing : {&OwnerFacing, &ServerFacing, &ObserverFacing})
					{
						ASSERT_THAT(IsTrue(Facing->bSawRootMotionLease));
						ASSERT_THAT(IsTrue(Facing->bHasWarpTarget && Facing->BestLateWarpError < 8.0f));
						if (Facing->PostWarpSamples > 0) ASSERT_THAT(IsTrue(Facing->MaximumPostWarpError < 8.0f));
						UE_LOG(LogTemp, Display, TEXT("RpgMantleAngledResult host=%d requestedYaw=%.2f pressActorYaw=%.2f controlError=%.2f warpYaw=%.2f lateActorYaw=%.2f lateControlYaw=%.2f alignmentError=%.2f postWarpSamples=%d postWarpError=%.2f landed=%d facingRestored=%d"),
							bUseHost, AngledApproachYaw, SpacePressActorYaw, MaximumControlYawError, Facing->WarpYaw,
							Facing->LateActorYaw, Facing->LateControlYaw, Facing->BestLateWarpError, Facing->PostWarpSamples,
							Facing->MaximumPostWarpError, Facing->bLanded, Facing->bRestoredFacing);
					}
				}
				if (bHoldMovementThroughHandoff)
				{
					// Sample the end delegate, before the next movement tick can hide a cleanup-induced full stop.
					ASSERT_THAT(IsTrue(OwnerHandoff.Time >= 0.0 && ServerHandoff.Time >= 0.0));
					ASSERT_THAT(IsFalse(OwnerHandoff.bCancelled || ServerHandoff.bCancelled));
					ASSERT_THAT(IsTrue(OwnerHandoff.Velocity.X > 1.0 && ServerHandoff.Velocity.X > 1.0));
					ASSERT_THAT(IsTrue(OwnerHandoff.Acceleration.X > 1.0 && ServerHandoff.Acceleration.X > 1.0));
				}
				ASSERT_THAT(IsTrue(Isolation.IsIsolated(ServerWorld.Get())));
				Report(TEXT("completed"));
			});
	}

	void FindWorlds()
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (Context.WorldType != EWorldType::PIE || !IsValid(World)
				|| !World->GetMapName().Contains(TEXT("Lvl_RpgGaspMantle"))) continue;
			if (World->GetNetMode() == NM_ListenServer) ServerWorld = World;
			if (World->GetNetMode() == NM_Client)
			{
				if (!ClientWorld.IsValid()) ClientWorld = World;
				else if (World != ClientWorld.Get() && !ObserverWorld.IsValid()) ObserverWorld = World;
			}
		}
		if (bUseHost && AngledApproachYaw != 0.0f) ObserverWorld = ClientWorld;
	}
	UWorld* InputWorld() const { return bUseHost ? ServerWorld.Get() : ClientWorld.Get(); }
	ARpgCharacter* Owner() const { return RpgMantleIntegrationTests::LocalCharacter(InputWorld()); }
	ARpgCharacter* Authority() const { return RpgMantleIntegrationTests::FindCharacter(ServerWorld.Get(), SubjectId); }
	ARpgCharacter* Observer() const { return RpgMantleIntegrationTests::FindCharacter(ObserverWorld.Get(), SubjectId); }
	void Key(FKey InKey, bool bPressed)
	{
		APlayerController* Controller = Owner() ? Cast<APlayerController>(Owner()->GetController()) : nullptr;
		if (Controller) Controller->InputKey(FInputKeyEventArgs::CreateSimulated(InKey, bPressed ? IE_Pressed : IE_Released, bPressed ? 1.0f : 0.0f));
	}
	void FindLaneObstacle(const ARpgCharacter& Character)
	{
		const FVector Position = Character.GetActorLocation();
		const double FeetZ = Position.Z - Character.GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		double BestDistance = TNumericLimits<double>::Max();
		for (TActorIterator<AActor> It(InputWorld()); It; ++It)
		{
			TInlineComponentArray<UPrimitiveComponent*> Components(*It);
			for (UPrimitiveComponent* Component : Components)
			{
				if (!Component || Component->IsSimulatingPhysics() || !Component->IsQueryCollisionEnabled()
					|| Component->GetCollisionResponseToChannel(ECC_GameTraceChannel1) != ECR_Block) continue;
				const FBox Bounds = Component->Bounds.GetBox();
				const double Distance = Bounds.Min.X - Position.X;
				if (Distance <= 0.0 || Distance >= BestDistance || Bounds.Max.Z - FeetZ < 80.0 || Bounds.Max.Z - FeetZ > 155.0
					|| Position.Y < Bounds.Min.Y + 50.0 || Position.Y > Bounds.Max.Y - 50.0 || Bounds.GetSize().X < 150.0) continue;
				Obstacle = Component;
				ObstacleBounds = Bounds;
				BestDistance = Distance;
			}
		}
	}
	void OnTick(UWorld* World, ELevelTick TickType, float DeltaSeconds)
	{
		if (!bDriving || !RpgMantleIntegrationTests::IsActiveTestWorld(World)) return;
		if (World == ServerWorld.Get())
		{
			Observe(Authority(), false);
			ObserveContinuation(Authority(), ServerHandoff);
			ObserveFacing(Authority(), ServerFacing);
		}
		if (World == ObserverWorld.Get()) ObserveFacing(Observer(), ObserverFacing);
		if (World != InputWorld()) return;
		ARpgCharacter* Character = Owner();
		if (!Character) return;
		Observe(Character, true);
		ObserveContinuation(Character, OwnerHandoff);
		ObserveFacing(Character, OwnerFacing);
		if (bSetAngledHeading && Character->GetController())
		{
			MaximumControlYawError = FMath::Max(MaximumControlYawError,
				static_cast<float>(FMath::Abs(FMath::FindDeltaAngleDegrees(Character->GetController()->GetControlRotation().Yaw, static_cast<double>(AngledApproachYaw)))));
		}
		if (!bReportedWait && World->GetTimeSeconds() - AttemptStart >= 5.0)
		{
			Report(TEXT("five_second_checkpoint"));
			bReportedWait = true;
		}
		const FGameplayAbilitySpec* Spec = RpgMantleIntegrationTests::MantleSpec(Character);
		const bool bActive = Spec && Spec->IsActive();
		if (bPressedSpace && Character->GetCharacterMovement()->IsFalling() && !bActive) bSawFallbackJump = true;
		if (bActive && !bReleasedJump)
		{
			Key(EKeys::SpaceBar, false);
			bReleasedJump = true;
			if (!bHoldMovementThroughHandoff)
			{
				Key(EKeys::W, false);
				bReleasedMovement = true;
			}
		}
		if (bReleasedJump)
		{
			if (!bReleasedMovement && OwnerHandoff.bContinuedMoving && ServerHandoff.bContinuedMoving)
			{
				Key(EKeys::W, false);
				bReleasedMovement = true;
			}
			return;
		}
		const FVector Position = Character->GetActorLocation();
		const float Distance = static_cast<float>(ObstacleBounds.Min.X - Position.X);
		const float Speed = static_cast<float>(Character->GetVelocity().Size2D());
		MaximumApproachSpeed = FMath::Max(MaximumApproachSpeed, Speed);
		APlayerController* Controller = Cast<APlayerController>(Character->GetController());
		if (Controller)
		{
			// Aim while walking; there is no pawn teleport, forced velocity or direct movement-component call.
			if (AngledApproachYaw != 0.0f && Distance <= 220.0f)
			{
				if (!bSetAngledHeading)
				{
					Controller->SetControlRotation(FRotator(0.0, AngledApproachYaw, 0.0));
					bSetAngledHeading = true;
				}
			}
			else if (!bSetAngledHeading)
			{
				const FVector Direction(250.0, SpawnLocation.Y + LateralOffset - Position.Y, 0.0);
				Controller->SetControlRotation(Direction.Rotation());
			}
		}
		if (bPressedSpace) return;
		if (bStopAtContact)
		{
			const float Radius = Character->GetCapsuleComponent()->GetScaledCapsuleRadius();
			if (Distance <= Radius + 4.0f && Speed < 5.0f)
			{
				if (ContactStart < 0.0) ContactStart = World->GetTimeSeconds();
				if (World->GetTimeSeconds() - ContactStart < 0.15) return;
			}
			else return;
		}
		// Jump well before the one-meter cube, then arrive with Space still held. A jump started close to this low
		// obstacle can legitimately clear it, which would not establish an opportunity for the held traversal retry.
		else if (Distance > (bHoldBeforeReach ? 900.0f : 170.0f)) return;
		SpacePressDistance = Distance;
		SpacePressSpeed = Speed;
		SpacePressLateralOffset = static_cast<float>(Position.Y - ObstacleBounds.GetCenter().Y);
		SpacePressActorYaw = static_cast<float>(FRotator::NormalizeAxis(Character->GetActorRotation().Yaw));
		bPressedSpace = true;
		Report(TEXT("space_pressed"));
		Key(EKeys::SpaceBar, true);
	}
	void SubscribeHandoff(ARpgCharacter* Character, FHandoffObservation& Observation)
	{
		URpgAbilitySystemComponent* ASC = Character ? Character->GetRpgAbilitySystemComponent() : nullptr;
		if (!ASC) return;
		Observation.ASC = ASC;
		Observation.EndedHandle = ASC->OnAbilityEnded.AddLambda(
			[WeakCharacter = TWeakObjectPtr<ARpgCharacter>(Character), Record = &Observation](const FAbilityEndedData& Data)
		{
			ARpgCharacter* Pawn = WeakCharacter.Get();
			if (!Pawn || !RpgMantleIntegrationTests::IsActiveTestWorld(Pawn->GetWorld()) || Record->Time >= 0.0
				|| !Data.AbilityThatEnded || !Data.AbilityThatEnded->IsA<URpgGameplayAbility_Mantle>()) return;
			Record->Location = Pawn->GetActorLocation();
			Record->Velocity = Pawn->GetVelocity();
			Record->Acceleration = Pawn->GetCharacterMovement()->GetCurrentAcceleration();
			Record->Time = Pawn->GetWorld()->GetTimeSeconds();
			Record->bCancelled = Data.bWasCancelled;
			UE_LOG(LogTemp, Display, TEXT("RpgMantleInputHandoff authority=%d cancelled=%d position=%s velocity=%s acceleration=%s"),
				Pawn->HasAuthority(), Record->bCancelled, *Record->Location.ToCompactString(),
				*Record->Velocity.ToCompactString(), *Record->Acceleration.ToCompactString());
		});
	}
	void ObserveContinuation(ARpgCharacter* Character, FHandoffObservation& Observation) const
	{
		if (!bHoldMovementThroughHandoff || !Character || Observation.Time < 0.0 || Observation.bContinuedMoving
			|| Character->GetWorld()->GetTimeSeconds() - Observation.Time < 0.15) return;
		// Keep physical W held past handoff; observing later movement alone would miss a one-frame stop at ability end.
		Observation.bContinuedMoving = FinishedOnObstacle(Character)
			&& Character->GetActorLocation().X > Observation.Location.X + 1.0 && Character->GetVelocity().X > 1.0;
	}
	void ObserveFacing(ARpgCharacter* Character, FFacingObservation& Observation)
	{
		if (AngledApproachYaw == 0.0f || !Character) return;
		const URpgCharacterMovementComponent* Movement = Cast<URpgCharacterMovementComponent>(Character->GetCharacterMovement());
		if (!Movement) return;
		const bool bHasLease = Movement->GetMantleCollisionComponent() != nullptr;
		Observation.bLanded |= FinishedOnObstacle(Character);
		Observation.bRestoredFacing |= Observation.bLanded && !bHasLease
			&& FMath::Abs(FMath::FindDeltaAngleDegrees(Character->GetActorRotation().Yaw, static_cast<double>(AngledApproachYaw))) < 5.0;
		UAnimInstance* Animation = Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
		UAnimMontage* Montage = Animation ? Animation->GetCurrentActiveMontage() : nullptr;
		if (!bHasLease || !Animation || !Montage || !Animation->Montage_IsPlaying(Montage)) return;
		Observation.bSawRootMotionLease |= Character->IsPlayingRootMotion() && Movement->MovementMode == MOVE_Flying;
		if (Observation.Montage.Get() != Montage)
		{
			Observation.Montage = Montage;
			TArray<FMotionWarpingWindowData> Windows;
			UMotionWarpingUtilities::GetMotionWarpingWindowsForWarpTargetFromAnimation(Montage, TEXT("FrontLedge"), Windows);
			float LatestEnd = -1.0f;
			for (const FMotionWarpingWindowData& Window : Windows)
			{
				if (Window.EndTime > LatestEnd)
				{
					LatestEnd = Window.EndTime;
					Observation.FinalWarpEndTime = Window.EndTime;
					Observation.LateWarpTime = FMath::Lerp(Window.StartTime, Window.EndTime, 0.8f);
				}
			}
		}
		const UMotionWarpingComponent* Warping = Character->FindComponentByClass<UMotionWarpingComponent>();
		const FMotionWarpingTarget* Target = Warping ? Warping->FindWarpTarget(TEXT("FrontLedge")) : nullptr;
		if (Target)
		{
			Observation.WarpYaw = static_cast<float>(Target->Rotator().Yaw);
			Observation.bHasWarpTarget = true;
		}
		else if (Character->GetLocalRole() == ROLE_SimulatedProxy && ServerFacing.bHasWarpTarget)
		{
			// Simulated proxies replay server root motion, without running a local GAS query or owning warp targets.
			Observation.WarpYaw = ServerFacing.WarpYaw;
			Observation.bHasWarpTarget = true;
		}
		if (!Observation.bHasWarpTarget || Observation.LateWarpTime < 0.0f
			|| Animation->Montage_GetPosition(Montage) < Observation.LateWarpTime) return;
		const float Error = static_cast<float>(FMath::Abs(FMath::FindDeltaAngleDegrees(
			Character->GetActorRotation().Yaw, static_cast<double>(Observation.WarpYaw))));
		if (Observation.FinalWarpEndTime >= 0.0f && Animation->Montage_GetPosition(Montage) > Observation.FinalWarpEndTime)
		{
			++Observation.PostWarpSamples;
			Observation.MaximumPostWarpError = FMath::Max(Observation.MaximumPostWarpError, Error);
			UE_LOG(LogTemp, Display, TEXT("RpgMantleAngledAfterWarp role=%d requestedYaw=%.2f actorYaw=%.2f warpYaw=%.2f montage=%.3f warpEnd=%.3f error=%.2f"),
				static_cast<int32>(Character->GetLocalRole()), AngledApproachYaw, Character->GetActorRotation().Yaw,
				Observation.WarpYaw, Animation->Montage_GetPosition(Montage), Observation.FinalWarpEndTime, Error);
		}
		if (Error < Observation.BestLateWarpError)
		{
			Observation.BestLateWarpError = Error;
			Observation.LateActorYaw = static_cast<float>(Character->GetActorRotation().Yaw);
			Observation.LateControlYaw = Character->GetController()
				? static_cast<float>(Character->GetController()->GetControlRotation().Yaw) : AngledApproachYaw;
			UE_LOG(LogTemp, Display, TEXT("RpgMantleAngledFacing role=%d requestedYaw=%.2f actorYaw=%.2f controlYaw=%.2f warpYaw=%.2f montage=%.3f lateWindow=%.3f error=%.2f"),
				static_cast<int32>(Character->GetLocalRole()), AngledApproachYaw, Observation.LateActorYaw,
				Observation.LateControlYaw, Observation.WarpYaw, Animation->Montage_GetPosition(Montage), Observation.LateWarpTime, Error);
		}
	}
	void Observe(ARpgCharacter* Character, bool bOwner)
	{
		if (!Character) return;
		const FGameplayAbilitySpec* Spec = RpgMantleIntegrationTests::MantleSpec(Character);
		const URpgAbilitySystemComponent* ASC = Character->GetRpgAbilitySystemComponent();
		UAnimInstance* Animation = Character->GetMesh()->GetAnimInstance();
		UAnimMontage* Montage = ASC ? ASC->GetCurrentMontage() : nullptr;
		if (!Spec || !Spec->IsActive() || !Animation || !Montage || !Animation->Montage_IsPlaying(Montage)) return;
		const float Position = Animation->Montage_GetPosition(Montage);
		float& First = bOwner ? FirstOwnerMontageTime : FirstServerMontageTime;
		float& Last = bOwner ? LastOwnerMontageTime : LastServerMontageTime;
		if (First < 0.0f) First = Position;
		Last = Position;
		(bOwner ? bSawOwnerRootMotion : bSawServerRootMotion) |= Character->IsPlayingRootMotion();
		const URpgCharacterMovementComponent* Movement = Cast<URpgCharacterMovementComponent>(Character->GetCharacterMovement());
		(bOwner ? bSawOwnerLease : bSawServerLease) |= Movement && Movement->GetMantleCollisionComponent()
			&& Movement->MovementMode == MOVE_Flying;
		if (Movement && (Movement->bIgnoreClientMovementErrorChecksAndCorrection || Movement->bServerAcceptClientAuthoritativePosition))
		{
			TestRunner->AddError(TEXT("Authored-map traversal disabled authoritative CMC movement correction."));
		}
	}
	bool FinishedOnObstacle(ARpgCharacter* Character) const
	{
		if (!Character || !Character->GetCharacterMovement()->IsMovingOnGround()) return false;
		// The held-W scenario must accept a grounded runner; the other cases still prove a settled landing.
		if (!bHoldMovementThroughHandoff && !RpgMantleIntegrationTests::Grounded(Character)) return false;
		const FGameplayAbilitySpec* Spec = RpgMantleIntegrationTests::MantleSpec(Character);
		const URpgCharacterMovementComponent* Movement = Cast<URpgCharacterMovementComponent>(Character->GetCharacterMovement());
		const FVector Position = Character->GetActorLocation();
		const double FeetZ = Position.Z - Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const bool bAbilityFinished = Spec ? !Spec->IsActive() : Character->GetLocalRole() == ROLE_SimulatedProxy;
		return bAbilityFinished && Movement && !Movement->GetMantleCollisionComponent()
			&& Position.X > ObstacleBounds.Min.X && Position.X < ObstacleBounds.Max.X
			&& Position.Y > ObstacleBounds.Min.Y && Position.Y < ObstacleBounds.Max.Y
			&& FMath::Abs(FeetZ - ObstacleBounds.Max.Z) < 8.0;
	}
	void Report(const TCHAR* Phase) const
	{
		const ARpgCharacter* Character = Owner();
		UE_LOG(LogTemp, Display, TEXT("RpgMantleAuthoredInput phase=%s host=%d offset=%.1f contact=%d heldRetry=%d spawn=%s position=%s maxApproachSpeed=%.1f pressSpeed=%.1f pressDistance=%.1f pressLateral=%.1f fallbackJump=%d ownerRootMotion=%d serverRootMotion=%d ownerMontage=%.3f..%.3f serverMontage=%.3f..%.3f"),
			Phase, bUseHost, LateralOffset, bStopAtContact, bHoldBeforeReach, *SpawnLocation.ToCompactString(),
			Character ? *Character->GetActorLocation().ToCompactString() : TEXT("None"), MaximumApproachSpeed,
			SpacePressSpeed, SpacePressDistance, SpacePressLateralOffset, bSawFallbackJump, bSawOwnerRootMotion, bSawServerRootMotion,
			FirstOwnerMontageTime, LastOwnerMontageTime, FirstServerMontageTime, LastServerMontageTime);
		if (AngledApproachYaw == 0.0f && Character && FCString::Strcmp(Phase, TEXT("space_pressed")) == 0)
		{
			URpgTraversalQueryComponent* Query = Character->FindComponentByClass<URpgTraversalQueryComponent>();
			const URpgGameplayAbility_Mantle* Definition = RpgMantleIntegrationTests::AbilityDefinition();
			FRpgTraversalQueryResult Raw;
			FRpgTraversalQueryResult Validated;
			const bool bRawQuery = Query && Query->QueryTraversal(Raw);
			const bool bAnimationAllowed = Query && Query->IsAnimationAllowed(*Character, Raw);
			FVector Landing = FVector::ZeroVector;
			const bool bLanding = Definition && Definition->GetMantleLandingLocation(*Character, Raw, Landing);
			const bool bValidated = Definition && Definition->FindTraversalCandidate(*Character, Validated);
			const UAnimInstance* Animation = Character->GetMesh()->GetAnimInstance();
			const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
			UE_LOG(LogTemp, Display, TEXT("RpgMantleAuthoredQuery raw=%d validated=%d action=%d hasFront=%d height=%.3f depth=%.3f front=%s normal=%s collider=%s montage=%s start=%.4f rate=%.4f allowed=%d landingValid=%d landing=%s queryClass=%s mode=%d ground=%d falling=%d slotActive=%d rootMotionMode=%d activeMontage=%s searchDistance=%.1f"),
				bRawQuery, bValidated, static_cast<int32>(Raw.ActionType), Raw.HasFrontLedge, Raw.ObstacleHeight, Raw.ObstacleDepth,
				*Raw.FrontLedgeLocation.ToCompactString(), *Raw.FrontLedgeNormal.ToCompactString(), *GetPathNameSafe(Raw.HitComponent.Get()),
				*GetPathNameSafe(Raw.ChosenMontage.Get()), Raw.StartTime, Raw.PlayRate, bAnimationAllowed, bLanding, *Landing.ToCompactString(),
				*GetNameSafe(Query ? Query->GetClass() : nullptr), Movement ? static_cast<int32>(Movement->MovementMode) : -1,
				Movement && Movement->IsMovingOnGround(), Movement && Movement->IsFalling(), Animation && Animation->IsSlotActive(TEXT("DefaultSlot")),
				Animation ? static_cast<int32>(Animation->RootMotionMode) : -1, *GetPathNameSafe(Animation ? Animation->GetCurrentActiveMontage() : nullptr),
				Definition ? Definition->CandidateSearchDistance : -1.0f);
		}
	}
	void Cleanup()
	{
		if (bDriving) Report(TEXT("teardown"));
		FWorldDelegates::OnWorldTickEnd.Remove(TickHandle);
		TickHandle.Reset();
		for (FHandoffObservation* Observation : {&OwnerHandoff, &ServerHandoff})
		{
			if (Observation->ASC.IsValid()) Observation->ASC->OnAbilityEnded.Remove(Observation->EndedHandle);
			Observation->ASC.Reset();
			Observation->EndedHandle.Reset();
		}
		Key(EKeys::W, false);
		Key(EKeys::SpaceBar, false);
		if (LookInputController.IsValid()) LookInputController->SetIgnoreLookInput(false);
		LookInputController.Reset();
		bDriving = false;
		if (bOwnsSession && GUnrealEd)
		{
			GUnrealEd->EndPlayMap();
			bOwnsSession = false;
		}
		if (bConfigured)
		{
			GetMutableDefault<URpgDeveloperSettings>()->ExperienceOverride = OriginalExperience;
			bConfigured = false;
		}
		PlaySettings.Reset();
	}
};

#endif // ENABLE_PIE_NETWORK_TEST
#endif // WITH_DEV_AUTOMATION_TESTS
