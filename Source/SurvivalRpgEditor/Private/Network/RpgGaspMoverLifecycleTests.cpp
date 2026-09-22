// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"
#include "Network/RpgCombatNetworkTestTypes.h"
#include "Network/RpgMoverPredictionTestHelpers.h"
#include "Network/RpgMoverPredictionTestTypes.h"
#include "AbilitySystemGlobals.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Backends/MoverNetworkPredictionLiaison.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/OverlapResult.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerStart.h"
#include "GameplayEffect.h"
#include "InputKeyEventArgs.h"
#include "Misc/Guid.h"
#include "MoverDataModelTypes.h"
#include "NetworkPredictionWorldManager.h"
#include "Retargeter/IKRetargeter.h"
#include "UObject/StrongObjectPtr.h"
#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility_BasicWeaponAttack.h"
#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility_Block.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgHealthSet.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Animation/RpgRuntimeRetargetComponent.h"
#include "SurvivalRpg/Animation/RpgRuntimeRetargetProfile.h"
#include "SurvivalRpg/Core/Character/RpgCharacterMoverComponent.h"
#include "SurvivalRpg/Core/Character/RpgDeadMovementMode.h"
#include "SurvivalRpg/Core/Character/RpgHealthComponent.h"
#include "SurvivalRpg/Core/Character/RpgMoverPawn.h"
#include "SurvivalRpg/Core/Character/RpgPawnData.h"
#include "SurvivalRpg/Core/Character/RpgPawnExtensionComponent.h"
#include "SurvivalRpg/Core/Character/RpgPawnGameplayComponent.h"
#include "SurvivalRpg/Core/Game/Experience/RpgExperienceDefinition.h"
#include "SurvivalRpg/Core/Game/Experience/RpgExperienceManagerComponent.h"
#include "SurvivalRpg/Core/Game/RpgGameModeBase.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/Development/RpgDeveloperSettings.h"
#include "SurvivalRpg/Equipment/RpgEquipmentManagerComponent.h"
#include "SurvivalRpg/Equipment/RpgWeaponInstance.h"
#include "SurvivalRpg/Physics/RpgCollisionChannels.h"
#include "SurvivalRpg/System/RpgGameData.h"

#if ENABLE_PIE_NETWORK_TEST

namespace RpgGaspMoverLifecycleTests
{
	enum class ERespawnStartScenario : uint8
	{
		Unconstrained,
		SingleLateJoiner,
		HostAndLateJoiner
	};

