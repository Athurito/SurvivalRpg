// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Editor/UnrealEdEngine.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "InputKeyEventArgs.h"
#include "Misc/Guid.h"
#include "MotionWarpingComponent.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Core/Character/RpgCharacter.h"
#include "SurvivalRpg/Core/Character/RpgCharacterMovementComponent.h"
#include "SurvivalRpg/Core/Character/RpgPawnGameplayComponent.h"
#include "SurvivalRpg/Core/Game/Experience/RpgExperienceManagerComponent.h"
#include "SurvivalRpg/Core/Game/RpgGameModeBase.h"
#include "SurvivalRpg/Development/RpgDeveloperSettings.h"
#include "SurvivalRpg/Traversal/RpgGameplayAbility_Mantle.h"
#include "SurvivalRpg/Traversal/RpgTraversalQueryComponent.h"
#include "UnrealEdGlobals.h"
#include "UObject/StrongObjectPtr.h"

#if ENABLE_PIE_NETWORK_TEST

namespace RpgVaultIntegrationTests
{
	constexpr TCHAR MapPath[] = TEXT("/Game/SurvivalRpg/Maps/Test/Lvl_RpgGaspMantle");
	constexpr TCHAR AbilityPath[] = TEXT("/Game/SurvivalRpg/Characters/GASP/CMC/RPG/Traversal/GA_RpgGasp_Mantle.GA_RpgGasp_Mantle_C");
	const FName BarrierTag(TEXT("Rpg.TraversalTest.Vault"));
	const FName ApproachTag(TEXT("Rpg.TraversalTest.Vault.Approach"));

	FBox PhysicalBlockBounds(const AActor& Actor)
	{
		const UStaticMeshComponent* Mesh = Actor.FindComponentByClass<UStaticMeshComponent>();
		return Mesh && Mesh->IsQueryCollisionEnabled() ? Mesh->Bounds.GetBox() : FBox(ForceInit);
	}

	bool ActiveWorld(const UWorld* World)
	{
		if (!World || !GEngine) return false;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* Current = Context.World();
			if (Context.WorldType == EWorldType::PIE && Current == World)
			{
				return IsValid(Current) && !Current->bIsTearingDown && !Current->IsBeingCleanedUp();
			}
		}
		return false;
	}

	ARpgCharacter* LocalPawn(UWorld* World)
	{
		APlayerController* Controller = ActiveWorld(World) ? World->GetFirstPlayerController() : nullptr;
		return Controller ? Cast<ARpgCharacter>(Controller->GetPawn()) : nullptr;
	}

	ARpgCharacter* FindPawn(UWorld* World, int32 PlayerId)
	{
		if (!ActiveWorld(World) || PlayerId == INDEX_NONE) return nullptr;
		for (TActorIterator<ARpgCharacter> It(World); It; ++It)
		{
			if (It->GetPlayerState() && It->GetPlayerState()->GetPlayerId() == PlayerId) return *It;
		}
		return nullptr;
	}

	FGameplayAbilitySpec* TraversalSpec(ARpgCharacter* Character)
	{
		URpgAbilitySystemComponent* ASC = Character ? Character->GetRpgAbilitySystemComponent() : nullptr;
		if (!ASC) return nullptr;
		for (FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			if (Spec.Ability && Spec.Ability->GetClass()->GetPathName() == AbilityPath) return &Spec;
		}
		return nullptr;
	}

	bool IsVaultMontage(const ARpgCharacter& Character, const UAnimMontage* Montage)
	{
		const URpgTraversalQueryComponent* Query = Character.FindComponentByClass<URpgTraversalQueryComponent>();
		return Montage && Query && Query->AllowedVaultAnimations.ContainsByPredicate(
			[Montage](const FRpgTraversalAnimationEntry& Entry) { return Entry.Montage == Montage; });
	}

	bool Ready(UWorld* World, ARpgCharacter* Character)
	{
		const AGameStateBase* GameState = ActiveWorld(World) ? World->GetGameState() : nullptr;
		const URpgExperienceManagerComponent* Experience = GameState ? GameState->FindComponentByClass<URpgExperienceManagerComponent>() : nullptr;
		const URpgTraversalQueryComponent* Query = Character ? Character->FindComponentByClass<URpgTraversalQueryComponent>() : nullptr;
		return Experience && Experience->IsExperienceLoaded() && Character && Character->GetPlayerState()
			&& Character->GetRpgAbilitySystemComponent() && Character->GetMesh() && Character->GetMesh()->GetAnimInstance()
			&& Character->FindComponentByClass<UMotionWarpingComponent>() && Query && !Query->AllowedVaultAnimations.IsEmpty();
	}

	bool Clean(ARpgCharacter* Character)
	{
		if (!Character) return false;
		const FGameplayAbilitySpec* Spec = TraversalSpec(Character);
		const URpgCharacterMovementComponent* Movement = Cast<URpgCharacterMovementComponent>(Character->GetCharacterMovement());
		const UMotionWarpingComponent* Warping = Character->FindComponentByClass<UMotionWarpingComponent>();
		const URpgAbilitySystemComponent* ASC = Character->GetRpgAbilitySystemComponent();
		const UAnimInstance* Animation = Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
		return Movement && !Movement->GetMantleCollisionComponent() && Movement->MovementMode != MOVE_Flying
			&& (!Spec || !Spec->IsActive()) && Warping && !Warping->FindWarpTarget(TEXT("FrontLedge"))
			&& !Warping->FindWarpTarget(TEXT("BackLedge")) && !Warping->FindWarpTarget(TEXT("BackFloor"))
			&& (!ASC || !ASC->GetAnimatingAbility() || !ASC->GetAnimatingAbility()->IsA<URpgGameplayAbility_Mantle>())
			&& (!Animation || !IsVaultMontage(*Character, Animation->GetCurrentActiveMontage())
				|| !Animation->Montage_IsPlaying(Animation->GetCurrentActiveMontage()));
	}

	/** Prevent any fixture world from reading or writing the player's persistence before InitGame. */
	class FSaveIsolation
	{
	public:
		~FSaveIsolation() { FGameModeEvents::OnGameModeInitializedEvent().Remove(Handle); }
		void Start()
		{
			Prefix = TEXT("SurvivalRpg_VaultAutomation_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
			Handle = FGameModeEvents::OnGameModeInitializedEvent().AddLambda([this](AGameModeBase* Initialized)
			{
				ARpgGameModeBase* Mode = Cast<ARpgGameModeBase>(Initialized);
				if (!Mode || !Mode->GetWorld() || Mode->GetWorld()->WorldType != EWorldType::PIE) return;
				Mode->bEnableDiskPersistence = false;
				Mode->WorldSaveSlotName = FString::Printf(TEXT("%s_%u"), *Prefix, Mode->GetUniqueID());
				Mode->WorldSaveBackupSlotName = Mode->WorldSaveSlotName + TEXT("_Backup");
				Mode->WorldSaveRecoverySlotName = Mode->WorldSaveSlotName + TEXT("_Recovery");
				Mode->OfflineProfileKey = Prefix;
			});
		}
		bool IsIsolated(UWorld* World) const
		{
			const ARpgGameModeBase* Mode = ActiveWorld(World) ? World->GetAuthGameMode<ARpgGameModeBase>() : nullptr;
			return Mode && !Mode->bEnableDiskPersistence && Mode->WorldSaveSlotName.StartsWith(Prefix)
				&& Mode->OfflineProfileKey.StartsWith(Prefix);
		}
	private:
		FString Prefix;
		FDelegateHandle Handle;
	};
}

