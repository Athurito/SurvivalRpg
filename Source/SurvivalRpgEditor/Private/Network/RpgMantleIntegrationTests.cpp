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
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "InputActionValue.h"
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
#include "SurvivalRpg/Traversal/RpgMantleAnchorComponent.h"

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
	TestTrue(TEXT("Existing camera, input and RPG inventory composition are preserved"),
		PawnData->InputConfig && PawnData->InputConfig == CmcPawnData->InputConfig
		&& PawnData->DefaultCameraMode == CmcPawnData->DefaultCameraMode
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
	TestNotNull(TEXT("Designer ability supplies a root-motion montage"), Ability->Montage.Get());
	if (Ability->Montage)
	{
		const ARpgCharacter* Pawn = PawnClass->GetDefaultObject<ARpgCharacter>();
		const ARpgCharacter* CmcPawn = CmcPawnData->PawnClass->GetDefaultObject<ARpgCharacter>();
		TestTrue(TEXT("The accepted CMC gameplay mesh and AnimBP remain in use"),
			Pawn->GetMesh()->GetSkeletalMeshAsset() == CmcPawn->GetMesh()->GetSkeletalMeshAsset()
			&& Pawn->GetMesh()->GetAnimClass() == CmcPawn->GetMesh()->GetAnimClass());
		TestTrue(TEXT("Montage contains authored root motion"), Ability->Montage->HasRootMotion());
		TestTrue(TEXT("Montage uses the existing gameplay animation slot"), Ability->Montage->IsValidSlot(TEXT("DefaultSlot")));
		TestTrue(TEXT("Montage is project-owned"), Ability->Montage->GetPathName().StartsWith(TEXT("/Game/SurvivalRpg/")));
	}
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
	URpgMantleAnchorComponent* AnchorInWorld(UWorld* World)
	{
		if (!IsActiveTestWorld(World)) return nullptr;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (URpgMantleAnchorComponent* Anchor = It->FindComponentByClass<URpgMantleAnchorComponent>()) return Anchor;
		}
		return nullptr;
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
			&& Character->FindComponentByClass<UMotionWarpingComponent>();
	}
	bool Grounded(const ARpgCharacter* Character)
	{
		return Character && Character->GetCharacterMovement()->IsMovingOnGround() && Character->GetVelocity().Size2D() < 5.0;
	}
	FVector EntryPosition(const ARpgCharacter& Character, const URpgMantleAnchorComponent& Anchor, float Distance = 110.0f)
	{
		FVector Position = Anchor.GetComponentLocation() - Anchor.GetForwardVector() * Distance;
		Position.Z = Character.GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.5;
		return Position;
	}
	FVector LandingPosition(const ARpgCharacter& Character, const URpgMantleAnchorComponent& Anchor)
	{
		return Anchor.GetLandingLocation() + FVector(0.0, 0.0, Character.GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.5);
	}
	bool HasCleanMantleState(ARpgCharacter* Character)
	{
		if (!Character) return false;
		const URpgGameplayAbility_Mantle* Definition = AbilityDefinition();
		const FGameplayAbilitySpec* Spec = MantleSpec(Character);
		const UMotionWarpingComponent* Warping = Character->FindComponentByClass<UMotionWarpingComponent>();
		if (!Definition || !Warping || (Spec && Spec->IsActive()) || Warping->FindWarpTarget(Definition->WarpTargetName)) return false;
		const URpgAbilitySystemComponent* ASC = Character->GetRpgAbilitySystemComponent();
		const UAnimInstance* Animation = Character->GetMesh()->GetAnimInstance();
		const URpgMantleAnchorComponent* Anchor = AnchorInWorld(Character->GetWorld());
		const URpgCharacterMovementComponent* Movement = Cast<URpgCharacterMovementComponent>(Character->GetCharacterMovement());
		return (!ASC || ASC->GetCurrentMontage() != Definition->Montage)
			&& (!Animation || !Animation->Montage_IsPlaying(Definition->Montage))
			&& Movement && !Movement->GetMantleCollisionComponent()
			&& (!Anchor || !Character->GetCapsuleComponent()->GetMoveIgnoreComponents().Contains(Anchor->GetTraversedComponent()))
			&& Character->GetCharacterMovement()->MovementMode != MOVE_Flying;
	}
	bool CompletedMantle(UWorld* World, int32 PlayerId)
	{
		ARpgCharacter* Character = FindCharacter(World, PlayerId);
		const URpgMantleAnchorComponent* Anchor = AnchorInWorld(World);
		return Anchor && Grounded(Character) && HasCleanMantleState(Character)
			&& Character->GetActorLocation().Equals(LandingPosition(*Character, *Anchor), 45.0);
	}
	bool ReadyAtEntry(UWorld* World, int32 PlayerId)
	{
		ARpgCharacter* Character = FindCharacter(World, PlayerId);
		const URpgGameplayAbility_Mantle* Definition = AbilityDefinition();
		const URpgMantleAnchorComponent* Anchor = AnchorInWorld(World);
		return Ready(World, Character) && Grounded(Character) && HasCleanMantleState(Character) && Anchor && Definition
			&& Character->GetActorLocation().Equals(EntryPosition(*Character, *Anchor), 12.0)
			&& Definition->FindCandidate(*Character) == Anchor;
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
		URpgMantleAnchorComponent* Anchor = AnchorInWorld(World);
		if (!Character || !Anchor || !Character->HasAuthority()) return;
		const FVector Requested = EntryPosition(*Character, *Anchor);
		const bool bTeleported = Character->TeleportTo(Requested, Anchor->GetComponentRotation());
		if (!bTeleported || !Character->GetActorLocation().Equals(Requested, 1.0))
		{
			UE_LOG(LogTemp, Display, TEXT("RpgMantleFixturePlacement success=%d requested=%s actual=%s"),
				bTeleported, *Requested.ToCompactString(), *Character->GetActorLocation().ToCompactString());
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
			const URpgGameplayAbility_Mantle* Definition = AbilityDefinition();
			const bool bMontagePlaying = Animation && Definition && Animation->Montage_IsPlaying(Definition->Montage);
			if (bMontagePlaying)
			{
				const float Position = Animation->Montage_GetPosition(Definition->Montage);
				if (Record->FirstMontagePosition < 0.0f) Record->FirstMontagePosition = Position;
				Record->LastMontagePosition = Position;
				Record->bRootMotion |= Character->IsPlayingRootMotion();
				Record->bFlying |= Character->GetCharacterMovement()->MovementMode == MOVE_Flying;
				const URpgMantleAnchorComponent* Anchor = AnchorInWorld(World);
				Record->bCollisionLease |= Movement && Anchor && Movement->GetIsReplicated()
					&& Movement->GetMantleCollisionComponent() == Anchor->GetTraversedComponent()
					&& Character->GetCapsuleComponent()->GetMoveIgnoreComponents().Contains(Anchor->GetTraversedComponent());
			}
			const URpgMantleAnchorComponent* Anchor = AnchorInWorld(World);
			const FVector Actual = Character->GetActorLocation();
			const FVector Expected = Anchor ? LandingPosition(*Character, *Anchor) : FVector::ZeroVector;
			const FGameplayAbilitySpec* Spec = MantleSpec(Character);
			const URpgAbilitySystemComponent* ASC = Character->GetRpgAbilitySystemComponent();
			const UMotionWarpingComponent* Warping = Character->FindComponentByClass<UMotionWarpingComponent>();
			Record->LastSnapshot = FString::Printf(TEXT("{\"observation\":%d,\"world\":\"%s\",\"netMode\":%d,\"role\":%d,\"elapsed\":%.3f,\"position\":[%.3f,%.3f,%.3f],\"expectedLanding\":[%.3f,%.3f,%.3f],\"landingDistance\":%.3f,\"mode\":%d,\"speed2D\":%.3f,\"grounded\":%d,\"clean\":%d,\"specActive\":%d,\"animationPlaying\":%d,\"montagePosition\":%.3f,\"animMontage\":\"%s\",\"ascMontage\":\"%s\",\"lease\":\"%s\",\"warpTarget\":%d}"),
				ObservationId, *World->GetPathName().ReplaceCharWithEscapedChar(), static_cast<int32>(World->GetNetMode()),
				static_cast<int32>(Character->GetLocalRole()), World->GetTimeSeconds() - Record->StartTime,
				Actual.X, Actual.Y, Actual.Z, Expected.X, Expected.Y, Expected.Z, FVector::Dist(Actual, Expected),
				static_cast<int32>(Character->GetCharacterMovement()->MovementMode), Character->GetVelocity().Size2D(),
				Grounded(Character), HasCleanMantleState(Character), Spec && Spec->IsActive(), bMontagePlaying,
				Animation && Definition ? Animation->Montage_GetPosition(Definition->Montage) : -1.0f,
				*GetPathNameSafe(Animation ? Animation->GetCurrentActiveMontage() : nullptr).ReplaceCharWithEscapedChar(),
				*GetPathNameSafe(ASC ? ASC->GetCurrentMontage() : nullptr).ReplaceCharWithEscapedChar(),
				*GetPathNameSafe(Movement ? Movement->GetMantleCollisionComponent() : nullptr).ReplaceCharWithEscapedChar(),
				Warping && Definition && Warping->FindWarpTarget(Definition->WarpTargetName));
			if (!Record->bWasMontagePlaying && bMontagePlaying) Report(TEXT("montage_started"), *Record);
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

	void ReportEntryBlockers(UWorld* World, const ARpgCharacter& Character, const URpgMantleAnchorComponent& Anchor)
	{
		if (!IsActiveTestWorld(World)) return;
		const UCapsuleComponent* Capsule = Character.GetCapsuleComponent();
		TArray<FOverlapResult> Overlaps;
		World->OverlapMultiByChannel(Overlaps, EntryPosition(Character, Anchor), Capsule->GetComponentQuat(),
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
				AActor* Obstacle = State.World->SpawnActor<AActor>(ObstacleClass, FVector(1000.0, 0.0, 0.0), FRotator::ZeroRotator, Params);
				ASSERT_THAT(IsNotNull(Obstacle));
				ASSERT_THAT(IsNotNull(Obstacle ? Obstacle->FindComponentByClass<URpgMantleAnchorComponent>() : nullptr));
				int32 ParkingIndex = 0;
				for (TActorIterator<ARpgCharacter> It(State.World); It; ++It)
				{
					It->TeleportTo(FVector(-1000.0, 2000.0 + 1000.0 * ParkingIndex++, 100.0), FRotator::ZeroRotator);
					It->GetCharacterMovement()->StopMovementImmediately();
					It->ForceNetUpdate();
				}
				PositionAtEntry(State.World, SubjectId);
			})
			.UntilClient(TEXT("Prepared anchor and authoritative entry position reach the owner"), 0, [this](FState& State)
				{ return ReadyAtEntry(State.World, SubjectId); }, Timeout())
			.ThenServer(TEXT("Server rejects unmarked, distant, excessive-height and obstructed landing proposals"), [this](FState& State)
			{
				ARpgCharacter* Character = FindCharacter(State.World, SubjectId);
				URpgMantleAnchorComponent* Anchor = AnchorInWorld(State.World);
				const URpgGameplayAbility_Mantle* Definition = AbilityDefinition();
				ASSERT_THAT(IsNotNull(Character));
				ASSERT_THAT(IsNotNull(Anchor));
				if (!Character || !Anchor || !Definition) return;
				ASSERT_THAT(IsTrue(Definition->ValidateAnchor(*Character, *Anchor)));
				const FTransform Entry = Character->GetActorTransform();
				Character->SetActorLocation(Entry.GetLocation() - Anchor->GetForwardVector() * (Anchor->MaxApproachDistance + 100.0f));
				ASSERT_THAT(IsFalse(Definition->ValidateAnchor(*Character, *Anchor)));
				Character->SetActorLocation(Entry.GetLocation() - FVector(0.0, 0.0, Anchor->MaxHeight + 100.0f));
				ASSERT_THAT(IsFalse(Definition->ValidateAnchor(*Character, *Anchor)));
				Character->SetActorTransform(Entry);
				AActor* Blocker = SpawnBlocker(State.World, LandingPosition(*Character, *Anchor), FVector(35.0, 35.0, 60.0));
				ASSERT_THAT(IsNotNull(Blocker));
				ASSERT_THAT(IsFalse(Definition->ValidateAnchor(*Character, *Anchor)));
				if (Blocker) Blocker->Destroy();
				Blocker = SpawnBlocker(State.World, Entry.GetLocation() + Anchor->GetForwardVector() * 65.0, FVector(12.0, 100.0, 100.0));
				ASSERT_THAT(IsNotNull(Blocker));
				ASSERT_THAT(IsNull(Definition->FindCandidate(*Character)));
				if (Blocker) Blocker->Destroy();
				ASSERT_THAT(IsTrue(Definition->FindCandidate(*Character) == Anchor));
				Character->TeleportTo(EntryPosition(*Character, *Anchor, 500.0f), Anchor->GetComponentRotation());
				Character->GetCharacterMovement()->StopMovementImmediately();
				Character->ForceNetUpdate();
			})
			.UntilClient(TEXT("Owner is grounded outside the prepared entry"), 0, [](FState& State)
				{ ARpgCharacter* Character = LocalCharacter(State.World); return Grounded(Character) && !AbilityDefinition()->FindCandidate(*Character); }, Timeout())
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
				const URpgMantleAnchorComponent* Anchor = AnchorInWorld(State.World);
				if (!Character || !Anchor) return;
				State.LandingBlocker = SpawnBlocker(State.World, LandingPosition(*Character, *Anchor), FVector(35.0, 35.0, 60.0));
				ASSERT_THAT(IsTrue(State.LandingBlocker.IsValid()));
			})
			.ThenClient(TEXT("Owner predicts its locally clear entry through ordinary jump input"), 0, [this](FState& State)
			{
				ARpgCharacter* Character = LocalCharacter(State.World);
				if (!Character) return;
				ASSERT_THAT(IsNotNull(AbilityDefinition()->FindCandidate(*Character)));
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
					&& Character->GetActorLocation().Equals(EntryPosition(*Character, *AnchorInWorld(State.World)), 20.0);
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
				URpgMantleAnchorComponent* OldAnchor = AnchorInWorld(State.World);
				if (!Character || !OldAnchor) return;
				ReportEntryBlockers(State.World, *Character, *OldAnchor);
				FTransform NewLane = OldAnchor->GetOwner()->GetActorTransform();
				NewLane.AddToTranslation(FVector(0.0, 2000.0, 0.0));
				ASSERT_THAT(IsTrue(OldAnchor->GetOwner()->Destroy()));
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
				URpgMantleAnchorComponent* Anchor = AnchorInWorld(State.World);
				ASSERT_THAT(IsNotNull(Anchor));
				if (!Anchor) return;
				AActor* Obstacle = Anchor->GetOwner();
				State.DestroyedObstacleTransform = Obstacle->GetActorTransform();
				ASSERT_THAT(IsTrue(Obstacle->Destroy()));
			})
			.UntilServer(TEXT("Destroyed obstacle releases authority ability, warp and stale collision lease"), [this](FState& State)
			{
				ARpgCharacter* Character = FindCharacter(State.World, SubjectId);
				return !AnchorInWorld(State.World) && Grounded(Character) && HasCleanMantleState(Character);
			}, Timeout())
			.UntilClient(TEXT("Owner reconciles obstacle destruction and returns to grounded movement"), 0, [this](FState& State)
			{
				ARpgCharacter* Character = FindCharacter(State.World, SubjectId);
				return !AnchorInWorld(State.World) && Grounded(Character) && HasCleanMantleState(Character);
			}, Timeout())
			.UntilClient(TEXT("Simulated observer also releases the destroyed obstacle lease"), 1, [this](FState& State)
			{
				ARpgCharacter* Character = FindCharacter(State.World, SubjectId);
				return !AnchorInWorld(State.World) && Grounded(Character) && HasCleanMantleState(Character);
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

#endif // ENABLE_PIE_NETWORK_TEST
#endif // WITH_DEV_AUTOMATION_TESTS