	constexpr TCHAR GameModePath[] = TEXT("/Game/SurvivalRpg/Maps/Test/GaspMover/BP_Rpg_GaspMoverTestGameMode.BP_Rpg_GaspMoverTestGameMode_C");
	constexpr TCHAR PawnDataPath[] = TEXT("/Game/SurvivalRpg/Characters/GASP/Mover/RPG/DA_PawnData_GaspMover.DA_PawnData_GaspMover");
	constexpr TCHAR UefnMeshPath[] = TEXT("/Game/SurvivalRpg/Characters/GASP/Shared/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin.SKM_UEFN_Mannequin");
	FGameplayTag Tag(const TCHAR* Name) { return FGameplayTag::RequestGameplayTag(Name); }
	FPrimaryAssetId ExperienceId() { return FPrimaryAssetId(URpgExperienceDefinition::StaticClass()->GetFName(), TEXT("RpgGaspMoverExperience")); }
	FTimespan Timeout() { return FTimespan::FromSeconds(35.0); }
	bool ActiveWorld(const UWorld* World)
	{
		if (!GEngine || !World) return false;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
			if (Context.WorldType == EWorldType::PIE && Context.World() == World)
				return IsValid(World) && !World->bIsTearingDown && !World->IsBeingCleanedUp();
		return false;
	}
	ARpgPlayerState* Player(UWorld* World, int32 Id)
	{
		const AGameStateBase* State = ActiveWorld(World) ? World->GetGameState() : nullptr;
		if (State) for (APlayerState* Candidate : State->PlayerArray)
			if (Candidate && Candidate->GetPlayerId() == Id) return Cast<ARpgPlayerState>(Candidate);
		return nullptr;
	}
	APawn* Pawn(UWorld* World, int32 Id) { const ARpgPlayerState* State = Player(World, Id); return State ? State->GetPawn() : nullptr; }
	APawn* LocalPawn(UWorld* World) { const APlayerController* PC = ActiveWorld(World) ? World->GetFirstPlayerController() : nullptr; return PC ? PC->GetPawn() : nullptr; }
	USkeletalMeshComponent* Mesh(const APawn* Character) { return URpgPawnExtensionComponent::FindGameplayMesh(Character); }
	URpgHealthComponent* Health(const APawn* Character) { return URpgHealthComponent::FindHealthComponent(Character); }
	URpgCharacterMoverComponent* Mover(const APawn* Character) { return Character ? Character->FindComponentByClass<URpgCharacterMoverComponent>() : nullptr; }
	URpgRuntimeRetargetComponent* Retarget(const APawn* Character) { return Character ? Character->FindComponentByClass<URpgRuntimeRetargetComponent>() : nullptr; }
	URpgEquipmentManagerComponent* Equipment(const APawn* Character) { return Character ? Character->FindComponentByClass<URpgEquipmentManagerComponent>() : nullptr; }
	URpgAbilitySystemComponent* ASC(const APawn* Character)
	{
		const URpgPawnExtensionComponent* Extension = URpgPawnExtensionComponent::FindPawnExtensionComponent(Character);
		return Extension ? Extension->GetRpgAbilitySystemComponent() : nullptr;
	}
	FGameplayAbilitySpec* CombatSpec(APawn* Character, bool bBlock)
	{
		if (!ASC(Character) || !Equipment(Character)) return nullptr;
		const FGameplayTag Input = Tag(bBlock ? TEXT("InputTag.Weapon.Block") : TEXT("InputTag.Weapon.Primary"));
		for (FGameplayAbilitySpec& Spec : ASC(Character)->GetActivatableAbilities())
		{
			const URpgEquipmentInstance* Item = Cast<URpgEquipmentInstance>(Spec.SourceObject.Get());
			const UGameplayAbility* Ability = Spec.GetPrimaryInstance();
			if (!Spec.PendingRemove && Item && Ability && Spec.GetDynamicSpecSourceTags().HasTagExact(Input)
				&& Equipment(Character)->IsEquipmentInstanceActiveForInputTag(Item, Input)
				&& (bBlock ? Ability->IsA<URpgGameplayAbility_Block>() : Ability->IsA<URpgGameplayAbility_BasicWeaponAttack>())) return &Spec;
		}
		return nullptr;
	}
	bool Equipped(APawn* Character)
	{
		if (!Equipment(Character) || !Mesh(Character)) return false;
		for (ERpgEquipmentSlot Slot : { ERpgEquipmentSlot::MainHand, ERpgEquipmentSlot::OffHand })
		{
			const URpgEquipmentInstance* Item = Equipment(Character)->GetEquipmentInstanceInSlot(Slot);
			if (!Item || Item->GetPawn() != Character || Item->GetSpawnedActors().IsEmpty()) return false;
			for (const AActor* Actor : Item->GetSpawnedActors())
			{
				const USceneComponent* Root = Actor ? Actor->GetRootComponent() : nullptr;
				if (!Root || Root->GetAttachParent() != Mesh(Character) || Root->GetAttachSocketName().IsNone()
					|| !Mesh(Character)->DoesSocketExist(Root->GetAttachSocketName())) return false;
			}
		}
		return Character->GetLocalRole() == ROLE_SimulatedProxy || (CombatSpec(Character, false) && CombatSpec(Character, true));
	}
	bool VisibleFollower(APawn* Character)
	{
		const USkeletalMeshComponent* Target = Retarget(Character) ? Retarget(Character)->GetRetargetMesh() : nullptr;
		return Target && Target->IsRegistered() && Target->IsVisible() && !Target->bHiddenInGame && Target->GetAnimInstance();
	}
	bool Ready(UWorld* World, APawn* Character, bool bFollower)
	{
		const AGameStateBase* GameState = ActiveWorld(World) ? World->GetGameState() : nullptr;
		const URpgExperienceManagerComponent* Experience = GameState ? GameState->FindComponentByClass<URpgExperienceManagerComponent>() : nullptr;
		const URpgPawnExtensionComponent* Extension = URpgPawnExtensionComponent::FindPawnExtensionComponent(Character);
		const URpgPawnGameplayComponent* Gameplay = URpgPawnGameplayComponent::FindPawnGameplayComponent(Character);
		const ARpgPlayerState* State = Character ? Character->GetPlayerState<ARpgPlayerState>() : nullptr;
		const USkeletalMeshComponent* Source = Mesh(Character);
		return Experience && Experience->IsExperienceLoaded() && Experience->GetCurrentExperienceChecked()->GetPrimaryAssetId() == ExperienceId()
			&& Character && Character->IsA<ARpgMoverPawn>() && State && Extension && Gameplay && Mover(Character) && Health(Character) && Retarget(Character)
			&& Extension->HasReachedInitState(Tag(TEXT("InitState.GameplayReady"))) && Gameplay->HasReachedInitState(Tag(TEXT("InitState.GameplayReady")))
			&& Extension->GetPawnData<URpgPawnData>() && Extension->GetPawnData<URpgPawnData>()->GetPathName() == PawnDataPath
			&& State->GetPawnData<URpgPawnData>() == Extension->GetPawnData<URpgPawnData>()
			&& Retarget(Character)->GetRetargetProfile() == Extension->GetPawnData<URpgPawnData>()->RuntimeRetargetProfile
			&& ASC(Character) && ASC(Character) == State->GetRpgAbilitySystemComponent() && ASC(Character)->GetAvatarActor() == Character
			&& ASC(Character)->AbilityActorInfo.IsValid() && ASC(Character)->AbilityActorInfo->SkeletalMeshComponent.Get() == Source
			&& Source && Source->GetAnimInstance() && Source->GetSkeletalMeshAsset() && Source->GetSkeletalMeshAsset()->GetPathName() == UefnMeshPath
			&& Mover(Character)->GetPrimaryVisualComponent() == Source && Health(Character)->GetHealth() > 0.0f && !Health(Character)->IsDeadOrDying()
			&& Equipped(Character) && (bFollower ? VisibleFollower(Character) : Retarget(Character)->GetRetargetMesh() == nullptr);
	}
	/** Retained through PIE teardown: no map, player profile or world persistence is read from disk. */
	class FScopedWorld final
	{
	public:
		~FScopedWorld() { FGameModeEvents::OnGameModeInitializedEvent().Remove(Handle); }
		void Start()
		{
			Prefix = TEXT("SurvivalRpg_MoverLifecycleAutomation_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
			Handle = FGameModeEvents::OnGameModeInitializedEvent().AddRaw(this, &FScopedWorld::Initialize);
		}
		bool Isolated(UWorld* World) const
		{
			if (!ActiveWorld(World)) return false;
			if (World->GetNetMode() == NM_Client) return World->GetAuthGameMode() == nullptr;
			const ARpgGameModeBase* Mode = World->GetAuthGameMode<ARpgGameModeBase>();
			return Mode && !Mode->bEnableDiskPersistence && Mode->WorldSaveSlotName.StartsWith(Prefix) && Mode->OfflineProfileKey == Prefix;
		}
		bool FindFreeSupportedStart(UWorld* World, const APawn* Character, FTransform& OutTransform) const
		{
			const UCapsuleComponent* Capsule = Character ? Cast<UCapsuleComponent>(Character->GetRootComponent()) : nullptr;
			if (!Isolated(World) || World->GetNetMode() == NM_Client || !Capsule) return false;
			for (const TWeakObjectPtr<APlayerStart>& Start : Starts)
			{
				if (!Start.IsValid() || Start->GetWorld() != World) continue;
				const FTransform Transform = Start->GetActorTransform();
				const FCollisionQueryParams Query(SCENE_QUERY_STAT(RpgMoverLifecycleFreeStart), false, Start.Get());
				const FCollisionResponseParams Response(Capsule->GetCollisionResponseToChannels());
				if (World->OverlapBlockingTestByChannel(Transform.GetLocation(), Transform.GetRotation(),
					Capsule->GetCollisionObjectType(), FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()),
					Query, Response)) continue;
				FHitResult Floor;
				if (!World->LineTraceSingleByChannel(Floor, Transform.GetLocation(), Transform.GetLocation() - FVector(0, 0, 500),
					Capsule->GetCollisionObjectType(), Query, Response) || !Floor.IsValidBlockingHit() || Floor.ImpactNormal.Z < 0.7f) continue;
				OutTransform = Transform;
				return true;
			}
			return false;
		}
		bool RetainOnlyStart(UWorld* World, const FTransform& Transform)
		{
			if (!Isolated(World) || World->GetNetMode() == NM_Client) return false;
			APlayerStart* Retained = nullptr;
			for (const TWeakObjectPtr<APlayerStart>& Start : Starts)
				if (Start.IsValid() && Start->GetWorld() == World && Start->GetActorTransform().Equals(Transform, 0.01))
				{
					if (Retained) return false;
					Retained = Start.Get();
				}
			if (!Retained) return false;
			// Only this fixture's transient starts are removed. The retained start and all pawns live until PIE teardown.
			for (const TWeakObjectPtr<APlayerStart>& Start : Starts)
				if (Start.IsValid() && Start->GetWorld() == World && Start.Get() != Retained && !Start->Destroy()) return false;
			return true;
		}
		bool FindStartOccupiedByPawn(UWorld* World, const APawn* Character, FTransform& OutTransform) const
		{
			const UCapsuleComponent* Capsule = Character ? Cast<UCapsuleComponent>(Character->GetRootComponent()) : nullptr;
			if (!Isolated(World) || World->GetNetMode() == NM_Client || !Capsule) return false;
			for (const TWeakObjectPtr<APlayerStart>& Start : Starts)
			{
				if (!Start.IsValid() || Start->GetWorld() != World
					|| FVector::Dist2D(Start->GetActorLocation(), Character->GetActorLocation()) > Capsule->GetScaledCapsuleRadius()) continue;
				TArray<FOverlapResult> Overlaps;
				World->OverlapMultiByChannel(Overlaps, Start->GetActorLocation(), Start->GetActorQuat(), Capsule->GetCollisionObjectType(),
					FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()),
					FCollisionQueryParams(SCENE_QUERY_STAT(RpgMoverLifecycleHostStart), false, Start.Get()),
					FCollisionResponseParams(Capsule->GetCollisionResponseToChannels()));
				if (!Overlaps.ContainsByPredicate([Character, Capsule](const FOverlapResult& Overlap)
					{ return Overlap.bBlockingHit && Overlap.GetActor() == Character && Overlap.GetComponent() == Capsule; })) continue;
				OutTransform = Start->GetActorTransform();
				return true;
			}
			return false;
		}
	private:
		void Initialize(AGameModeBase* Initialized)
		{
			ARpgGameModeBase* Mode = Cast<ARpgGameModeBase>(Initialized);
			if (!Mode || Mode->GetWorld()->WorldType != EWorldType::PIE) return;
			Mode->bEnableDiskPersistence = false;
			Mode->WorldSaveSlotName = Prefix;
			Mode->WorldSaveBackupSlotName = Prefix + TEXT("_Backup");
			Mode->WorldSaveRecoverySlotName = Prefix + TEXT("_Recovery");
			Mode->OfflineProfileKey = Prefix;
			FActorSpawnParameters Spawn;
			Spawn.ObjectFlags |= RF_Transient;
			Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Mode->GetWorld()->SpawnActor<ARpgCombatNetworkFloorFixture>(FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
			for (int32 Index = 0; Index < 3; ++Index)
				Starts.Add(Mode->GetWorld()->SpawnActor<APlayerStart>(FVector(0, (Index - 1) * 500.0, 120.0), FRotator::ZeroRotator, Spawn));
		}
		FString Prefix;
		FDelegateHandle Handle;
		TArray<TWeakObjectPtr<APlayerStart>> Starts;
	};
	class FScopedInput final
	{
	public:
		~FScopedInput() { Stop(); }
		void Start(APawn* Character)
		{
			Stop();
			PC = Cast<APlayerController>(Character->GetController());
			PC->SetIgnoreLookInput(true);
			PC->SetControlRotation(FRotator::ZeroRotator);
			Handle = FWorldDelegates::OnWorldTickStart.AddRaw(this, &FScopedInput::Tick);
		}
		void Move(bool bEnabled) { AxisValue = bEnabled ? 1.0f : 0.0f; }
		void Attack() { Key(EKeys::LeftMouseButton, true); ReleaseFrames = 2; }
		void Block(bool bHeld) { Key(EKeys::RightMouseButton, bHeld); }
		void Stop()
		{
			FWorldDelegates::OnWorldTickStart.Remove(Handle);
			Handle.Reset();
			if (PC.IsValid())
			{
				PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_LeftY, IE_Axis, 0.0f, 1));
				Key(EKeys::LeftMouseButton, false); Key(EKeys::RightMouseButton, false);
				PC->SetIgnoreLookInput(false);
			}
			PC.Reset(); AxisValue = 0.0f; ReleaseFrames = 0;
		}
	private:
		void Key(FKey Input, bool bPressed) { if (PC.IsValid()) PC->InputKey(FInputKeyEventArgs::CreateSimulated(Input, bPressed ? IE_Pressed : IE_Released, bPressed ? 1.0f : 0.0f)); }
		void Tick(UWorld* World, ELevelTick, float)
		{
			if (!PC.IsValid() || PC->GetWorld() != World) return;
			PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_LeftY, IE_Axis, AxisValue, 1));
			if (ReleaseFrames > 0 && --ReleaseFrames == 0) Key(EKeys::LeftMouseButton, false);
		}
		TWeakObjectPtr<APlayerController> PC;
		FDelegateHandle Handle;
		float AxisValue = 0.0f;
		int32 ReleaseFrames = 0;
	};
	/** Named profile overrides are memory-only and restored even after an aborted test. */
	class FScopedProfile final
	{
	public:
		~FScopedProfile() { Restore(); }
		bool Enable(const URpgPawnData* Data)
		{
			Profile.Reset(Data ? const_cast<URpgRuntimeRetargetProfile*>(Data->RuntimeRetargetProfile.Get()) : nullptr);
			if (!Profile.IsValid()) return false;
			OldMesh.Reset(Profile->TargetMesh.Get()); OldRetargeter.Reset(Profile->Retargeter.Get());
			Profile->TargetMesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/SurvivalRpg/Characters/GASP/Shared/Characters/UE5_Mannequins/Meshes/SKM_Manny.SKM_Manny"));
			Profile->Retargeter = LoadObject<UIKRetargeter>(nullptr, TEXT("/Game/SurvivalRpg/Characters/GASP/Shared/Characters/UE5_Mannequins/Rigs/RTG_UEFN_to_UE5_Mannequin.RTG_UEFN_to_UE5_Mannequin"));
			return Profile->TargetMesh && Profile->Retargeter && Profile->RetargetAnimClass;
		}
		void Restore()
		{
			if (Profile.IsValid()) { Profile->TargetMesh = OldMesh.Get(); Profile->Retargeter = OldRetargeter.Get(); }
			Profile.Reset(); OldMesh.Reset(); OldRetargeter.Reset();
		}
	private:
		TStrongObjectPtr<URpgRuntimeRetargetProfile> Profile;
		TStrongObjectPtr<USkeletalMesh> OldMesh;
		TStrongObjectPtr<UIKRetargeter> OldRetargeter;
	};
	/** Introduces one owner prediction error and observes the actual NP restore before its next forward tick. */
	class FScopedDeadCorrection final
	{
	public:
		~FScopedDeadCorrection() { Stop(); }
		bool Inject(APawn* Character)
		{
			Owner = Character;
			Liaison = Character ? Character->FindComponentByClass<UMoverNetworkPredictionLiaisonComponent>() : nullptr;
			FMoverSyncState Sync;
			if (!Liaison.IsValid() || Character->GetLocalRole() != ROLE_AutonomousProxy || !Liaison->ReadPendingSyncState(Sync)
				|| Sync.MovementMode != URpgDeadMovementMode::ModeName) return false;
			const UNetworkPredictionWorldManager* Prediction = Character->GetWorld()->GetSubsystem<UNetworkPredictionWorldManager>();
			if (!Prediction || Prediction->GetSettings().PreferredTickingPolicy != ENetworkPredictionTickingPolicy::Fixed
				|| !Prediction->GetSettings().bEnableFixedTickSmoothing || Prediction->GetFixedTickState().PendingFrame <= 0) return false;
			FMoverDefaultSyncState* Default = Sync.SyncStateCollection.FindMutableDataByType<FMoverDefaultSyncState>();
			if (!Default) return false;
			Offset = Character->GetActorRightVector().GetSafeNormal2D() * 50.0;
			Default->SetTransforms_WorldSpace(Default->GetLocation_WorldSpace() + Offset, Default->GetOrientation_WorldSpace(),
				Default->GetVelocity_WorldSpace(), Default->GetAngularVelocityDegrees_WorldSpace(), Default->GetMovementBase(), Default->GetMovementBaseBoneName());
			if (!Liaison->WritePendingSyncState(Sync)) return false;
			// Kinematic simulation captures this same component transform into its next pending sync state.
			Mover(Character)->GetUpdatedComponent()->SetWorldLocation(Default->GetLocation_WorldSpace(), false, nullptr, ETeleportType::TeleportPhysics);
			bInjected = true;
			BeforeHandle = FWorldDelegates::OnWorldTickStart.AddRaw(this, &FScopedDeadCorrection::BeforeDispatch);
			AfterHandle = FWorldDelegates::OnWorldPreActorTick.AddRaw(this, &FScopedDeadCorrection::AfterDispatch);
			return true;
		}
		FVector TestOffset(UWorld* World) const { return bInjected && !bObserved && Owner.IsValid() && Owner->GetWorld() == World ? Offset : FVector::ZeroVector; }
		bool Observed() const { return bObserved; }
		bool KeptTerminalState() const { return bObserved && bTerminal && bSameSimulationFrame; }
		void Report() const
		{
			UE_LOG(LogTemp, Display, TEXT("RpgMoverDeathCorrection injected=%d observed=%d sameFrame=%d terminal=%d frame=%d time=%.0f delta=%s"),
				bInjected, bObserved, bSameSimulationFrame, bTerminal, BeforeFrame, BeforeTimeMs, *CorrectionDelta.ToCompactString());
			UE_LOG(LogTemp, Display, TEXT("RpgMoverDeathCorrection clock local=%d->%d offset=%d->%d step=%d->%d"),
				BeforeClock.LocalPendingFrame, AfterClock.LocalPendingFrame, BeforeClock.ServerOffset, AfterClock.ServerOffset,
				BeforeClock.StepMs, AfterClock.StepMs);
		}
		void Stop()
		{
			FWorldDelegates::OnWorldTickStart.Remove(BeforeHandle); FWorldDelegates::OnWorldPreActorTick.Remove(AfterHandle);
			BeforeHandle.Reset(); AfterHandle.Reset();
			Before = FMoverSyncState();
			bBeforeValid = false;
		}
	private:
		void BeforeDispatch(UWorld* World, ELevelTick, float)
		{
			if (bObserved || !Owner.IsValid() || Owner->GetWorld() != World || !Liaison.IsValid()) return;
			bBeforeValid = Liaison->ReadPendingSyncState(Before);
			BeforeClock = RpgMoverPredictionTests::FFixedPredictionHeadSnapshot::Capture(World, Liaison.Get());
			BeforeFrame = BeforeClock.ServerFrame; BeforeTimeMs = BeforeClock.SimulationTimeMs;
		}
		void AfterDispatch(UWorld* World, ELevelTick, float)
		{
			if (bObserved || !bBeforeValid || !Owner.IsValid() || Owner->GetWorld() != World || !Liaison.IsValid()) return;
			FMoverSyncState After;
			if (!Liaison->ReadPendingSyncState(After)) return;
			const FMoverDefaultSyncState* A = Before.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
			const FMoverDefaultSyncState* B = After.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
			if (!A || !B) return;
			const FVector Delta = B->GetLocation_WorldSpace() - A->GetLocation_WorldSpace();
			if (FVector::DotProduct(Delta, Offset.GetSafeNormal()) > -25.0) return;
			bObserved = true; CorrectionDelta = Delta;
			AfterClock = RpgMoverPredictionTests::FFixedPredictionHeadSnapshot::Capture(World, Liaison.Get());
			bSameSimulationFrame = BeforeClock.IsSameLocalHead(AfterClock);
			bTerminal = After.MovementMode == URpgDeadMovementMode::ModeName && B->GetVelocity_WorldSpace().IsNearlyZero(1.0)
				&& B->GetAngularVelocityDegrees_WorldSpace().IsNearlyZero(1.0) && B->GetIntent_WorldSpace().IsNearlyZero()
				&& !B->GetMovementBase() && !After.LayeredMoves.FindActiveMove<FRpgMoverAbilityRootMotion>();
			Report();
		}
		TWeakObjectPtr<APawn> Owner;
		TWeakObjectPtr<UMoverNetworkPredictionLiaisonComponent> Liaison;
		FDelegateHandle BeforeHandle, AfterHandle;
		FMoverSyncState Before;
		RpgMoverPredictionTests::FFixedPredictionHeadSnapshot BeforeClock, AfterClock;
		FVector Offset = FVector::ZeroVector, CorrectionDelta = FVector::ZeroVector;
		int32 BeforeFrame = INDEX_NONE;
		double BeforeTimeMs = 0.0;
		bool bInjected = false, bBeforeValid = false, bObserved = false, bSameSimulationFrame = false, bTerminal = false;
	};
	struct FPeer
	{
		TWeakObjectPtr<ARpgPlayerState> PersistentPlayer;
		TWeakObjectPtr<URpgAbilitySystemComponent> PersistentASC;
		TWeakObjectPtr<const URpgHealthSet> HealthSet;
		TWeakObjectPtr<APawn> OldPawn;
		TWeakObjectPtr<USkeletalMeshComponent> OldFollower;
		TArray<TWeakObjectPtr<AActor>> OldEquipmentActors;
		FDelegateHandle HealthHandle, EndHandle;
		TStrongObjectPtr<URpgMoverRollbackTestObserver> RollbackObserver;
		FMoverSyncState BeforeDeathDispatch;
		RpgMoverPredictionTests::FFixedPredictionHeadSnapshot BeforeDeathClock;
		FVector BeforeDeathActorLocation = FVector::ZeroVector, BeforeDeathArtificialOffset = FVector::ZeroVector;
		FVector DeathLocation = FVector::ZeroVector, RespawnLocation = FVector::ZeroVector;
		FVector FirstTerminalLocation = FVector::ZeroVector;
		TWeakObjectPtr<APawn> ObservedRespawnPawn;
		FVector FirstRespawnActor = FVector::ZeroVector, FirstRespawnSync = FVector::ZeroVector;
		FName LastRespawnMode;
		float RespawnObservedSeconds = 0.0f, RespawnFrameStallSeconds = 0.0f, LastRespawnStallReportSeconds = -1.0f;
		int32 RespawnObservedTicks = 0, LastRespawnFrame = INDEX_NONE, RespawnTransitionReports = 0, RespawnStallReports = 0;
		bool bFirstRespawnSyncObserved = false;
		FVector MaximumDeathDriftLocation = FVector::ZeroVector, MaximumDeathDriftSyncLocation = FVector::ZeroVector;
		float MaximumDeathDriftSeconds = 0.0f;
		float ProxyDeathConvergenceSeconds = 0.0f, ProxyDeathTargetError = -1.0f;
		float OwnerDeathConvergenceSeconds = 0.0f, OwnerDeathTargetError = -1.0f;
		float StartingHealth = 0.0f, DamagedHealth = 0.0f, DeadSeconds = 0.0f, MaximumDeathDrift = 0.0f;
		int32 AttackCancellations = 0, BlockCancellations = 0, RealDamageEvents = 0;
		int32 BeforeDeathRollbacks = 0, OwnerDeathAnchorAdjustments = 0;
		bool bMeshHitContext = false, bObservedDeath = false, bSawTerminalSync = false, bInvalidDeadState = false, bSawRespawnMontage = false, bCapturedRespawn = false;
		bool bProxyDeathConverged = false;
		bool bOwnerDeathConverged = false, bBeforeDeathDispatchValid = false;
	};
	/** Reads real ASC events and naturally simulated states; it never advances death or respawn itself. */
	class FScopedObservations final
	{
	public:
		~FScopedObservations() { Stop(); }
		TFunction<FVector(UWorld*)> TestPredictionOffset;
		void Start(int32 Id)
		{
			Subject = Id;
			BeforeDispatchHandle = FWorldDelegates::OnWorldTickStart.AddRaw(this, &FScopedObservations::BeforeDispatch);
			AfterDispatchHandle = FWorldDelegates::OnWorldPreActorTick.AddRaw(this, &FScopedObservations::AfterDispatch);
			TickHandle = FWorldDelegates::OnWorldTickEnd.AddRaw(this, &FScopedObservations::Tick);
		}
		void Add(UWorld* World)
		{
			if (Peers.Contains(World)) return;
			ARpgPlayerState* State = Player(World, Subject);
			APawn* Character = State ? State->GetPawn() : nullptr;
			if (!State || !State->GetRpgAbilitySystemComponent() || !State->GetRpgAbilitySystemComponent()->GetSet<URpgHealthSet>()) return;
			FPeer& Peer = Peers.Add(World);
			Peer.PersistentPlayer = State; Peer.PersistentASC = State->GetRpgAbilitySystemComponent(); Peer.OldPawn = Character;
			Peer.HealthSet = Peer.PersistentASC->GetSet<URpgHealthSet>();
			if (Character)
			{
				Peer.StartingHealth = Health(Character)->GetHealth();
				Peer.OldFollower = Retarget(Character)->GetRetargetMesh();
				for (ERpgEquipmentSlot Slot : { ERpgEquipmentSlot::MainHand, ERpgEquipmentSlot::OffHand })
					for (AActor* Actor : Equipment(Character)->GetEquipmentInstanceInSlot(Slot)->GetSpawnedActors()) Peer.OldEquipmentActors.Add(Actor);
				if (Character->GetLocalRole() == ROLE_AutonomousProxy && Mover(Character))
				{
					Peer.RollbackObserver.Reset(NewObject<URpgMoverRollbackTestObserver>());
					Mover(Character)->OnPostSimulationRollback.AddDynamic(Peer.RollbackObserver.Get(), &URpgMoverRollbackTestObserver::ObserveRollback);
				}
			}
			Peer.HealthHandle = Peer.HealthSet->OnHealthChanged.AddLambda([this, World](AActor*, AActor*, const FGameplayEffectSpec* Spec, float, float Before, float After)
			{
				FPeer& Record = Peers.FindChecked(World);
				if (After >= Before) return;
				Record.DamagedHealth = After;
				if (Spec)
				{
					++Record.RealDamageEvents;
					const FHitResult* Hit = Spec->GetContext().GetHitResult();
					Record.bMeshHitContext |= Hit && Hit->GetComponent() == Mesh(Record.OldPawn.Get()) && !Hit->BoneName.IsNone();
				}
			});
			Peer.EndHandle = Peer.PersistentASC->OnAbilityEnded.AddLambda([this, World](const FAbilityEndedData& Data)
			{
				if (!Data.bWasCancelled || !Data.AbilityThatEnded) return;
				FPeer& Record = Peers.FindChecked(World);
				Record.AttackCancellations += Data.AbilityThatEnded->IsA<URpgGameplayAbility_BasicWeaponAttack>() ? 1 : 0;
				Record.BlockCancellations += Data.AbilityThatEnded->IsA<URpgGameplayAbility_Block>() ? 1 : 0;
			});
		}
		const FPeer& Get(UWorld* World) const { return Peers.FindChecked(World); }
		bool Has(UWorld* World) const { return Peers.Contains(World); }
		void Report() const
		{
			const FPeer* Authority = FindAuthorityPeer();
			for (const auto& Entry : Peers)
			{
				const FPeer& Peer = Entry.Value;
				UE_LOG(LogTemp, Display, TEXT("RpgMoverLifecycle world=%s damageEvents=%d meshHit=%d health=%.2f deathSeen=%d deadSeconds=%.3f terminalSync=%d deathDrift=%.3f invalidDead=%d attackCancelled=%d blockCancelled=%d oldPawn=%s respawnMontage=%d"),
					*GetPathNameSafe(Entry.Key.Get()), Peer.RealDamageEvents, Peer.bMeshHitContext, Peer.DamagedHealth, Peer.bObservedDeath, Peer.DeadSeconds,
					Peer.bSawTerminalSync, Peer.MaximumDeathDrift, Peer.bInvalidDeadState, Peer.AttackCancellations, Peer.BlockCancellations, *GetNameSafe(Peer.OldPawn.Get()), Peer.bSawRespawnMontage);
				UE_LOG(LogTemp, Display, TEXT("RpgMoverLifecycle death position world=%s firstTerminal=%s anchor=%s authorityDeath=%s proxyConverged=%d convergenceSeconds=%.4f targetError=%.4f maxDriftActor=%s maxDriftSync=%s maxDriftSeconds=%.4f"),
					*GetPathNameSafe(Entry.Key.Get()), *Peer.FirstTerminalLocation.ToCompactString(), *Peer.DeathLocation.ToCompactString(),
					Authority && Authority->bObservedDeath ? *Authority->DeathLocation.ToCompactString() : TEXT("unobserved"),
					Peer.bProxyDeathConverged, Peer.ProxyDeathConvergenceSeconds, Peer.ProxyDeathTargetError,
						*Peer.MaximumDeathDriftLocation.ToCompactString(), *Peer.MaximumDeathDriftSyncLocation.ToCompactString(), Peer.MaximumDeathDriftSeconds);
				UE_LOG(LogTemp, Display, TEXT("RpgMoverLifecycle owner death world=%s converged=%d convergenceSeconds=%.4f targetError=%.4f anchorAdjustments=%d"),
					*GetPathNameSafe(Entry.Key.Get()), Peer.bOwnerDeathConverged, Peer.OwnerDeathConvergenceSeconds,
					Peer.OwnerDeathTargetError, Peer.OwnerDeathAnchorAdjustments);
			}
		}
		void Stop()
		{
			FWorldDelegates::OnWorldTickEnd.Remove(TickHandle); TickHandle.Reset();
			FWorldDelegates::OnWorldTickStart.Remove(BeforeDispatchHandle); BeforeDispatchHandle.Reset();
			FWorldDelegates::OnWorldPreActorTick.Remove(AfterDispatchHandle); AfterDispatchHandle.Reset();
			for (auto& Entry : Peers)
			{
				if (Entry.Value.HealthSet.IsValid()) Entry.Value.HealthSet->OnHealthChanged.Remove(Entry.Value.HealthHandle);
				if (Entry.Value.PersistentASC.IsValid()) Entry.Value.PersistentASC->OnAbilityEnded.Remove(Entry.Value.EndHandle);
				if (Entry.Value.OldPawn.IsValid() && Mover(Entry.Value.OldPawn.Get()) && Entry.Value.RollbackObserver.IsValid())
					Mover(Entry.Value.OldPawn.Get())->OnPostSimulationRollback.RemoveDynamic(Entry.Value.RollbackObserver.Get(), &URpgMoverRollbackTestObserver::ObserveRollback);
				Entry.Value.RollbackObserver.Reset();
				Entry.Value.BeforeDeathDispatch = FMoverSyncState();
				Entry.Value.bBeforeDeathDispatchValid = false;
			}
		}
	private:
		void ObserveRespawn(UWorld* World, FPeer& Peer, APawn* Current, float DeltaSeconds)
		{
			URpgCharacterMoverComponent* Movement = Mover(Current);
			const FMoverDefaultSyncState* Sync = Movement ? Movement->GetSyncState().SyncStateCollection.FindDataByType<FMoverDefaultSyncState>() : nullptr;
			const FCharacterDefaultInputs* Inputs = Movement ? Movement->GetLastInputCmd().InputCollection.FindDataByType<FCharacterDefaultInputs>() : nullptr;
			const int32 Frame = Movement ? Movement->GetLastTimeStep().ServerFrame : INDEX_NONE;
			const FName Mode = Movement ? Movement->GetSyncState().MovementMode : NAME_None;
			const bool bFirst = !Peer.ObservedRespawnPawn.IsValid();
			if (bFirst)
			{
				Peer.ObservedRespawnPawn = Current;
				Peer.FirstRespawnActor = Current->GetActorLocation();
			}
			if (Sync && !Peer.bFirstRespawnSyncObserved)
			{
				Peer.FirstRespawnSync = Sync->GetLocation_WorldSpace();
				Peer.bFirstRespawnSyncObserved = true;
			}
			++Peer.RespawnObservedTicks;
			Peer.RespawnObservedSeconds += DeltaSeconds;
			Peer.RespawnFrameStallSeconds = Frame == Peer.LastRespawnFrame ? Peer.RespawnFrameStallSeconds + DeltaSeconds : 0.0f;
			const int32 PreviousFrame = Peer.LastRespawnFrame;
			Peer.LastRespawnFrame = Frame;
			const bool bTransition = Mode != Peer.LastRespawnMode && Peer.RespawnTransitionReports < 4;
			Peer.LastRespawnMode = Mode;
			const bool bInputWithoutMovement = Inputs && !Inputs->GetMoveInput().IsNearlyZero()
				&& Sync && Sync->GetVelocity_WorldSpace().IsNearlyZero(1.0);
			const bool bStall = Peer.RespawnObservedSeconds > 0.5f && (Peer.RespawnFrameStallSeconds > 0.25f || bInputWithoutMovement)
				&& Peer.RespawnStallReports < 3 && Peer.RespawnObservedSeconds - Peer.LastRespawnStallReportSeconds >= 1.0f;
			if (Peer.RespawnObservedTicks > 6 && !bTransition && !bStall) return;
			if (bTransition) ++Peer.RespawnTransitionReports;
			if (bStall) { ++Peer.RespawnStallReports; Peer.LastRespawnStallReportSeconds = Peer.RespawnObservedSeconds; }
			UMoverNetworkPredictionLiaisonComponent* Liaison = Current->FindComponentByClass<UMoverNetworkPredictionLiaisonComponent>();
			const auto Clock = RpgMoverPredictionTests::FFixedPredictionHeadSnapshot::Capture(World, Liaison);
			FMoverSyncState Pending;
			const bool bPending = Liaison && Liaison->ReadPendingSyncState(Pending);
			const FMoverDefaultSyncState* PendingDefault = bPending ? Pending.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>() : nullptr;
			const APlayerController* PC = Cast<APlayerController>(Current->GetController());
			const URpgPawnExtensionComponent* Extension = URpgPawnExtensionComponent::FindPawnExtensionComponent(Current);
			const URpgPawnGameplayComponent* Gameplay = URpgPawnGameplayComponent::FindPawnGameplayComponent(Current);
			// TickEnd is the earliest per-world observation used here, after SCS components exist.
			// These are first observed transforms, not a claim to sample the pre-construction spawn transform.
			UE_LOG(LogTemp, Display, TEXT("RpgMoverLifecycle respawn tick world=%s pawn=%s role=%d tick=%d age=%.4f first=%d stall=%d actor=%s sync=%s firstActor=%s firstSync=%s mode=%s velocity=%s frame=%d previousFrame=%d noFrameProgress=%.4f npFrame=%d npTimeMs=%.3f pendingFrame=%d pendingMode=%s pendingLocation=%s"),
				*World->GetPathName(), *Current->GetPathName(), static_cast<int32>(Current->GetLocalRole()), Peer.RespawnObservedTicks,
				Peer.RespawnObservedSeconds, bFirst, bStall, *Current->GetActorLocation().ToCompactString(),
				Sync ? *Sync->GetLocation_WorldSpace().ToCompactString() : TEXT("missing"), *Peer.FirstRespawnActor.ToCompactString(),
				Peer.bFirstRespawnSyncObserved ? *Peer.FirstRespawnSync.ToCompactString() : TEXT("missing"), *Mode.ToString(),
				Sync ? *Sync->GetVelocity_WorldSpace().ToCompactString() : TEXT("missing"), Frame, PreviousFrame, Peer.RespawnFrameStallSeconds,
				Clock.ServerFrame, Clock.SimulationTimeMs, Clock.LocalPendingFrame, bPending ? *Pending.MovementMode.ToString() : TEXT("missing"),
				PendingDefault ? *PendingDefault->GetLocation_WorldSpace().ToCompactString() : TEXT("missing"));
			UE_LOG(LogTemp, Display, TEXT("RpgMoverLifecycle respawn readiness world=%s begunPlay=%d extensionReady=%d gameplayReady=%d inputReady=%d asc=%s avatar=%s local=%d ignoreMove=%d rawLeftY=%.3f moveInput=%s inputType=%d suggestedMode=%s movementActive=%d movementTick=%d inputProducer=%s inputProducers=%d spawnCollisionMethod=%d"),
				*World->GetPathName(), Current->HasActorBegunPlay(), Extension && Extension->HasReachedInitState(Tag(TEXT("InitState.GameplayReady"))),
				Gameplay && Gameplay->HasReachedInitState(Tag(TEXT("InitState.GameplayReady"))), Gameplay && Gameplay->IsReadyToBindInputs(),
				*GetPathNameSafe(ASC(Current)), *GetPathNameSafe(ASC(Current) ? ASC(Current)->GetAvatarActor() : nullptr),
				Current->IsLocallyControlled(), PC && PC->IsMoveInputIgnored(), PC ? PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftY) : 0.0f,
				Inputs ? *Inputs->GetMoveInput().ToCompactString() : TEXT("missing"), Inputs ? static_cast<int32>(Inputs->GetMoveInputType()) : -1,
				Inputs ? *Inputs->SuggestedMovementMode.ToString() : TEXT("missing"), Movement && Movement->IsActive(), Movement && Movement->IsComponentTickEnabled(),
				*GetPathNameSafe(Movement ? Movement->InputProducer.Get() : nullptr), Movement ? Movement->InputProducers.Num() : 0,
				static_cast<int32>(Current->SpawnCollisionHandlingMethod));
			const UCapsuleComponent* Capsule = Movement ? Cast<UCapsuleComponent>(Movement->GetUpdatedComponent()) : nullptr;
			FHitResult CachedFloor;
			const bool bCachedFloor = Movement && Movement->TryGetFloorCheckHitResult(CachedFloor);
			FHitResult Floor;
			TArray<FOverlapResult> Overlaps;
			if (Capsule)
			{
				const FCollisionQueryParams Query(SCENE_QUERY_STAT(RpgMoverLifecycleRespawnTick), false, Current);
				const FCollisionResponseParams Response(Capsule->GetCollisionResponseToChannels());
				World->OverlapMultiByChannel(Overlaps, Capsule->GetComponentLocation(), Capsule->GetComponentQuat(),
					Capsule->GetCollisionObjectType(), FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()), Query, Response);
				World->LineTraceSingleByChannel(Floor, Capsule->GetComponentLocation(), Capsule->GetComponentLocation() - FVector(0, 0, 500),
					Capsule->GetCollisionObjectType(), Query, Response);
			}
			UE_LOG(LogTemp, Display, TEXT("RpgMoverLifecycle respawn contacts world=%s root=%s updated=%s collision=%d registered=%d physics=%d radius=%.3f halfHeight=%.3f overlaps=%d cachedFloor=%d cachedBlocking=%d cachedPenetration=%d cachedActor=%s cachedNormal=%s traceFloor=%d tracePenetration=%d traceDepth=%.3f traceActor=%s traceDistance=%.3f traceNormal=%s"),
				*World->GetPathName(), *GetPathNameSafe(Current->GetRootComponent()), *GetPathNameSafe(Capsule),
				Capsule ? static_cast<int32>(Capsule->GetCollisionEnabled()) : -1, Capsule && Capsule->IsRegistered(), Capsule && Capsule->IsSimulatingPhysics(),
				Capsule ? Capsule->GetScaledCapsuleRadius() : 0.0f, Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 0.0f, Overlaps.Num(),
				bCachedFloor, CachedFloor.bBlockingHit, CachedFloor.bStartPenetrating, *GetPathNameSafe(CachedFloor.GetActor()), *CachedFloor.ImpactNormal.ToCompactString(),
				Floor.bBlockingHit, Floor.bStartPenetrating, Floor.PenetrationDepth, *GetPathNameSafe(Floor.GetActor()), Floor.Distance, *Floor.ImpactNormal.ToCompactString());
			for (int32 Index = 0; Index < FMath::Min(Overlaps.Num(), 8); ++Index)
			{
				const FOverlapResult& Overlap = Overlaps[Index];
				UE_LOG(LogTemp, Display, TEXT("RpgMoverLifecycle respawn tick overlap world=%s blocking=%d actor=%s component=%s"),
					*World->GetPathName(), Overlap.bBlockingHit, *GetPathNameSafe(Overlap.GetActor()), *GetPathNameSafe(Overlap.GetComponent()));
			}
		}
		static bool IsTerminal(const FMoverSyncState& Sync)
		{
			const FMoverDefaultSyncState* Default = Sync.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
			return Sync.MovementMode == URpgDeadMovementMode::ModeName && Default
				&& Default->GetVelocity_WorldSpace().IsNearlyZero(1.0) && Default->GetAngularVelocityDegrees_WorldSpace().IsNearlyZero(1.0)
				&& Default->GetIntent_WorldSpace().IsNearlyZero() && !Default->GetMovementBase()
				&& !Sync.LayeredMoves.FindActiveMove<FRpgMoverAbilityRootMotion>();
		}
		static void RecordDrift(FPeer& Peer, const FVector& ObservedLocation, const FVector& SyncLocation)
		{
			const float Drift = static_cast<float>(FVector::Dist(Peer.DeathLocation, ObservedLocation));
			if (Drift > Peer.MaximumDeathDrift)
			{
				Peer.MaximumDeathDrift = Drift;
				Peer.MaximumDeathDriftLocation = ObservedLocation;
				Peer.MaximumDeathDriftSyncLocation = SyncLocation;
				Peer.MaximumDeathDriftSeconds = Peer.DeadSeconds;
			}
		}
		void BeforeDispatch(UWorld* World, ELevelTick, float)
		{
			FPeer* Peer = ActiveWorld(World) ? Peers.Find(World) : nullptr;
			if (!Peer || !Peer->bObservedDeath || !Peer->bSawTerminalSync || Peer->bOwnerDeathConverged || !Peer->RollbackObserver.IsValid()) return;
			APawn* Old = Peer->OldPawn.Get();
			UMoverNetworkPredictionLiaisonComponent* Liaison = Old ? Old->FindComponentByClass<UMoverNetworkPredictionLiaisonComponent>() : nullptr;
			Peer->bBeforeDeathDispatchValid = Liaison && Liaison->ReadPendingSyncState(Peer->BeforeDeathDispatch);
			Peer->BeforeDeathClock = RpgMoverPredictionTests::FFixedPredictionHeadSnapshot::Capture(World, Liaison);
			Peer->BeforeDeathRollbacks = Peer->RollbackObserver->Count;
			Peer->BeforeDeathActorLocation = Old ? Old->GetActorLocation() : FVector::ZeroVector;
			Peer->BeforeDeathArtificialOffset = TestPredictionOffset ? TestPredictionOffset(World) : FVector::ZeroVector;
		}
		void AfterDispatch(UWorld* World, ELevelTick, float DeltaSeconds)
		{
			FPeer* Peer = ActiveWorld(World) ? Peers.Find(World) : nullptr;
			if (!Peer || !Peer->bBeforeDeathDispatchValid || Peer->bOwnerDeathConverged || !Peer->RollbackObserver.IsValid()
				|| Peer->RollbackObserver->Count <= Peer->BeforeDeathRollbacks) return;
			APawn* Old = Peer->OldPawn.Get();
			UMoverNetworkPredictionLiaisonComponent* Liaison = Old ? Old->FindComponentByClass<UMoverNetworkPredictionLiaisonComponent>() : nullptr;
			const FPeer* Authority = FindAuthorityPeer();
			FMoverSyncState After;
			const float ConvergenceSeconds = Peer->DeadSeconds + DeltaSeconds;
			if (!Liaison || !Liaison->ReadPendingSyncState(After) || !Authority || !Authority->bSawTerminalSync
				|| ConvergenceSeconds > 0.75f || !IsTerminal(Peer->BeforeDeathDispatch) || !IsTerminal(After)
				|| !Peer->BeforeDeathClock.IsSameLocalHead(RpgMoverPredictionTests::FFixedPredictionHeadSnapshot::Capture(World, Liaison))) return;
			const FMoverDefaultSyncState* BeforeDefault = Peer->BeforeDeathDispatch.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
			const FMoverDefaultSyncState* AfterDefault = After.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
			if (FVector::DistSquared(AfterDefault->GetLocation_WorldSpace(), Authority->DeathLocation) > 1.0
				|| FVector::DistSquared(Old->GetActorLocation(), AfterDefault->GetLocation_WorldSpace()) > 1.0) return;
			// The owner initially stops at its predicted position. Only the first proven NP rollback to
			// the authority's already stationary Dead state may replace that anchor; ordinary movement
			// before or after this correction remains subject to the unchanged strict drift bound.
			RecordDrift(*Peer, Peer->BeforeDeathActorLocation - Peer->BeforeDeathArtificialOffset, BeforeDefault->GetLocation_WorldSpace());
			Peer->DeathLocation = Authority->DeathLocation;
			Peer->bOwnerDeathConverged = true;
			Peer->OwnerDeathConvergenceSeconds = ConvergenceSeconds;
			Peer->OwnerDeathAnchorAdjustments = 1;
			UE_LOG(LogTemp, Display, TEXT("RpgMoverLifecycle owner death reconciled world=%s seconds=%.4f localFrame=%d rollbacks=%d->%d from=%s authority=%s"),
				*World->GetPathName(), ConvergenceSeconds, Peer->BeforeDeathClock.LocalPendingFrame,
				Peer->BeforeDeathRollbacks, Peer->RollbackObserver->Count,
				*BeforeDefault->GetLocation_WorldSpace().ToCompactString(), *Authority->DeathLocation.ToCompactString());
			Peer->BeforeDeathDispatch = FMoverSyncState();
			Peer->bBeforeDeathDispatchValid = false;
		}
		const FPeer* FindAuthorityPeer() const
		{
			for (const auto& Entry : Peers)
				if (Entry.Key.IsValid() && Entry.Key->GetNetMode() != NM_Client) return &Entry.Value;
			return nullptr;
		}
		void Tick(UWorld* World, ELevelTick, float DeltaSeconds)
		{
			if (!ActiveWorld(World)) return;
			FPeer* Peer = Peers.Find(World);
			if (!Peer) return;
			APawn* Old = Peer->OldPawn.Get();
			if (Old && Health(Old) && Health(Old)->IsDeadOrDying() && Mover(Old))
			{
				if (!Peer->bObservedDeath) { Peer->DeathLocation = Old->GetActorLocation(); Peer->bObservedDeath = true; }
				Peer->DeadSeconds += DeltaSeconds;
				const FMoverSyncState& Sync = Mover(Old)->GetSyncState();
				const FMoverDefaultSyncState* Default = Sync.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
				const bool bTerminal = IsTerminal(Sync);
				const bool bProxy = Old->GetLocalRole() == ROLE_SimulatedProxy;
				if (bTerminal && !Peer->bSawTerminalSync)
				{
					Peer->bSawTerminalSync = true;
					Peer->FirstTerminalLocation = Old->GetActorLocation();
				}
				if (bProxy)
				{
					const FPeer* Authority = FindAuthorityPeer();
					if (Authority && Authority->bObservedDeath && Authority->bSawTerminalSync)
					{
						Peer->ProxyDeathTargetError = static_cast<float>(FVector::Dist(Old->GetActorLocation(), Authority->DeathLocation));
						// Mode comes from the newer NP sample, but position interpolates independently of
						// velocity/intent: even two zero-velocity samples can still have different positions.
						// Require arrival at the fixed authoritative death position within the existing budget.
						if (!Peer->bProxyDeathConverged && bTerminal && Peer->DeadSeconds <= 0.75f && Peer->ProxyDeathTargetError <= 1.0f)
						{
							Peer->DeathLocation = Authority->DeathLocation;
							Peer->bProxyDeathConverged = true;
							Peer->ProxyDeathConvergenceSeconds = Peer->DeadSeconds;
						}
					}
					if (!Peer->bProxyDeathConverged && Peer->DeadSeconds > 0.75f) Peer->bInvalidDeadState = true;
				}
				else if (Old->GetLocalRole() == ROLE_AutonomousProxy)
				{
					const FPeer* Authority = FindAuthorityPeer();
					if (Authority && Authority->bSawTerminalSync)
					{
						Peer->OwnerDeathTargetError = static_cast<float>(FVector::Dist(Old->GetActorLocation(), Authority->DeathLocation));
						// Already matching the server needs no anchor change; a differing predicted anchor
						// can converge only through the explicitly observed rollback above.
						if (!Peer->bOwnerDeathConverged && bTerminal && Peer->DeadSeconds <= 0.75f
							&& FVector::DistSquared(Peer->DeathLocation, Authority->DeathLocation) <= 1.0 && Peer->OwnerDeathTargetError <= 1.0f)
						{
							Peer->bOwnerDeathConverged = true;
							Peer->OwnerDeathConvergenceSeconds = Peer->DeadSeconds;
							Peer->BeforeDeathDispatch = FMoverSyncState();
							Peer->bBeforeDeathDispatchValid = false;
						}
					}
					if (!Peer->bOwnerDeathConverged && Peer->DeadSeconds > 0.75f) Peer->bInvalidDeadState = true;
				}
				// Terminal/ability checks start at the first complete terminal sample, independently of convergence.
				if (bProxy ? Peer->bSawTerminalSync : Peer->DeadSeconds > 0.05f)
				{
					// Owners retain their predicted anchor until a proven authoritative rollback. Only buffered proxies wait for arrival.
					if (!bProxy || Peer->bProxyDeathConverged)
					{
						// Exclude only the known, intentionally injected owner prediction error.
						const FVector ArtificialOffset = TestPredictionOffset ? TestPredictionOffset(World) : FVector::ZeroVector;
						const FVector ObservedLocation = Old->GetActorLocation() - ArtificialOffset;
						RecordDrift(*Peer, ObservedLocation, Default ? Default->GetLocation_WorldSpace() : FVector::ZeroVector);
					}
					const bool bInvalid = !bTerminal;
					if (bInvalid && !Peer->bInvalidDeadState)
						UE_LOG(LogTemp, Display, TEXT("RpgMoverLifecycle invalid death world=%s seconds=%.3f mode=%s velocity=%s angular=%s rootMotion=%d"),
							*World->GetPathName(), Peer->DeadSeconds, *Sync.MovementMode.ToString(),
							Default ? *Default->GetVelocity_WorldSpace().ToCompactString() : TEXT("missing"),
							Default ? *Default->GetAngularVelocityDegrees_WorldSpace().ToCompactString() : TEXT("missing"),
							Sync.LayeredMoves.FindActiveMove<FRpgMoverAbilityRootMotion>() != nullptr);
					Peer->bInvalidDeadState |= bInvalid;
					if (Peer->DeadSeconds > 0.2f && ASC(Old))
					{
						const FGameplayAbilitySpec* Attack = CombatSpec(Old, false);
						const FGameplayAbilitySpec* Block = CombatSpec(Old, true);
						Peer->bInvalidDeadState |= (Attack && Attack->IsActive()) || (Block && Block->IsActive())
							|| ASC(Old)->HasMatchingGameplayTag(Tag(TEXT("State.Blocking")));
					}
				}
			}
			APawn* Current = Pawn(World, Subject);
			if (Current && Current != Old) ObserveRespawn(World, *Peer, Current, DeltaSeconds);
			if (Current && Current != Old && ASC(Current))
			{
				if (!Peer->bCapturedRespawn) { Peer->RespawnLocation = Current->GetActorLocation(); Peer->bCapturedRespawn = true; }
				const FGameplayAbilitySpec* Spec = CombatSpec(Current, false);
				Peer->bSawRespawnMontage |= ASC(Current)->GetCurrentMontage() != nullptr
					&& (Current->GetLocalRole() == ROLE_SimulatedProxy || (Spec && Spec->IsActive()));
			}
		}
		TMap<TWeakObjectPtr<UWorld>, FPeer> Peers;
		FDelegateHandle TickHandle;
		FDelegateHandle BeforeDispatchHandle, AfterDispatchHandle;
		int32 Subject = INDEX_NONE;
	};
	struct FState : FBasePIENetworkComponentState {};
}