/** Real saved-map traversal: Vault must cross the rear edge and fall to the lower floor, rather than mantle onto a box. */
NETWORK_TEST_CLASS(GaspVaultAuthoredMapPIE, "SurvivalRpg.GASP.Vault")
{
	using FIsolation = RpgVaultIntegrationTests::FSaveIsolation;
	struct FObservation
	{
		TWeakObjectPtr<UWorld> World;
		TWeakObjectPtr<URpgAbilitySystemComponent> ASC;
		FDelegateHandle CommittedHandle;
		FDelegateHandle EndedHandle;
		FVector HandoffPosition = FVector::ZeroVector;
		FVector HandoffVelocity = FVector::ZeroVector;
		float FirstMontageTime = -1.0f;
		float LastMontageTime = -1.0f;
		float WarpYaw = 0.0f;
		float BestAlignmentError = 180.0f;
		float LastWarpEnd = 0.0f;
		float MaximumPostWarpError = 0.0f;
		int32 PostWarpSamples = 0;
		int32 Commits = 0;
		int32 Ends = 0;
		EMovementMode HandoffMode = MOVE_None;
		bool bCancelled = false;
		bool bVaultMontage = false;
		bool bWalkingMontage = false;
		bool bRootMotionLease = false;
		bool bSawFrontWarp = false;
		bool bSawBackWarp = false;
		bool bCrossedRear = false;
		bool bSawReleasedFalling = false;
		bool bLandedBeyond = false;
		bool bRestoredFacing = false;
		bool bMovingAfterLanding = false;
		bool bSawOrdinaryJump = false;
		bool bCorrectionDisabled = false;
	};

	FIsolation Isolation;
	TStrongObjectPtr<ULevelEditorPlaySettings> PlaySettings;
	FPrimaryAssetId PreviousExperience;
	TWeakObjectPtr<UWorld> ServerWorld;
	TWeakObjectPtr<UWorld> ClientWorld;
	TWeakObjectPtr<UWorld> ObserverWorld;
	TWeakObjectPtr<APlayerController> LookInputController;
	TWeakObjectPtr<AActor> Barrier;
	TWeakObjectPtr<AActor> Deck;
	TWeakObjectPtr<AActor> ExitBlocker;
	FBox BarrierBounds{ForceInit};
	FBox DeckBounds{ForceInit};
	FObservation OwnerRecord;
	FObservation AuthorityRecord;
	FObservation ProxyRecord;
	FDelegateHandle TickHandle;
	FVector SpawnLocation = FVector::ZeroVector;
	int32 PlayerId = INDEX_NONE;
	int32 LaneIndex = 0;
	int32 Waypoint = 0;
	float ApproachYaw = 0.0f;
	float PressSpeed = 0.0f;
	float PressYaw = 0.0f;
	float MaximumViewError = 0.0f;
	double InputStarted = 0.0;
	double ContactSince = -1.0;
	double SpacePressedAt = -1.0;
	bool bStanding = false;
	bool bWalking = false;
	bool bWalkInputActive = false;
	bool bHost = false;
	bool bBlockExitOnServer = false;
	bool bCancelOnServer = false;
	bool bCancelledOnServer = false;
	bool bConfigured = false;
	bool bOwnsSession = false;
	bool bDriving = false;
	bool bSpaceReleased = false;
	bool bMovementReleased = false;
	bool bSetFinalHeading = false;
	bool bReportedWait = false;

	BEFORE_EACH()
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && IsValid(Context.World()))
			{
				TestRunner->AddError(TEXT("Vault test refuses to interrupt an existing PIE session."));
				return;
			}
		}
		Isolation.Start();
		PreviousExperience = GetDefault<URpgDeveloperSettings>()->ExperienceOverride;
		GetMutableDefault<URpgDeveloperSettings>()->ExperienceOverride = FPrimaryAssetId();
		bConfigured = true;
		TestCommandBuilder.OnTearDown(TEXT("Release Vault test keys and only its own PIE session"), [this]() { Cleanup(); });
	}
	AFTER_EACH() { Cleanup(); }

	TEST_METHOD(StandingVaultCrossesRearEdgeAndFallsToLowerFloor) { Queue(true, false, 0.0f, 0); }
	TEST_METHOD(WalkingVaultCrossesRearEdgeAndFallsToLowerFloor) { Queue(false, false, 0.0f, 0, false, false, true); }
	TEST_METHOD(RunningVaultCrossesRearEdgeAndKeepsMovingAfterLanding) { Queue(false, false, 0.0f, 0); }
	TEST_METHOD(PositiveAngledVaultAlignsWithoutTurningTheView) { Queue(false, false, 35.0f, 0); }
	TEST_METHOD(NegativeAngledVaultAlignsWithoutTurningTheView) { Queue(false, false, -35.0f, 0); }
	TEST_METHOD(ListenServerVaultCrossesTheThinnerBarrier) { Queue(false, true, 0.0f, 1); }
	TEST_METHOD(ServerBlockedExitRejectsPredictedVault) { Queue(false, false, 0.0f, 0, true, false); }
	TEST_METHOD(CancelledVaultReleasesMovementCollisionAndRotation) { Queue(false, false, 35.0f, 0, false, true); }

	void Queue(bool bInStanding, bool bInHost, float InYaw, int32 InLane, bool bInBlockExit = false, bool bInCancel = false, bool bInWalking = false)
	{
		if (!bConfigured) return;
		bStanding = bInStanding;
		bWalking = bInWalking;
		bHost = bInHost;
		ApproachYaw = InYaw;
		LaneIndex = InLane;
		bBlockExitOnServer = bInBlockExit;
		bCancelOnServer = bInCancel;
		TestCommandBuilder
			.Do(TEXT("Start the saved traversal map with its authored Experience and PlayerStarts"), [this]()
			{
				PlaySettings.Reset(NewObject<ULevelEditorPlaySettings>());
				PlaySettings->SetPlayNetMode(EPlayNetMode::PIE_ListenServer);
				PlaySettings->SetPlayNumberOfClients(bHost ? 2 : 3);
				PlaySettings->SetRunUnderOneProcess(true);
				PlaySettings->bLaunchSeparateServer = false;
				PlaySettings->GameGetsMouseControl = false;
				FRequestPlaySessionParams Params;
				Params.EditorPlaySettings = PlaySettings.Get();
				Params.GlobalMapOverride = RpgVaultIntegrationTests::MapPath;
				Params.bAllowOnlineSubsystem = false;
				bOwnsSession = true;
				GUnrealEd->RequestPlaySession(Params);
				GUnrealEd->StartQueuedPlaySessionRequest();
			})
			.Until(TEXT("All roles receive the real Vault composition and owner input binding"), [this]()
			{
				FindWorlds();
				ARpgCharacter* Character = Owner();
				const URpgPawnGameplayComponent* Input = URpgPawnGameplayComponent::FindPawnGameplayComponent(Character);
				if (!RpgVaultIntegrationTests::Ready(InputWorld(), Character) || !Input || !Input->IsReadyToBindInputs()
					|| !RpgVaultIntegrationTests::TraversalSpec(Character) || !Character->GetCharacterMovement()->IsMovingOnGround()) return false;
				PlayerId = Character->GetPlayerState()->GetPlayerId();
				return RpgVaultIntegrationTests::Ready(ServerWorld.Get(), Authority())
					&& RpgVaultIntegrationTests::Ready(ObserverWorld.Get(), Observer())
					&& Observer()->GetLocalRole() == ROLE_SimulatedProxy && Isolation.IsIsolated(ServerWorld.Get());
			}, FTimespan::FromSeconds(60.0))
			.Then(TEXT("Find the authored elevated lane and walk from the saved start to its stairs"), [this]()
			{
				FindLane();
				ASSERT_THAT(IsTrue(Barrier.IsValid() && Deck.IsValid()));
				if (!Barrier.IsValid() || !Deck.IsValid() || !Owner()) return;
				ASSERT_THAT(IsTrue(BarrierBounds.GetSize().X > 10.0 && BarrierBounds.GetSize().X <= 59.0));
				ASSERT_THAT(IsTrue(BarrierBounds.GetSize().Y >= 350.0));
				ASSERT_THAT(IsTrue(FMath::Abs(BarrierBounds.Max.Z - DeckBounds.Max.Z - 100.0) < 5.0));
				ASSERT_THAT(IsTrue(DeckBounds.Max.Z >= 90.0));
				if (APlayerController* Controller = Cast<APlayerController>(Owner()->GetController()))
				{
					// Suppress unrelated raw look input while keeping real movement and any direct
					// camera/ability SetControlRotation changes visible to the existing assertions.
					Controller->SetIgnoreLookInput(true);
					LookInputController = Controller;
				}
				SpawnLocation = Owner()->GetActorLocation();
				InputStarted = InputWorld()->GetTimeSeconds();
				Subscribe(Owner(), OwnerRecord);
				Subscribe(Authority(), AuthorityRecord);
				ProxyRecord.World = ObserverWorld;
				bDriving = true;
				TickHandle = FWorldDelegates::OnWorldTickEnd.AddRaw(this, &GaspVaultAuthoredMapPIE::OnTick);
				Key(EKeys::W, true);
			})
			.Until(TEXT("The chosen Vault crosses and lands, or the requested interruption cleans up"), [this]()
			{
				if (SpacePressedAt < 0.0) return false;
				if (bBlockExitOnServer || bCancelOnServer)
				{
					return OwnerRecord.Ends > 0 && AuthorityRecord.Ends > 0
						&& RpgVaultIntegrationTests::Clean(Owner()) && RpgVaultIntegrationTests::Clean(Authority())
						&& RpgVaultIntegrationTests::Clean(Observer())
						&& Owner()->GetCharacterMovement()->IsMovingOnGround() && Authority()->GetCharacterMovement()->IsMovingOnGround();
				}
				return Finished(OwnerRecord) && Finished(AuthorityRecord) && Finished(ProxyRecord);
			}, FTimespan::FromSeconds(45.0))
			.Then(TEXT("Validate actual Vault semantics independently of Mantle's on-obstacle completion"), [this]()
			{
				Report(TEXT("completed"));
				ASSERT_THAT(IsTrue(SpacePressedAt >= 0.0));
				ASSERT_THAT(IsTrue(bStanding ? PressSpeed < 5.0f : bWalking ? PressSpeed > 100.0f && PressSpeed < 250.0f : PressSpeed > 250.0f));
				ASSERT_THAT(IsTrue(MaximumViewError < 1.0f));
				ASSERT_THAT(IsTrue(Isolation.IsIsolated(ServerWorld.Get())));
				ASSERT_THAT(IsFalse(OwnerRecord.bSawOrdinaryJump));
				if (bBlockExitOnServer)
				{
					ASSERT_THAT(IsTrue(OwnerRecord.Commits == 1 && AuthorityRecord.Commits == 0));
					ASSERT_THAT(IsTrue(AuthorityRecord.bCancelled));
					ASSERT_THAT(IsFalse(OwnerRecord.bCrossedRear || AuthorityRecord.bCrossedRear));
					return;
				}
				ASSERT_THAT(IsTrue(OwnerRecord.Commits == 1 && AuthorityRecord.Commits == 1));
				if (bCancelOnServer)
				{
					ASSERT_THAT(IsTrue(bCancelledOnServer && AuthorityRecord.bCancelled && OwnerRecord.bCancelled));
					ASSERT_THAT(IsTrue(OwnerRecord.Ends == 1 && AuthorityRecord.Ends == 1));
					ASSERT_THAT(IsFalse(OwnerRecord.bLandedBeyond));
					ASSERT_THAT(IsTrue(AuthorityRecord.HandoffVelocity.IsNearlyZero(0.01)));
					ASSERT_THAT(IsTrue(FMath::Abs(FMath::FindDeltaAngleDegrees(Owner()->GetActorRotation().Yaw, static_cast<double>(ApproachYaw))) < 5.0));
					return;
				}
				for (const FObservation* Record : {&OwnerRecord, &AuthorityRecord, &ProxyRecord})
				{
					ASSERT_THAT(IsTrue(Record->bVaultMontage && Record->bRootMotionLease));
					if (bWalking) ASSERT_THAT(IsTrue(Record->bWalkingMontage));
					ASSERT_THAT(IsTrue(Record->LastMontageTime > Record->FirstMontageTime + 0.1f));
					ASSERT_THAT(IsTrue(Record->bCrossedRear && Record->bSawReleasedFalling && Record->bLandedBeyond));
					ASSERT_THAT(IsFalse(Record->bCorrectionDisabled));
					ASSERT_THAT(IsTrue(Record->bRestoredFacing));
					if (!bStanding) ASSERT_THAT(IsTrue(Record->bMovingAfterLanding));
					if (ApproachYaw != 0.0f)
					{
						ASSERT_THAT(IsTrue(Record->BestAlignmentError < 8.0f));
						ASSERT_THAT(IsTrue(Record->PostWarpSamples > 0 && Record->MaximumPostWarpError < 8.0f));
					}
				}
				ASSERT_THAT(IsTrue(OwnerRecord.HandoffMode == MOVE_Falling && AuthorityRecord.HandoffMode == MOVE_Falling));
				ASSERT_THAT(IsFalse(OwnerRecord.bCancelled || AuthorityRecord.bCancelled));
				ASSERT_THAT(IsTrue(OwnerRecord.bSawFrontWarp && AuthorityRecord.bSawFrontWarp && ProxyRecord.bSawFrontWarp));
				if (bStanding) ASSERT_THAT(IsTrue(OwnerRecord.bSawBackWarp && AuthorityRecord.bSawBackWarp && ProxyRecord.bSawBackWarp));
				ASSERT_THAT(IsTrue(OwnerRecord.HandoffVelocity.X > 1.0 && AuthorityRecord.HandoffVelocity.X > 1.0));
				if (ApproachYaw != 0.0f) ASSERT_THAT(IsTrue(FMath::Abs(PressYaw) >= 25.0f));
			});
	}

	UWorld* InputWorld() const { return bHost ? ServerWorld.Get() : ClientWorld.Get(); }
	ARpgCharacter* Owner() const { return RpgVaultIntegrationTests::LocalPawn(InputWorld()); }
	ARpgCharacter* Authority() const { return RpgVaultIntegrationTests::FindPawn(ServerWorld.Get(), PlayerId); }
	ARpgCharacter* Observer() const { return RpgVaultIntegrationTests::FindPawn(ObserverWorld.Get(), PlayerId); }
	bool Finished(const FObservation& Record) const
	{
		return Record.bLandedBeyond && Record.bRestoredFacing && (bStanding || Record.bMovingAfterLanding);
	}
	void FindWorlds()
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (Context.WorldType != EWorldType::PIE || !IsValid(World) || !World->GetMapName().Contains(TEXT("Lvl_RpgGaspMantle"))) continue;
			if (World->GetNetMode() == NM_ListenServer) ServerWorld = World;
			if (World->GetNetMode() == NM_Client)
			{
				if (!ClientWorld.IsValid()) ClientWorld = World;
				else if (World != ClientWorld.Get() && !ObserverWorld.IsValid()) ObserverWorld = World;
			}
		}
		if (bHost) ObserverWorld = ClientWorld;
	}
	void FindLane()
	{
		if (!RpgVaultIntegrationTests::ActiveWorld(InputWorld())) return;
		TArray<AActor*> Barriers;
		for (TActorIterator<AActor> It(InputWorld()); It; ++It)
		{
			if (It->ActorHasTag(RpgVaultIntegrationTests::BarrierTag)) Barriers.Add(*It);
		}
		Barriers.Sort([](const AActor& A, const AActor& B) { return A.GetActorLocation().Y < B.GetActorLocation().Y; });
		if (!Barriers.IsValidIndex(LaneIndex)) return;
		Barrier = Barriers[LaneIndex];
		BarrierBounds = RpgVaultIntegrationTests::PhysicalBlockBounds(*Barrier);
		for (TActorIterator<AActor> It(InputWorld()); It; ++It)
		{
			if (!It->ActorHasTag(RpgVaultIntegrationTests::ApproachTag)) continue;
			const FBox Bounds = RpgVaultIntegrationTests::PhysicalBlockBounds(**It);
			if (!Bounds.IsValid) continue;
			if (FMath::Abs(Bounds.GetCenter().Y - BarrierBounds.GetCenter().Y) < 10.0)
			{
				Deck = *It;
				DeckBounds = Bounds;
				break;
			}
		}
	}
	void Key(FKey InKey, bool bPressed)
	{
		APlayerController* Controller = Owner() ? Cast<APlayerController>(Owner()->GetController()) : nullptr;
		if (Controller) Controller->InputKey(FInputKeyEventArgs::CreateSimulated(InKey, bPressed ? IE_Pressed : IE_Released, bPressed ? 1.0f : 0.0f));
	}
	void WalkAxis(float Amount)
	{
		APlayerController* Controller = Owner() ? Cast<APlayerController>(Owner()->GetController()) : nullptr;
		// The authored IMC_Movement maps the paired Gamepad_Left2D axis to the existing IA_Move binding.
		// Partial stick input reaches CMC's normal analog speed handling; no gait, velocity or query state is forced.
		if (Controller) Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_LeftY, IE_Axis, Amount, 1));
	}
	void Subscribe(ARpgCharacter* Character, FObservation& Record)
	{
		URpgAbilitySystemComponent* ASC = Character ? Character->GetRpgAbilitySystemComponent() : nullptr;
		if (!ASC) return;
		Record.World = Character->GetWorld();
		Record.ASC = ASC;
		Record.CommittedHandle = ASC->AbilityCommittedCallbacks.AddLambda([Snapshot = &Record](UGameplayAbility* Ability)
		{
			if (Ability && Ability->GetClass()->GetPathName() == RpgVaultIntegrationTests::AbilityPath) ++Snapshot->Commits;
		});
		Record.EndedHandle = ASC->OnAbilityEnded.AddLambda(
			[WeakPawn = TWeakObjectPtr<ARpgCharacter>(Character), Snapshot = &Record](const FAbilityEndedData& Data)
		{
			ARpgCharacter* Pawn = WeakPawn.Get();
			if (!Pawn || !RpgVaultIntegrationTests::ActiveWorld(Pawn->GetWorld()) || !Data.AbilityThatEnded
				|| Data.AbilityThatEnded->GetClass()->GetPathName() != RpgVaultIntegrationTests::AbilityPath) return;
			++Snapshot->Ends;
			Snapshot->bCancelled |= Data.bWasCancelled;
			Snapshot->HandoffPosition = Pawn->GetActorLocation();
			Snapshot->HandoffVelocity = Pawn->GetVelocity();
			Snapshot->HandoffMode = Pawn->GetCharacterMovement()->MovementMode;
			UE_LOG(LogTemp, Display, TEXT("RpgVaultHandoff role=%d cancelled=%d mode=%d position=%s velocity=%s"),
				static_cast<int32>(Pawn->GetLocalRole()), Data.bWasCancelled, static_cast<int32>(Snapshot->HandoffMode),
				*Snapshot->HandoffPosition.ToCompactString(), *Snapshot->HandoffVelocity.ToCompactString());
		});
	}
	void OnTick(UWorld* World, ELevelTick TickType, float DeltaSeconds)
	{
		if (!bDriving || !RpgVaultIntegrationTests::ActiveWorld(World)) return;
		if (World == ServerWorld.Get())
		{
			Observe(Authority(), AuthorityRecord);
			if (bCancelOnServer && !bCancelledOnServer && AuthorityRecord.bRootMotionLease
				&& AuthorityRecord.LastMontageTime > AuthorityRecord.FirstMontageTime + 0.15f)
			{
				if (FGameplayAbilitySpec* Spec = RpgVaultIntegrationTests::TraversalSpec(Authority()); Spec && Spec->IsActive())
				{
					bCancelledOnServer = true;
					Authority()->GetRpgAbilitySystemComponent()->CancelAbilityHandle(Spec->Handle);
				}
			}
		}
		if (World == ObserverWorld.Get()) Observe(Observer(), ProxyRecord);
		if (World != InputWorld() || !Owner()) return;
		ARpgCharacter* Character = Owner();
		Observe(Character, OwnerRecord);
		APlayerController* Controller = Cast<APlayerController>(Character->GetController());
		if (!Controller) return;
		if (bWalkInputActive && !bMovementReleased) WalkAxis(0.5f);
		if (bSetFinalHeading)
		{
			MaximumViewError = FMath::Max(MaximumViewError,
				static_cast<float>(FMath::Abs(FMath::FindDeltaAngleDegrees(Controller->GetControlRotation().Yaw, static_cast<double>(ApproachYaw)))));
		}
		if (!bReportedWait && World->GetTimeSeconds() - InputStarted > 15.0)
		{
			Report(TEXT("checkpoint"));
			bReportedWait = true;
		}
		if (SpacePressedAt >= 0.0)
		{
			if (!bSpaceReleased && World->GetTimeSeconds() > SpacePressedAt)
			{
				Key(EKeys::SpaceBar, false);
				bSpaceReleased = true;
			}
			if (!bMovementReleased && ((Finished(OwnerRecord) && Finished(AuthorityRecord) && Finished(ProxyRecord))
				|| (bBlockExitOnServer && OwnerRecord.Ends > 0) || bCancelledOnServer))
			{
				Key(EKeys::W, false);
				if (bWalkInputActive) WalkAxis(0.0f);
				bMovementReleased = true;
			}
			return;
		}
		const FVector Position = Character->GetActorLocation();
		if (Waypoint < 2)
		{
			const FVector Destination(-2800.0, Waypoint == 0 ? SpawnLocation.Y : BarrierBounds.GetCenter().Y, Position.Z);
			if (FVector::Dist2D(Position, Destination) < 40.0) { ++Waypoint; if (Waypoint < 2) return; }
			else { Controller->SetControlRotation((Destination - Position).Rotation()); return; }
		}
		const float Distance = static_cast<float>(BarrierBounds.Min.X - Position.X);
		const float FeetZ = static_cast<float>(Position.Z - Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		if (bWalking && !bWalkInputActive && Distance < 600.0f)
		{
			Key(EKeys::W, false);
			bWalkInputActive = true;
			WalkAxis(0.5f);
		}
		if (!bSetFinalHeading)
		{
			if (Distance <= 220.0f)
			{
				Controller->SetControlRotation(FRotator(0.0, ApproachYaw, 0.0));
				bSetFinalHeading = true;
			}
			else
			{
				const double Offset = ApproachYaw == 0.0f ? 0.0 : -FMath::Sign(ApproachYaw) * 130.0;
				Controller->SetControlRotation(FVector(250.0, BarrierBounds.GetCenter().Y + Offset - Position.Y, 0.0).Rotation());
			}
		}
		if (!Character->GetCharacterMovement()->IsMovingOnGround() || FMath::Abs(FeetZ - DeckBounds.Max.Z) > 8.0) return;
		const float Speed = static_cast<float>(Character->GetVelocity().Size2D());
		if (bStanding)
		{
			if (Distance > Character->GetCapsuleComponent()->GetScaledCapsuleRadius() + 5.0f || Speed >= 5.0f) return;
			if (ContactSince < 0.0)
			{
				ContactSince = World->GetTimeSeconds();
				Key(EKeys::W, false);
				bMovementReleased = true;
			}
			if (World->GetTimeSeconds() - ContactSince < 0.15) return;
		}
		else if (Distance > 170.0f) return;
		if (bBlockExitOnServer) SpawnExitBlocker();
		PressSpeed = Speed;
		PressYaw = static_cast<float>(FRotator::NormalizeAxis(Character->GetActorRotation().Yaw));
		SpacePressedAt = World->GetTimeSeconds();
		Report(TEXT("space_pressed"));
		Key(EKeys::SpaceBar, true);
	}
	void Observe(ARpgCharacter* Character, FObservation& Record)
	{
		if (!Character || SpacePressedAt < 0.0) return;
		const URpgCharacterMovementComponent* Movement = Cast<URpgCharacterMovementComponent>(Character->GetCharacterMovement());
		if (!Movement) return;
		const bool bLease = Movement->GetMantleCollisionComponent() != nullptr;
		UAnimInstance* Animation = Character->GetMesh()->GetAnimInstance();
		UAnimMontage* Montage = Animation ? Animation->GetCurrentActiveMontage() : nullptr;
		const bool bPlayingVault = Animation && RpgVaultIntegrationTests::IsVaultMontage(*Character, Montage) && Animation->Montage_IsPlaying(Montage);
		Record.bCorrectionDisabled |= Movement->bIgnoreClientMovementErrorChecksAndCorrection || Movement->bServerAcceptClientAuthoritativePosition;
		Record.bSawOrdinaryJump |= !Record.bVaultMontage && !bPlayingVault && !bLease && Movement->IsFalling() && Record.Ends == 0;
		if (bPlayingVault)
		{
			Record.bVaultMontage = true;
			const URpgTraversalQueryComponent* Query = Character->FindComponentByClass<URpgTraversalQueryComponent>();
			Record.bWalkingMontage |= Query && Query->AllowedVaultAnimations.ContainsByPredicate(
				[Montage](const FRpgTraversalAnimationEntry& Entry)
				{ return Entry.Montage == Montage && Entry.MinSpeed >= 100.0f && Entry.MaxSpeed <= 250.0f; });
			const float Time = Animation->Montage_GetPosition(Montage);
			if (Record.FirstMontageTime < 0.0f)
			{
				Record.FirstMontageTime = Time;
				for (const FName Target : {FName(TEXT("FrontLedge")), FName(TEXT("BackLedge"))})
				{
					TArray<FMotionWarpingWindowData> Windows;
					UMotionWarpingUtilities::GetMotionWarpingWindowsForWarpTargetFromAnimation(Montage, Target, Windows);
					for (const FMotionWarpingWindowData& Window : Windows) Record.LastWarpEnd = FMath::Max(Record.LastWarpEnd, Window.EndTime);
				}
			}
			Record.LastMontageTime = Time;
			Record.bRootMotionLease |= bLease && Character->IsPlayingRootMotion() && Movement->MovementMode == MOVE_Flying;
			const UMotionWarpingComponent* Warping = Character->FindComponentByClass<UMotionWarpingComponent>();
			const FMotionWarpingTarget* Front = Warping ? Warping->FindWarpTarget(TEXT("FrontLedge")) : nullptr;
			Record.bSawBackWarp |= Warping && Warping->FindWarpTarget(TEXT("BackLedge"));
			if (Front) { Record.bSawFrontWarp = true; Record.WarpYaw = static_cast<float>(Front->Rotator().Yaw); }
			else if (Character->GetLocalRole() == ROLE_SimulatedProxy) Record.WarpYaw = AuthorityRecord.WarpYaw;
			if (bLease && Character->GetActorLocation().X >= BarrierBounds.Min.X)
			{
				const float HeadingError = static_cast<float>(FMath::Abs(FMath::FindDeltaAngleDegrees(Character->GetActorRotation().Yaw, static_cast<double>(Record.WarpYaw))));
				Record.BestAlignmentError = FMath::Min(Record.BestAlignmentError, HeadingError);
				if (Record.LastWarpEnd > 0.0f && Time > Record.LastWarpEnd + 0.001f)
				{
					++Record.PostWarpSamples;
					Record.MaximumPostWarpError = FMath::Max(Record.MaximumPostWarpError, HeadingError);
				}
			}
		}
		// Crossing must occur under traversal's lease; later ordinary input cannot satisfy this proof.
		Record.bCrossedRear |= bLease && bPlayingVault && Character->GetActorLocation().X > BarrierBounds.Max.X;
		Record.bSawReleasedFalling |= Record.bVaultMontage && !bLease && Movement->IsFalling();
		if (Record.bSawReleasedFalling && Movement->IsMovingOnGround() && RpgVaultIntegrationTests::Clean(Character))
		{
			const FVector Position = Character->GetActorLocation();
			const float Radius = Character->GetCapsuleComponent()->GetScaledCapsuleRadius();
			const UPrimitiveComponent* Floor = Movement->CurrentFloor.HitResult.GetComponent();
			Record.bLandedBeyond |= Position.X - Radius > BarrierBounds.Max.X && Floor && Movement->CurrentFloor.IsWalkableFloor()
				&& Floor->GetOwner() && !Floor->GetOwner()->ActorHasTag(RpgVaultIntegrationTests::BarrierTag)
				&& !Floor->GetOwner()->ActorHasTag(RpgVaultIntegrationTests::ApproachTag)
				&& Movement->CurrentFloor.HitResult.ImpactPoint.Z < DeckBounds.Max.Z - 50.0;
			if (Record.bLandedBeyond)
			{
				Record.bRestoredFacing |= FMath::Abs(FMath::FindDeltaAngleDegrees(Character->GetActorRotation().Yaw, static_cast<double>(ApproachYaw))) < 5.0;
				Record.bMovingAfterLanding |= Character->GetVelocity().Size2D() > 30.0;
			}
		}
	}
	void SpawnExitBlocker()
	{
		if (ExitBlocker.IsValid() || !RpgVaultIntegrationTests::ActiveWorld(ServerWorld.Get())) return;
		AActor* Blocker = ServerWorld->SpawnActor<AActor>();
		if (!Blocker) return;
		UBoxComponent* Box = NewObject<UBoxComponent>(Blocker);
		Blocker->SetRootComponent(Box);
		Blocker->AddInstanceComponent(Box);
		Box->InitBoxExtent(FVector(45.0, 180.0, 100.0));
		Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Box->SetCollisionResponseToAllChannels(ECR_Block);
		Box->SetWorldLocation(FVector(BarrierBounds.Max.X + 75.0, BarrierBounds.GetCenter().Y, DeckBounds.Max.Z + 50.0));
		Box->SetMobility(EComponentMobility::Static);
		Box->RegisterComponent();
		ExitBlocker = Blocker;
	}
	void Report(const TCHAR* Phase) const
	{
		UE_LOG(LogTemp, Display, TEXT("RpgVaultInput phase=%s host=%d standing=%d walking=%d lane=%d yaw=%.2f waypoint=%d position=%s barrierMin=%s barrierMax=%s deckTop=%.2f pressSpeed=%.2f pressYaw=%.2f viewError=%.2f blocked=%d cancel=%d"),
			Phase, bHost, bStanding, bWalking, LaneIndex, ApproachYaw, Waypoint, Owner() ? *Owner()->GetActorLocation().ToCompactString() : TEXT("None"),
			*BarrierBounds.Min.ToCompactString(), *BarrierBounds.Max.ToCompactString(), DeckBounds.Max.Z, PressSpeed, PressYaw,
			MaximumViewError, bBlockExitOnServer, bCancelledOnServer);
		for (const FObservation* Record : {&OwnerRecord, &AuthorityRecord, &ProxyRecord})
		{
			UE_LOG(LogTemp, Display, TEXT("RpgVaultObservation world=%s commits=%d ends=%d cancelled=%d vault=%d rootLease=%d montage=%.3f..%.3f crossed=%d fallingAfterRelease=%d landedBeyond=%d handoffMode=%d handoff=%s velocity=%s alignmentError=%.2f postWarpSamples=%d postWarpError=%.2f restoredFacing=%d continuedMoving=%d correctionDisabled=%d"),
				*GetPathNameSafe(Record->World.Get()), Record->Commits, Record->Ends, Record->bCancelled, Record->bVaultMontage,
				Record->bRootMotionLease, Record->FirstMontageTime, Record->LastMontageTime, Record->bCrossedRear,
				Record->bSawReleasedFalling, Record->bLandedBeyond, static_cast<int32>(Record->HandoffMode),
				*Record->HandoffPosition.ToCompactString(), *Record->HandoffVelocity.ToCompactString(), Record->BestAlignmentError,
				Record->PostWarpSamples, Record->MaximumPostWarpError, Record->bRestoredFacing, Record->bMovingAfterLanding, Record->bCorrectionDisabled);
		}
	}
	void Cleanup()
	{
		if (bDriving) Report(TEXT("teardown"));
		FWorldDelegates::OnWorldTickEnd.Remove(TickHandle);
		TickHandle.Reset();
		for (FObservation* Record : {&OwnerRecord, &AuthorityRecord})
		{
			if (Record->ASC.IsValid())
			{
				Record->ASC->AbilityCommittedCallbacks.Remove(Record->CommittedHandle);
				Record->ASC->OnAbilityEnded.Remove(Record->EndedHandle);
			}
			Record->ASC.Reset();
		}
		Key(EKeys::W, false);
		if (bWalkInputActive) WalkAxis(0.0f);
		Key(EKeys::SpaceBar, false);
		if (LookInputController.IsValid()) LookInputController->SetIgnoreLookInput(false);
		LookInputController.Reset();
		bDriving = false;
		if (ExitBlocker.IsValid()) ExitBlocker->Destroy();
		if (bOwnsSession && GUnrealEd) { GUnrealEd->EndPlayMap(); bOwnsSession = false; }
		if (bConfigured)
		{
			GetMutableDefault<URpgDeveloperSettings>()->ExperienceOverride = PreviousExperience;
			bConfigured = false;
		}
		PlaySettings.Reset();
	}
};

#endif // ENABLE_PIE_NETWORK_TEST
#endif // WITH_DEV_AUTOMATION_TESTS