NETWORK_TEST_CLASS(GaspMoverLifecyclePIE, "SurvivalRpg.GASP.Mover.Lifecycle")
{
	using FState = RpgGaspMoverLifecycleTests::FState;
	RpgGaspMoverLifecycleTests::FScopedWorld Isolation;
	RpgGaspMoverLifecycleTests::FScopedInput Input;
	RpgGaspMoverLifecycleTests::FScopedProfile Profile;
	RpgGaspMoverLifecycleTests::FScopedObservations Observations;
	RpgGaspMoverLifecycleTests::FScopedDeadCorrection Correction;
	FPIENetworkComponent<FState> Network{TestRunner, TestCommandBuilder, bInitializing};
	FPrimaryAssetId PreviousExperience;
	TWeakObjectPtr<UWorld> AuthorityWorld, OwnerWorld, ObserverWorld;
	int32 SubjectId = INDEX_NONE;
	double RespawnMovementStarted = 0.0;
	bool bReportedRespawnMovement = false;
	bool bConfigured = false, bFollower = false, bBlock = false;
	bool bOccupiedRespawnStart = false;
	RpgGaspMoverLifecycleTests::ERespawnStartScenario RespawnStartScenario = RpgGaspMoverLifecycleTests::ERespawnStartScenario::Unconstrained;
	FTransform OccupiedRespawnTransform = FTransform::Identity;
	int32 RespawnBlockerId = INDEX_NONE;
	int32 RespawnHostBlockerId = INDEX_NONE;

	BEFORE_EACH()
	{
		using namespace RpgGaspMoverLifecycleTests;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
			if (Context.WorldType == EWorldType::PIE && IsValid(Context.World()))
			{
				TestRunner->AddError(TEXT("Mover lifecycle automation refuses to interrupt an existing PIE session."));
				return;
			}
		UClass* GameMode = LoadClass<ARpgGameModeBase>(nullptr, GameModePath);
		ASSERT_THAT(IsNotNull(GameMode));
		if (!GameMode) return;
		// CQTest executes the network setup queued by Before before invoking the test method. Configure
		// the named profile here so even the first listen-host pawn sees it during normal composition.
		bFollower = TestRunner->GetTestContext().EndsWith(TEXT("DeathCancelsHeldBlockAndRespawnRecomposesOptionalFollower"));
		bBlock = bFollower;
		if (bFollower)
		{
			const bool bEnabled = Profile.Enable(LoadObject<URpgPawnData>(nullptr, PawnDataPath));
			ASSERT_THAT(IsTrue(bEnabled));
			if (!bEnabled) return;
		}
		Isolation.Start();
		PreviousExperience = GetDefault<URpgDeveloperSettings>()->ExperienceOverride;
		GetMutableDefault<URpgDeveloperSettings>()->ExperienceOverride = ExperienceId();
		bConfigured = true;
		FNetworkComponentBuilder<FState>().WithClients(1).AsListenServer()
			.WithGameInstanceClass(FSoftClassPath(TEXT("/Game/SurvivalRpg/Core/Game/BP_Rpg_GameInstance.BP_Rpg_GameInstance_C")))
			.WithGameMode(GameMode).Build(Network);
	}
	AFTER_EACH()
	{
		Input.Stop();
		if (TestRunner->HasAnyErrors()) Observations.Report();
		if (TestRunner->HasAnyErrors()) Correction.Report();
		Correction.Stop();
		Observations.Stop();
		Profile.Restore();
		if (bConfigured) GetMutableDefault<URpgDeveloperSettings>()->ExperienceOverride = PreviousExperience;
	}
	TEST_METHOD(WeaponDamageCancelsAttackStopsMovementAndRespawnsForLateJoin) { Queue(false); }
	TEST_METHOD(DeathCancelsHeldBlockAndRespawnRecomposesOptionalFollower) { Queue(true); }
	TEST_METHOD(OccupiedRespawnStartAllowsMovementAndCombat) { Queue(false, RpgGaspMoverLifecycleTests::ERespawnStartScenario::SingleLateJoiner); }
	TEST_METHOD(CrowdedCheckpointAllowsMovementAndCombat) { Queue(false, RpgGaspMoverLifecycleTests::ERespawnStartScenario::HostAndLateJoiner); }

	bool CrowdedCheckpointHasBlockers(UWorld* World) const
	{
		using namespace RpgGaspMoverLifecycleTests;
		APawn* Host = Pawn(World, RespawnHostBlockerId);
		APawn* Joiner = Pawn(World, RespawnBlockerId);
		const UCapsuleComponent* HostCapsule = Host ? Cast<UCapsuleComponent>(Host->GetRootComponent()) : nullptr;
		const UCapsuleComponent* JoinerCapsule = Joiner ? Cast<UCapsuleComponent>(Joiner->GetRootComponent()) : nullptr;
		if (!Host || !Joiner || Host == Joiner || RespawnHostBlockerId == SubjectId || RespawnBlockerId == SubjectId
			|| !Ready(World, Host, false) || !Ready(World, Joiner, false) || !HostCapsule || !JoinerCapsule
			|| !Mover(Host)->IsOnGround() || !Mover(Joiner)->IsOnGround()) return false;
		const double TouchingDistance = HostCapsule->GetScaledCapsuleRadius() + JoinerCapsule->GetScaledCapsuleRadius();
		const double Separation = FVector::Dist2D(Host->GetActorLocation(), Joiner->GetActorLocation());
		// The late joiner must naturally leave the host's capsule yet stay directly beside it.
		// Five centimetres allow the engine's clearance margin without accepting an unrelated spawn.
		if (Separation < TouchingDistance || Separation > TouchingDistance + 5.0
			|| FVector::Dist2D(Host->GetActorLocation(), OccupiedRespawnTransform.GetLocation()) > HostCapsule->GetScaledCapsuleRadius()) return false;
		TArray<FOverlapResult> Overlaps;
		World->OverlapMultiByChannel(Overlaps, OccupiedRespawnTransform.GetLocation(), OccupiedRespawnTransform.GetRotation(),
			HostCapsule->GetCollisionObjectType(), FCollisionShape::MakeCapsule(HostCapsule->GetScaledCapsuleRadius(), HostCapsule->GetScaledCapsuleHalfHeight()),
			FCollisionQueryParams(SCENE_QUERY_STAT(RpgMoverLifecycleCrowdedCheckpoint), false),
			FCollisionResponseParams(HostCapsule->GetCollisionResponseToChannels()));
		return Overlaps.ContainsByPredicate([Host, HostCapsule](const FOverlapResult& Overlap)
			{ return Overlap.bBlockingHit && Overlap.GetActor() == Host && Overlap.GetComponent() == HostCapsule; });
	}
	void ReportCrowdedCheckpoint(UWorld* World, const TCHAR* Stage) const
	{
		using namespace RpgGaspMoverLifecycleTests;
		if (RespawnStartScenario != ERespawnStartScenario::HostAndLateJoiner) return;
		const APawn* Host = Pawn(World, RespawnHostBlockerId);
		const APawn* Joiner = Pawn(World, RespawnBlockerId);
		UE_LOG(LogTemp, Display, TEXT("RpgMoverLifecycle crowded checkpoint stage=%s world=%s checkpoint=%s valid=%d separation2D=%.3f"),
			Stage, *GetPathNameSafe(World), *OccupiedRespawnTransform.GetLocation().ToCompactString(), CrowdedCheckpointHasBlockers(World),
			Host && Joiner ? FVector::Dist2D(Host->GetActorLocation(), Joiner->GetActorLocation()) : -1.0);
		for (const APawn* Blocker : { Host, Joiner })
		{
			const UCapsuleComponent* Capsule = Blocker ? Cast<UCapsuleComponent>(Blocker->GetRootComponent()) : nullptr;
			const URpgCharacterMoverComponent* Movement = Mover(Blocker);
			UE_LOG(LogTemp, Display, TEXT("RpgMoverLifecycle crowded blocker stage=%s host=%d pawn=%s playerId=%d actor=%s checkpointDistance2D=%.3f mode=%s grounded=%d radius=%.3f halfHeight=%.3f collision=%d"),
				Stage, Blocker == Host, *GetPathNameSafe(Blocker), Blocker && Blocker->GetPlayerState() ? Blocker->GetPlayerState()->GetPlayerId() : INDEX_NONE,
				Blocker ? *Blocker->GetActorLocation().ToCompactString() : TEXT("missing"),
				Blocker ? FVector::Dist2D(Blocker->GetActorLocation(), OccupiedRespawnTransform.GetLocation()) : -1.0,
				Movement ? *Movement->GetSyncState().MovementMode.ToString() : TEXT("missing"), Movement && Movement->IsOnGround(),
				Capsule ? Capsule->GetScaledCapsuleRadius() : 0.0f, Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 0.0f,
				Capsule ? static_cast<int32>(Capsule->GetCollisionEnabled()) : -1);
		}
	}

	bool RespawnStartHasBlocker(UWorld* World) const
	{
		using namespace RpgGaspMoverLifecycleTests;
		if (RespawnStartScenario == ERespawnStartScenario::HostAndLateJoiner) return CrowdedCheckpointHasBlockers(World);
		APawn* Blocker = Pawn(World, RespawnBlockerId);
		const UCapsuleComponent* Capsule = Blocker ? Cast<UCapsuleComponent>(Blocker->GetRootComponent()) : nullptr;
		if (!Ready(World, Blocker, false) || !Capsule || !Mover(Blocker)->IsOnGround()
			|| FVector::Dist2D(Blocker->GetActorLocation(), OccupiedRespawnTransform.GetLocation()) > 1.0) return false;
		TArray<FOverlapResult> Overlaps;
		World->OverlapMultiByChannel(Overlaps, OccupiedRespawnTransform.GetLocation(), OccupiedRespawnTransform.GetRotation(),
			Capsule->GetCollisionObjectType(), FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()),
			FCollisionQueryParams(SCENE_QUERY_STAT(RpgMoverLifecycleOccupiedStart), false),
			FCollisionResponseParams(Capsule->GetCollisionResponseToChannels()));
		return Overlaps.ContainsByPredicate([Blocker, Capsule](const FOverlapResult& Overlap)
			{ return Overlap.bBlockingHit && Overlap.GetActor() == Blocker && Overlap.GetComponent() == Capsule; });
	}

	bool Respawned(UWorld* World) const
	{
		using namespace RpgGaspMoverLifecycleTests;
		if (!Observations.Has(World)) return false;
		const FPeer& Peer = Observations.Get(World);
		ARpgPlayerState* State = Player(World, SubjectId);
		APawn* Character = Pawn(World, SubjectId);
		return State && State == Peer.PersistentPlayer.Get() && !State->IsWaitingForRespawn()
			&& State->GetRpgAbilitySystemComponent() == Peer.PersistentASC.Get() && Character && Character != Peer.OldPawn.Get()
			&& Ready(World, Character, bFollower) && !ASC(Character)->HasMatchingGameplayTag(Tag(TEXT("Status.Death")))
			&& !ASC(Character)->HasMatchingGameplayTag(Tag(TEXT("State.Blocking")))
			&& Mover(Character)->GetSyncState().MovementMode != URpgDeadMovementMode::ModeName;
	}
	bool OldObjectsReleased(UWorld* World) const
	{
		using namespace RpgGaspMoverLifecycleTests;
		if (!Observations.Has(World)) return false;
		const FPeer& Peer = Observations.Get(World);
		if (Peer.OldPawn.IsValid() || Peer.OldFollower.IsValid()) return false;
		for (const TWeakObjectPtr<AActor>& Actor : Peer.OldEquipmentActors) if (Actor.IsValid()) return false;
		return true;
	}
	void ReportRespawnMovement() const
	{
		using namespace RpgGaspMoverLifecycleTests;
		for (UWorld* World : { AuthorityWorld.Get(), OwnerWorld.Get(), ObserverWorld.Get() })
		{
			APawn* Character = Pawn(World, SubjectId);
			const URpgCharacterMoverComponent* Movement = Mover(Character);
			const FMoverDefaultSyncState* Sync = Movement ? Movement->GetSyncState().SyncStateCollection.FindDataByType<FMoverDefaultSyncState>() : nullptr;
			const FCharacterDefaultInputs* Inputs = Movement ? Movement->GetLastInputCmd().InputCollection.FindDataByType<FCharacterDefaultInputs>() : nullptr;
			const APlayerController* PC = Character ? Cast<APlayerController>(Character->GetController()) : nullptr;
			const URpgPawnGameplayComponent* Gameplay = URpgPawnGameplayComponent::FindPawnGameplayComponent(Character);
			const FVector Start = Observations.Has(World) ? Observations.Get(World).RespawnLocation : FVector::ZeroVector;
			UE_LOG(LogTemp, Display, TEXT("RpgMoverLifecycle respawn movement world=%s pawn=%s role=%d ready=%d respawned=%d inputReady=%d local=%d ignoreMove=%d rawLeftY=%.3f actor=%s sync=%s start=%s velocity=%s mode=%s moveInput=%s traversal=%d frame=%d"),
				*GetPathNameSafe(World), *GetPathNameSafe(Character), Character ? static_cast<int32>(Character->GetLocalRole()) : -1,
				Ready(World, Character, bFollower), Respawned(World), Gameplay && Gameplay->IsReadyToBindInputs(), PC && PC->IsLocalController(),
				PC && PC->IsMoveInputIgnored(), PC ? PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftY) : 0.0f,
				Character ? *Character->GetActorLocation().ToCompactString() : TEXT("missing"),
				Sync ? *Sync->GetLocation_WorldSpace().ToCompactString() : TEXT("missing"), *Start.ToCompactString(),
				Movement ? *Movement->GetVelocity().ToCompactString() : TEXT("missing"),
				Movement ? *Movement->GetSyncState().MovementMode.ToString() : TEXT("missing"),
				Inputs ? *Inputs->GetMoveInput().ToCompactString() : TEXT("missing"),
				Movement && Movement->HasTraversalLease(), Movement ? Movement->GetLastTimeStep().ServerFrame : INDEX_NONE);
			const UCapsuleComponent* Capsule = Movement ? Cast<UCapsuleComponent>(Movement->GetUpdatedComponent()) : nullptr;
			UE_LOG(LogTemp, Display, TEXT("RpgMoverLifecycle respawn capsule world=%s updated=%s root=%s collision=%d radius=%.3f halfHeight=%.3f simulatePhysics=%d"),
				*GetPathNameSafe(World), *GetPathNameSafe(Movement ? Movement->GetUpdatedComponent() : nullptr),
				*GetPathNameSafe(Character ? Character->GetRootComponent() : nullptr), Capsule ? static_cast<int32>(Capsule->GetCollisionEnabled()) : -1,
				Capsule ? Capsule->GetScaledCapsuleRadius() : 0.0f, Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 0.0f,
				Capsule && Capsule->IsSimulatingPhysics());
			const AGameStateBase* GameState = ActiveWorld(World) ? World->GetGameState() : nullptr;
			if (GameState)
			{
				for (const APlayerState* State : GameState->PlayerArray)
				{
					const APawn* Other = State ? State->GetPawn() : nullptr;
					if (!Other || Other == Character) continue;
					const UCapsuleComponent* OtherCapsule = Cast<UCapsuleComponent>(Other->GetRootComponent());
					UE_LOG(LogTemp, Display, TEXT("RpgMoverLifecycle respawn other pawn world=%s playerId=%d pawn=%s actor=%s distance=%.3f distance2D=%.3f capsule=%s collision=%d radius=%.3f halfHeight=%.3f pawnResponse=%d"),
						*GetPathNameSafe(World), State->GetPlayerId(), *GetPathNameSafe(Other), *Other->GetActorLocation().ToCompactString(),
						Character ? FVector::Distance(Character->GetActorLocation(), Other->GetActorLocation()) : -1.0,
						Character ? FVector::Dist2D(Character->GetActorLocation(), Other->GetActorLocation()) : -1.0,
						*GetPathNameSafe(OtherCapsule), OtherCapsule ? static_cast<int32>(OtherCapsule->GetCollisionEnabled()) : -1,
						OtherCapsule ? OtherCapsule->GetScaledCapsuleRadius() : 0.0f, OtherCapsule ? OtherCapsule->GetScaledCapsuleHalfHeight() : 0.0f,
						OtherCapsule ? static_cast<int32>(OtherCapsule->GetCollisionResponseToChannel(ECC_Pawn)) : -1);
				}
			}
			if (ActiveWorld(World) && Character && Capsule)
			{
				TArray<FOverlapResult> Overlaps;
				World->OverlapMultiByChannel(Overlaps, Capsule->GetComponentLocation(), Capsule->GetComponentQuat(),
					Capsule->GetCollisionObjectType(), FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()),
					FCollisionQueryParams(SCENE_QUERY_STAT(RpgMoverLifecycleRespawn), false, Character),
					FCollisionResponseParams(Capsule->GetCollisionResponseToChannels()));
				for (const FOverlapResult& Overlap : Overlaps)
				{
					UE_LOG(LogTemp, Display, TEXT("RpgMoverLifecycle respawn overlap world=%s blocking=%d actor=%s component=%s"),
						*GetPathNameSafe(World), Overlap.bBlockingHit, *GetPathNameSafe(Overlap.GetActor()), *GetPathNameSafe(Overlap.GetComponent()));
				}
			}
		}
	}
	void VerifyDeath(UWorld* World)
	{
		using namespace RpgGaspMoverLifecycleTests;
		const FPeer& Peer = Observations.Get(World);
		ASSERT_THAT(IsTrue(Isolation.Isolated(World)));
		ASSERT_THAT(IsTrue(Peer.bObservedDeath && Peer.DeadSeconds > 0.25f));
		ASSERT_THAT(IsTrue(Peer.bSawTerminalSync));
		ASSERT_THAT(IsFalse(Peer.bInvalidDeadState));
		ASSERT_THAT(IsTrue(Peer.MaximumDeathDrift < 5.0f));
		if (World == ObserverWorld.Get()) ASSERT_THAT(IsTrue(Peer.bProxyDeathConverged && Peer.ProxyDeathConvergenceSeconds <= 0.75f));
		if (World == OwnerWorld.Get()) ASSERT_THAT(IsTrue(Peer.bOwnerDeathConverged && Peer.OwnerDeathConvergenceSeconds <= 0.75f));
		if (World == AuthorityWorld.Get() || World == OwnerWorld.Get())
			ASSERT_THAT(IsTrue(bBlock ? Peer.BlockCancellations > 0 : Peer.AttackCancellations > 0));
		if (World == AuthorityWorld.Get())
		{
			ASSERT_THAT(IsTrue(Peer.RealDamageEvents >= 2));
			ASSERT_THAT(IsTrue(Peer.bMeshHitContext));
		}
	}
	void DamageThroughMesh(UWorld* World)
	{
		using namespace RpgGaspMoverLifecycleTests;
		APawn* Character = Pawn(World, SubjectId);
		USkeletalMeshComponent* Source = Mesh(Character);
		ASSERT_THAT(IsTrue(Character && Character->HasAuthority() && Source && Source->DoesSocketExist(TEXT("spine_03"))));
		if (!Character || !Source) return;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(MoverLifecycleWeaponSweep), false);
		// Equipment can cover the body. This fixture isolates the incoming body-hit contract, not shield blocking.
		if (World->GetGameState()) for (APlayerState* State : World->GetGameState()->PlayerArray)
		{
			APawn* Other = State ? State->GetPawn() : nullptr;
			if (Other && Other != Character) Query.AddIgnoredActor(Other);
			if (Equipment(Other)) for (ERpgEquipmentSlot Slot : { ERpgEquipmentSlot::MainHand, ERpgEquipmentSlot::OffHand })
				if (const URpgEquipmentInstance* Item = Equipment(Other)->GetEquipmentInstanceInSlot(Slot))
					for (AActor* Actor : Item->GetSpawnedActors()) Query.AddIgnoredActor(Actor);
		}
		const FVector Center = Source->GetSocketLocation(TEXT("spine_03"));
		FHitResult Hit;
		const bool bHit = World->SweepSingleByChannel(Hit, Center - FVector(120, 0, 0), Center + FVector(120, 0, 0),
			FQuat::Identity, Rpg_TraceChannel_Weapon, FCollisionShape::MakeSphere(6.0f), Query);
		UE_LOG(LogTemp, Display, TEXT("RpgMoverLifecycle incoming mesh sweep hit=%d actor=%s component=%s bone=%s center=%s"),
			bHit, *GetNameSafe(Hit.GetActor()), *GetNameSafe(Hit.GetComponent()), *Hit.BoneName.ToString(), *Center.ToCompactString());
		ASSERT_THAT(IsTrue(bHit && Hit.GetActor() == Character && Hit.GetComponent() == Source && !Hit.BoneName.IsNone()));
		if (!bHit || Hit.GetComponent() != Source || Hit.BoneName.IsNone()) return;
		URpgAbilitySystemComponent* AbilitySystem = ASC(Character);
		ASSERT_THAT(IsTrue(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Hit.GetActor()) == AbilitySystem));
		ASSERT_THAT(IsTrue(AbilitySystem == Player(World, SubjectId)->GetRpgAbilitySystemComponent()));
		FGameplayEffectContextHandle Context = AbilitySystem->MakeEffectContext();
		Context.AddHitResult(Hit, true);
		const TSubclassOf<UGameplayEffect> Damage = URpgGameData::Get().DamageGameplayEffect_SetByCaller.LoadSynchronous();
		FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(Damage, 1.0f, Context);
		ASSERT_THAT(IsTrue(Spec.IsValid()));
		if (Spec.IsValid())
		{
			Spec.Data->SetSetByCallerMagnitude(Tag(TEXT("SetByCaller.Damage")), Health(Character)->GetMaxHealth() * 0.1f);
			AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		}
	}
	void Queue(bool bOptionalFollower, RpgGaspMoverLifecycleTests::ERespawnStartScenario StartScenario = RpgGaspMoverLifecycleTests::ERespawnStartScenario::Unconstrained)
	{
		using namespace RpgGaspMoverLifecycleTests;
		if (!bConfigured) return;
		ASSERT_THAT(IsTrue(bFollower == bOptionalFollower));
		RespawnStartScenario = StartScenario;
		bOccupiedRespawnStart = StartScenario != ERespawnStartScenario::Unconstrained;
		Network.UntilClient(TEXT("Owner composes normal healthy Mover, ASC, equipment and optional presentation"), 0, [this](FState& State)
			{ return Ready(State.World, LocalPawn(State.World), bFollower) && Mover(LocalPawn(State.World))->IsOnGround(); }, Timeout())
			.ThenClient(TEXT("Select the real owning player and retain its normal input route"), 0, [this](FState& State)
			{
				OwnerWorld = State.World;
				APawn* Character = LocalPawn(State.World);
				SubjectId = Character->GetPlayerState()->GetPlayerId();
				ASSERT_THAT(IsTrue(Character->GetLocalRole() == ROLE_AutonomousProxy));
				Input.Start(Character);
			})
			.UntilServer(TEXT("Authority and listen host finish normal healthy composition"), [this](FState& State)
				{ return Ready(State.World, Pawn(State.World, SubjectId), bFollower) && Ready(State.World, LocalPawn(State.World), bFollower); }, Timeout())
			.ThenServer(TEXT("Observe the real health and GAS lifecycle on authority and owner"), [this](FState& State)
			{
				AuthorityWorld = State.World;
				Observations.Start(SubjectId); Observations.Add(State.World); Observations.Add(OwnerWorld.Get());
				Observations.TestPredictionOffset = [this](UWorld* World) { return Correction.TestOffset(World); };
			});
		if (bOccupiedRespawnStart)
		{
			Network.ThenServer(TEXT("Pin the selected fixture start as the checkpoint before death snapshots the respawn target"), [this](FState& State)
			{
				APawn* Character = Pawn(State.World, SubjectId);
				APlayerController* PC = Character ? Cast<APlayerController>(Character->GetController()) : nullptr;
				ARpgGameModeBase* Mode = State.World->GetAuthGameMode<ARpgGameModeBase>();
				APawn* Host = LocalPawn(State.World);
				const bool bCrowded = RespawnStartScenario == ERespawnStartScenario::HostAndLateJoiner;
				const bool bFound = bCrowded
					? Ready(State.World, Host, false) && Mover(Host)->IsOnGround() && Isolation.FindStartOccupiedByPawn(State.World, Host, OccupiedRespawnTransform)
					: Isolation.FindFreeSupportedStart(State.World, Character, OccupiedRespawnTransform);
				ASSERT_THAT(IsTrue(PC && Mode && bFound));
				if (!PC || !Mode || !bFound) return;
				if (bCrowded)
				{
					RespawnHostBlockerId = Host->GetPlayerState()->GetPlayerId();
					ASSERT_THAT(IsTrue(RespawnHostBlockerId != SubjectId && Host->IsLocallyControlled() && Host->HasAuthority()));
				}
				Mode->SetPlayerCheckpoint(PC, OccupiedRespawnTransform);
				ASSERT_THAT(IsTrue(Mode->GetPlayerCheckpointTransform(PC).Equals(OccupiedRespawnTransform, 0.01)));
				UE_LOG(LogTemp, Display, TEXT("RpgMoverLifecycle occupied checkpoint pinned before death world=%s subject=%d checkpoint=%s crowded=%d hostBlocker=%s hostLocation=%s"),
					*State.World->GetPathName(), SubjectId, *OccupiedRespawnTransform.GetLocation().ToCompactString(), bCrowded,
					bCrowded ? *GetPathNameSafe(Host) : TEXT("none"), bCrowded ? *Host->GetActorLocation().ToCompactString() : TEXT("none"));
			});
		}
		if (bFollower)
		{
			Network.ThenClientJoins()
				.UntilClient(TEXT("A joined observer composes the living equipped follower before death"), 1, [this](FState& State)
					{ return Ready(State.World, Pawn(State.World, SubjectId), true); }, Timeout())
				.ThenClient(TEXT("Observe death and follower teardown on the simulated proxy too"), 1, [this](FState& State)
					{ ObserverWorld = State.World; Observations.Add(State.World); });
		}
		Network.ThenServer(TEXT("An incoming weapon sweep resolves the physical gameplay mesh and applies real nonlethal GE damage"),
				[this](FState& State) { DamageThroughMesh(State.World); })
			.UntilServer(TEXT("Authority loses health while remaining alive after the physical body hit"), [this](FState& State)
			{
				APawn* Character = Pawn(State.World, SubjectId);
				return Character && Health(Character)->GetHealth() > 0.0f && Health(Character)->GetHealth() < Observations.Get(State.World).StartingHealth;
			}, Timeout())
			.UntilClient(TEXT("Real damage replicates to the owning player's HealthSet"), 0, [this](FState& State)
			{
				APawn* Character = Pawn(State.World, SubjectId);
				return Character && Health(Character)->GetHealth() > 0.0f && Health(Character)->GetHealth() < Observations.Get(State.World).StartingHealth;
			}, Timeout())
			.ThenClient(TEXT("Hold original Mover input while starting a real equipment combat ability"), 0, [this](FState&)
				{ Input.Move(true); if (bBlock) Input.Block(true); else Input.Attack(); })
			.UntilServer(TEXT("Authority is processing the held block or a real active attack hit window"), [this](FState& State)
			{
				APawn* Character = Pawn(State.World, SubjectId);
				const FGameplayAbilitySpec* Spec = CombatSpec(Character, bBlock);
				if (!Spec || !Spec->IsActive()) return false;
				if (bBlock)
				{
					APawn* Proxy = Pawn(ObserverWorld.Get(), SubjectId);
					return ASC(Character)->HasMatchingGameplayTag(Tag(TEXT("State.Blocking"))) && ASC(Proxy)
						&& ASC(Proxy)->HasMatchingGameplayTag(Tag(TEXT("State.Blocking"))) && ASC(Proxy)->GetCurrentMontage();
				}
				const auto* Attack = Cast<URpgGameplayAbility_BasicWeaponAttack>(Spec->GetPrimaryInstance());
				return Attack && Attack->IsAttackWindowOpenForTests();
			}, Timeout())
			.ThenServer(TEXT("Apply lethal damage through the existing authoritative gameplay effect, retaining held input"), [this](FState& State)
				{ Health(Pawn(State.World, SubjectId))->DamageSelfDestruct(false); });
		if (!bFollower && !bOccupiedRespawnStart)
		{
			Network.UntilClient(TEXT("Owner has entered the terminal simulated mode while movement remains held"), 0, [this](FState& State)
				{
					APawn* Character = Pawn(State.World, SubjectId);
					return Character && Health(Character)->IsDeadOrDying() && Observations.Get(State.World).DeadSeconds > 0.25f
						&& Mover(Character)->GetSyncState().MovementMode == URpgDeadMovementMode::ModeName;
				}, Timeout())
				.ThenClient(TEXT("Introduce one incorrect owner prediction without changing authority or held input"), 0, [this](FState& State)
					{ ASSERT_THAT(IsTrue(Correction.Inject(Pawn(State.World, SubjectId)))); })
				.UntilClient(TEXT("A real Fixed NP correction restores the dead pawn before the next simulation frame"), 0,
					[this](FState&) { return Correction.Observed(); }, Timeout())
				.ThenClient(TEXT("Reconciliation retains terminal mode and cannot revive velocity or attack root motion"), 0,
					[this](FState&) { ASSERT_THAT(IsTrue(Correction.KeptTerminalState())); });
		}
		Network.UntilClient(TEXT("Ordinary death finishes and the owner receives the server's respawn wait"), 0, [this](FState& State)
			{
				const ARpgPlayerState* StateOwner = Player(State.World, SubjectId);
				return StateOwner && StateOwner->IsWaitingForRespawn() && !StateOwner->GetPawn();
			}, Timeout())
			.ThenServer(TEXT("Death cancels combat and leaves the authority stationary until normal destruction"), [this](FState& State) { VerifyDeath(State.World); })
			.ThenClient(TEXT("Owning prediction also stops combat and held movement throughout death"), 0, [this](FState& State) { VerifyDeath(State.World); });
		if (bFollower)
		{
			Network.UntilClient(TEXT("The simulated observer receives normal final death"), 1, [this](FState& State)
				{ const ARpgPlayerState* StateOwner = Player(State.World, SubjectId); return StateOwner && StateOwner->IsWaitingForRespawn() && !StateOwner->GetPawn(); }, Timeout())
				.ThenClient(TEXT("Proxy simulation remains terminal through the observed death phase"), 1, [this](FState& State) { VerifyDeath(State.World); });
		}
		else
		{
			if (bOccupiedRespawnStart)
			{
				Network.ThenServer(TEXT("Keep only the stored respawn start for a real late-joining player's normal spawn"), [this](FState& State)
				{
					ARpgPlayerState* Subject = Player(State.World, SubjectId);
					APlayerController* PC = Subject ? Cast<APlayerController>(Subject->GetOwner()) : nullptr;
					ARpgGameModeBase* Mode = State.World->GetAuthGameMode<ARpgGameModeBase>();
					ASSERT_THAT(IsTrue(Subject && Subject->IsWaitingForRespawn() && !Subject->GetPawn() && PC && Mode));
					if (!PC || !Mode) return;
					// NotifyPlayerDeath already captured the explicit checkpoint. Re-evaluating an
					// unconfigured FindPlayerStart here would reserve a different target after death.
					ASSERT_THAT(IsTrue(Mode->GetPlayerCheckpointTransform(PC).Equals(OccupiedRespawnTransform, 0.01)));
					ASSERT_THAT(IsTrue(Isolation.RetainOnlyStart(State.World, OccupiedRespawnTransform)));
					UE_LOG(LogTemp, Display, TEXT("RpgMoverLifecycle occupied start retained world=%s reserved=%s"),
						*GetPathNameSafe(State.World), *OccupiedRespawnTransform.GetLocation().ToCompactString());
				});
			}
			Network.ThenClientJoins()
				.UntilClient(TEXT("Late join while dead receives waiting PlayerState with no living avatar"), 1, [this](FState& State)
				{
					const ARpgPlayerState* StateOwner = Player(State.World, SubjectId);
					return StateOwner && StateOwner->IsWaitingForRespawn() && !StateOwner->GetPawn() && StateOwner->GetRpgAbilitySystemComponent()
						&& StateOwner->GetRpgAbilitySystemComponent()->GetSet<URpgHealthSet>()
						&& StateOwner->GetRpgAbilitySystemComponent()->GetAvatarActor() == nullptr;
				}, Timeout())
				.ThenClient(TEXT("Retain the late observer's persistent ASC before the next pawn spawns"), 1, [this](FState& State)
					{ ObserverWorld = State.World; Observations.Add(State.World); });
			if (bOccupiedRespawnStart)
			{
				Network.UntilClient(TEXT("The late joiner's own pawn naturally settles on the only remaining start"), 1, [this](FState& State)
					{ return Ready(State.World, LocalPawn(State.World), false) && Mover(LocalPawn(State.World))->IsOnGround(); }, Timeout())
					.ThenClient(TEXT("Identify the live player occupying the reserved respawn point"), 1, [this](FState& State)
					{
						RespawnBlockerId = LocalPawn(State.World)->GetPlayerState()->GetPlayerId();
						ASSERT_THAT(IsTrue(RespawnBlockerId != SubjectId));
					})
					.ThenServer(TEXT("Record the naturally separated host and late joiner when testing the crowded checkpoint"), [this](FState& State)
						{ ReportCrowdedCheckpoint(State.World, TEXT("after-late-join")); })
					.UntilServer(TEXT("The selected real-pawn arrangement blocks the reserved respawn shape on authority"), [this](FState& State)
						{ return RespawnStartHasBlocker(State.World); }, Timeout());
			}
		}
		Network.UntilServer(TEXT("Old authority pawn, equipment actors and follower are released"), [this](FState& State) { return OldObjectsReleased(State.World); }, Timeout())
			.UntilClients(TEXT("All clients release the old pawn-owned visual and equipment objects"), [this](FState& State) { return OldObjectsReleased(State.World); }, Timeout())
			.UntilClient(TEXT("Server-authored respawn delay permits the existing owner request"), 0, [this](FState& State)
				{ const ARpgPlayerState* StateOwner = Player(State.World, SubjectId); return StateOwner && StateOwner->IsWaitingForRespawn() && StateOwner->CanRespawnNow(); }, Timeout());
		if (bOccupiedRespawnStart)
		{
			Network.ThenServer(TEXT("Verify the late joiner still physically blocks the stored start immediately before the real respawn RPC"), [this](FState& State)
			{
				const ARpgPlayerState* Subject = Player(State.World, SubjectId);
				APlayerController* PC = Subject ? Cast<APlayerController>(Subject->GetOwner()) : nullptr;
				const ARpgGameModeBase* Mode = State.World->GetAuthGameMode<ARpgGameModeBase>();
				ASSERT_THAT(IsTrue(PC && Mode && Mode->GetPlayerCheckpointTransform(PC).Equals(OccupiedRespawnTransform, 0.01)));
				const bool bBlocked = RespawnStartHasBlocker(State.World);
				ASSERT_THAT(IsTrue(bBlocked));
				ReportCrowdedCheckpoint(State.World, TEXT("before-respawn-RPC"));
				UE_LOG(LogTemp, Display, TEXT("RpgMoverLifecycle occupied start before respawn blocking=%d reserved=%s blocker=%s actor=%s"),
					bBlocked, *OccupiedRespawnTransform.GetLocation().ToCompactString(), *GetPathNameSafe(Pawn(State.World, RespawnBlockerId)),
					Pawn(State.World, RespawnBlockerId) ? *Pawn(State.World, RespawnBlockerId)->GetActorLocation().ToCompactString() : TEXT("missing"));
			});
		}
		Network.ThenClient(TEXT("Release old held input and send the real owning-controller respawn RPC once"), 0, [this](FState& State)
			{
				Input.Stop();
				ARpgPlayerController* PC = Cast<ARpgPlayerController>(State.World->GetFirstPlayerController());
				ASSERT_THAT(IsTrue(PC && PC->IsLocalController() && !PC->HasAuthority() && PC->GetPlayerState<ARpgPlayerState>() == Player(State.World, SubjectId)));
				if (PC) PC->RequestRespawn();
			})
			.UntilServer(TEXT("Respawn reuses PlayerState ASC and composes a new healthy equipped Mover on authority"), [this](FState& State) { return Respawned(State.World); }, Timeout())
			.UntilClients(TEXT("Owner and late observer receive the new healthy pawn and automatic profile composition"), [this](FState& State) { return Respawned(State.World); }, Timeout())
			.ThenClient(TEXT("Drive the newly possessed pawn through normal Mover input"), 0, [this](FState& State)
				{ Input.Start(LocalPawn(State.World)); Input.Move(true); RespawnMovementStarted = FPlatformTime::Seconds(); })
			.UntilServer(TEXT("All roles naturally move the respawned pawn beyond its starting position"), [this](FState&)
			{
				if (!bReportedRespawnMovement && FPlatformTime::Seconds() - RespawnMovementStarted >= 2.5)
				{
					bReportedRespawnMovement = true; ReportRespawnMovement();
				}
				for (UWorld* World : { AuthorityWorld.Get(), OwnerWorld.Get(), ObserverWorld.Get() })
				{
					APawn* Character = Pawn(World, SubjectId);
					if (!Respawned(World) || Mover(Character)->GetVelocity().Size2D() < 100.0
						|| FVector::Dist2D(Character->GetActorLocation(), Observations.Get(World).RespawnLocation) < 150.0) return false;
				}
				return true;
			}, Timeout())
			.ThenClient(TEXT("Attack using the newly granted weapon after respawn"), 0, [this](FState&) { Input.Attack(); })
			.UntilServer(TEXT("The respawned owner, authority and proxy all present the new weapon montage"), [this](FState&)
			{
				for (UWorld* World : { AuthorityWorld.Get(), OwnerWorld.Get(), ObserverWorld.Get() })
					if (!Observations.Get(World).bSawRespawnMontage) return false;
				return true;
			}, Timeout())
			.ThenClient(TEXT("Release movement at completion"), 0, [this](FState&) { Input.Stop(); })
			.ThenServer(TEXT("Check authority source and persistent state after the restored combat path"), [this](FState& State) { ASSERT_THAT(IsTrue(Respawned(State.World) && Isolation.Isolated(State.World))); })
			.ThenClients(TEXT("Owner and observer retain healthy source composition after respawn"), [this](FState& State) { ASSERT_THAT(IsTrue(Respawned(State.World) && Isolation.Isolated(State.World))); });
	}
};

#endif // ENABLE_PIE_NETWORK_TEST
#endif // WITH_DEV_AUTOMATION_TESTS
