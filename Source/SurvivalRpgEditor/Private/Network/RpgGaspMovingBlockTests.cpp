// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"
#include "Network/RpgCombatNetworkTestTypes.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/InputSettings.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "InputKeyEventArgs.h"
#include "Misc/Guid.h"
#include "UObject/StrongObjectPtr.h"
#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility_Block.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Core/Character/RpgCharacterMoverComponent.h"
#include "SurvivalRpg/Core/Character/RpgMoverPawn.h"
#include "SurvivalRpg/Core/Character/RpgPawnData.h"
#include "SurvivalRpg/Core/Character/RpgPawnExtensionComponent.h"
#include "SurvivalRpg/Core/Character/RpgPawnGameplayComponent.h"
#include "SurvivalRpg/Core/Game/Experience/RpgExperienceDefinition.h"
#include "SurvivalRpg/Core/Game/Experience/RpgExperienceManagerComponent.h"
#include "SurvivalRpg/Core/Game/RpgGameModeBase.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/Development/RpgDeveloperSettings.h"
#include "SurvivalRpg/Equipment/RpgEquipmentManagerComponent.h"
#include "SurvivalRpg/Equipment/RpgWeaponInstance.h"

#if ENABLE_PIE_NETWORK_TEST

namespace RpgGaspMovingBlockTests
{
	constexpr TCHAR SourceMeshPath[] = TEXT("/Game/SurvivalRpg/Characters/GASP/Shared/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin.SKM_UEFN_Mannequin");
	enum class EVariant : uint8 { CMC, Mover };
	enum class EStage : uint8 { Locomotion, MovingBlock, IdleBlock, Released };
	FPrimaryAssetId ExperienceId(EVariant Variant)
	{
		return FPrimaryAssetId(URpgExperienceDefinition::StaticClass()->GetFName(),
			Variant == EVariant::CMC ? TEXT("RpgGaspMantleExperience") : TEXT("RpgGaspMoverExperience"));
	}
	FTimespan Timeout() { return FTimespan::FromSeconds(45.0); }
	bool ActiveWorld(const UWorld* World)
	{
		if (!GEngine || !World) return false;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
			if (Context.WorldType == EWorldType::PIE && Context.World() == World)
				return IsValid(World) && !World->bIsTearingDown && !World->IsBeingCleanedUp();
		return false;
	}
	APawn* LocalPawn(UWorld* World)
	{
		const APlayerController* Controller = ActiveWorld(World) ? World->GetFirstPlayerController() : nullptr;
		return Controller ? Controller->GetPawn() : nullptr;
	}
	APawn* Pawn(UWorld* World, int32 PlayerId)
	{
		const AGameStateBase* GameState = ActiveWorld(World) ? World->GetGameState() : nullptr;
		if (!GameState || PlayerId == INDEX_NONE) return nullptr;
		for (APlayerState* Player : GameState->PlayerArray)
			if (Player && Player->GetPlayerId() == PlayerId) return Player->GetPawn();
		return nullptr;
	}
	USkeletalMeshComponent* Mesh(const APawn* Character) { return URpgPawnExtensionComponent::FindGameplayMesh(Character); }
	URpgEquipmentManagerComponent* Equipment(const APawn* Character) { return Character ? Character->FindComponentByClass<URpgEquipmentManagerComponent>() : nullptr; }
	URpgCharacterMoverComponent* Mover(const APawn* Character) { return Character ? Character->FindComponentByClass<URpgCharacterMoverComponent>() : nullptr; }
	URpgAbilitySystemComponent* ASC(const APawn* Character)
	{
		const URpgPawnExtensionComponent* Extension = URpgPawnExtensionComponent::FindPawnExtensionComponent(Character);
		return Extension ? Extension->GetRpgAbilitySystemComponent() : nullptr;
	}
	float Speed(const APawn* Character)
	{
		return Mover(Character) ? Mover(Character)->GetVelocity().Size2D() : Character ? Character->GetVelocity().Size2D() : 0.0f;
	}
	bool Grounded(const APawn* Character)
	{
		if (Mover(Character)) return Mover(Character)->IsOnGround();
		const ACharacter* CMC = Cast<ACharacter>(Character);
		return CMC && CMC->GetCharacterMovement()->IsMovingOnGround();
	}
	FGameplayAbilitySpec* BlockSpec(APawn* Character)
	{
		if (!ASC(Character) || !Equipment(Character)) return nullptr;
		const FGameplayTag InputTag = FGameplayTag::RequestGameplayTag(TEXT("InputTag.Weapon.Block"));
		for (FGameplayAbilitySpec& Spec : ASC(Character)->GetActivatableAbilities())
		{
			const URpgEquipmentInstance* Item = Cast<URpgEquipmentInstance>(Spec.SourceObject.Get());
			if (!Spec.PendingRemove && Item && Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag)
				&& Cast<URpgGameplayAbility_Block>(Spec.GetPrimaryInstance())
				&& Equipment(Character)->IsEquipmentInstanceActiveForInputTag(Item, InputTag)) return &Spec;
		}
		return nullptr;
	}
	bool Blocking(const APawn* Character)
	{
		return ASC(Character) && ASC(Character)->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.Blocking")));
	}
	bool Ready(UWorld* World, APawn* Character, EVariant Variant)
	{
		const AGameStateBase* GameState = ActiveWorld(World) ? World->GetGameState() : nullptr;
		const URpgExperienceManagerComponent* Experience = GameState ? GameState->FindComponentByClass<URpgExperienceManagerComponent>() : nullptr;
		const URpgPawnExtensionComponent* Extension = URpgPawnExtensionComponent::FindPawnExtensionComponent(Character);
		const URpgPawnGameplayComponent* Gameplay = URpgPawnGameplayComponent::FindPawnGameplayComponent(Character);
		const ARpgPlayerState* Player = Character ? Character->GetPlayerState<ARpgPlayerState>() : nullptr;
		const URpgAbilitySystemComponent* AbilitySystem = ASC(Character);
		const USkeletalMeshComponent* Source = Mesh(Character);
		const FGameplayTag GameplayReady = FGameplayTag::RequestGameplayTag(TEXT("InitState.GameplayReady"));
		return Experience && Experience->IsExperienceLoaded() && Experience->GetCurrentExperienceChecked()->GetPrimaryAssetId() == ExperienceId(Variant)
			&& Character && Player && Extension && Gameplay && AbilitySystem && Source && Equipment(Character)
			&& Extension->HasReachedInitState(GameplayReady) && Gameplay->HasReachedInitState(GameplayReady)
			&& Extension->GetPawnData<URpgPawnData>() == Experience->GetCurrentExperienceChecked()->DefaultPawnData
			&& Player->GetPawnData<URpgPawnData>() == Extension->GetPawnData<URpgPawnData>()
			&& AbilitySystem == Player->GetRpgAbilitySystemComponent() && AbilitySystem->GetOwnerActor() == Player && AbilitySystem->GetAvatarActor() == Character
			&& AbilitySystem->AbilityActorInfo.IsValid() && AbilitySystem->AbilityActorInfo->SkeletalMeshComponent.Get() == Source
			&& Source->GetAnimInstance() && Source->GetSkeletalMeshAsset() && Source->GetSkeletalMeshAsset()->GetPathName() == SourceMeshPath
			&& (Variant == EVariant::CMC ? Cast<ACharacter>(Character) != nullptr
				: Character->IsA<ARpgMoverPawn>() && Mover(Character) && Mover(Character)->GetPrimaryVisualComponent() == Source)
			&& (!Character->IsLocallyControlled() || Gameplay->IsReadyToBindInputs());
	}

	/** Installs the same save isolation and pre-spawn floor used by the Mover gameplay fixtures. */
	class FScopedWorld final
	{
	public:
		~FScopedWorld() { FGameModeEvents::OnGameModeInitializedEvent().Remove(Handle); }
		void Start()
		{
			Prefix = TEXT("SurvivalRpg_MovingBlockAutomation_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
			Handle = FGameModeEvents::OnGameModeInitializedEvent().AddRaw(this, &FScopedWorld::OnInitialized);
		}
		bool IsIsolated(UWorld* World) const
		{
			if (!ActiveWorld(World)) return false;
			if (World->GetNetMode() == NM_Client) return World->GetAuthGameMode() == nullptr;
			const ARpgGameModeBase* Mode = World->GetAuthGameMode<ARpgGameModeBase>();
			return Mode && !Mode->bEnableDiskPersistence && Mode->WorldSaveSlotName == Prefix
				&& Mode->WorldSaveBackupSlotName == Prefix + TEXT("_Backup")
				&& Mode->WorldSaveRecoverySlotName == Prefix + TEXT("_Recovery") && Mode->OfflineProfileKey == Prefix;
		}
	private:
		void OnInitialized(AGameModeBase* Initialized)
		{
			ARpgGameModeBase* Mode = Cast<ARpgGameModeBase>(Initialized);
			if (!Mode || !Mode->GetWorld() || Mode->GetWorld()->WorldType != EWorldType::PIE) return;
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
				Mode->GetWorld()->SpawnActor<APlayerStart>(FVector(0.0, (Index - 1) * 500.0, 120.0), FRotator::ZeroRotator, Spawn);
		}
		FString Prefix;
		FDelegateHandle Handle;
	};
	/** CQTest opens another local window for a remote join; that editor focus change is not a player release. */
	struct FFixtureGameOnlyInputMode final : FInputModeGameOnly
	{
		virtual bool ShouldFlushInputOnViewportFocus() const override { return false; }
	};
	/** Uses ordinary Enhanced Input keys, with PIE focus-flush isolated and restored; never re-presses held RMB. */
	class FScopedInput final
	{
	public:
		~FScopedInput() { Stop(); }
		bool Start(APawn* Character)
		{
			Stop();
			Controller = Cast<APlayerController>(Character->GetController());
			if (!Controller.IsValid()) return false;
			// ARpgPlayerController::RestoreGameplayInputFocus installs GameOnly after possession.
			// Confirm that contract before temporarily changing its focus-flush policy.
#if UE_ENABLE_DEBUG_DRAWING
			if (Controller->GetCurrentInputModeDebugString() != FInputModeGameOnly().GetDebugDisplayName())
			{
				UE_LOG(LogTemp, Display, TEXT("RpgMovingBlock unexpected input mode=%s controller=%s"),
					*Controller->GetCurrentInputModeDebugString(), *Controller->GetPathName());
				Controller.Reset(); return false;
			}
#endif
			if (!Controller->ShouldFlushKeysWhenViewportFocusChanges()) { Controller.Reset(); return false; }
			bOriginalFocusFlush = GetDefault<UInputSettings>()->bShouldFlushPressedKeysOnViewportFocusLost;
			GetMutableDefault<UInputSettings>()->bShouldFlushPressedKeysOnViewportFocusLost = false;
			Controller->SetInputMode(FFixtureGameOnlyInputMode());
			bFocusIsolated = true;
			Controller->SetIgnoreLookInput(true);
			Handle = FWorldDelegates::OnWorldTickStart.AddRaw(this, &FScopedInput::Tick);
			UE_LOG(LogTemp, Display, TEXT("RpgMovingBlock isolated PIE join focus world=%s originalDefaultFlush=%d controllerFlush=%d"),
				*GetPathNameSafe(Controller->GetWorld()), bOriginalFocusFlush, Controller->ShouldFlushKeysWhenViewportFocusChanges());
			return true;
		}
		void Move(bool bMove) { Axis = bMove ? 1.0f : 0.0f; }
		void Block(bool bHold)
		{
			bHoldingBlock = bHold;
			if (Controller.IsValid()) UE_LOG(LogTemp, Display, TEXT("RpgMovingBlock fixture RMB world=%s frame=%llu requestedHold=%d rawBefore=%d"),
				*GetPathNameSafe(Controller->GetWorld()), GFrameCounter, bHold, Controller->IsInputKeyDown(EKeys::RightMouseButton));
			if (Controller.IsValid()) Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::RightMouseButton,
				bHold ? IE_Pressed : IE_Released, bHold ? 1.0f : 0.0f));
		}
		void PressJump()
		{
			if (Controller.IsValid()) Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::SpaceBar, IE_Pressed, 1.0f));
			JumpFrames = 2;
		}
		void Stop()
		{
			FWorldDelegates::OnWorldTickStart.Remove(Handle); Handle.Reset();
			if (Controller.IsValid())
			{
				Block(false);
				Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::SpaceBar, IE_Released, 0.0f));
				Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_LeftY, IE_Axis, 0.0f, 1));
				Controller->SetIgnoreLookInput(false);
				if (bFocusIsolated) Controller->SetInputMode(FInputModeGameOnly());
			}
			if (bFocusIsolated)
			{
				GetMutableDefault<UInputSettings>()->bShouldFlushPressedKeysOnViewportFocusLost = bOriginalFocusFlush;
				bFocusIsolated = false;
				UE_LOG(LogTemp, Display, TEXT("RpgMovingBlock restored PIE focus policy defaultFlush=%d"), bOriginalFocusFlush);
			}
			Controller.Reset(); Axis = 0.0f; JumpFrames = 0; bHoldingBlock = false; bPreviousRawBlock = false;
		}
	private:
		void Tick(UWorld* World, ELevelTick, float)
		{
			if (!Controller.IsValid() || Controller->GetWorld() != World) return;
			const bool bRawBlock = Controller->IsInputKeyDown(EKeys::RightMouseButton);
			if (bHoldingBlock && bPreviousRawBlock && !bRawBlock)
			{
				const FGameplayAbilitySpec* Spec = BlockSpec(Controller->GetPawn());
				UE_LOG(LogTemp, Display, TEXT("RpgMovingBlock unexpected held RMB release world=%s frame=%llu specPressed=%d active=%d controllerFlush=%d defaultFlush=%d"),
					*GetPathNameSafe(World), GFrameCounter, Spec && Spec->InputPressed, Spec && Spec->IsActive(),
					Controller->ShouldFlushKeysWhenViewportFocusChanges(), GetDefault<UInputSettings>()->bShouldFlushPressedKeysOnViewportFocusLost);
			}
			bPreviousRawBlock = bRawBlock;
			Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_LeftY, IE_Axis, Axis, 1));
			if (JumpFrames > 0 && --JumpFrames == 0)
				Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::SpaceBar, IE_Released, 0.0f));
		}
		TWeakObjectPtr<APlayerController> Controller;
		FDelegateHandle Handle;
		float Axis = 0.0f;
		int32 JumpFrames = 0;
		bool bFocusIsolated = false, bOriginalFocusFlush = false, bHoldingBlock = false, bPreviousRawBlock = false;
	};

	/** Local rotations exclude capsule motion; arm motion can never satisfy a leg measurement. */
	TArray<FQuat> ReadPose(USkeletalMeshComponent* Source)
	{
		if (!Source) return {};
		// This public accessor joins outstanding evaluation and copies the naturally evaluated pose.
		const TArray<FTransform> Pose = Source->GetBoneSpaceTransforms();
		TArray<FQuat> Result;
		for (const FName Bone : { FName(TEXT("thigh_l")), FName(TEXT("calf_l")), FName(TEXT("thigh_r")), FName(TEXT("calf_r")),
			FName(TEXT("spine_03")), FName(TEXT("upperarm_l")), FName(TEXT("lowerarm_l")), FName(TEXT("upperarm_r")), FName(TEXT("lowerarm_r")) })
		{
			const int32 Index = Source->GetBoneIndex(Bone);
			if (!Pose.IsValidIndex(Index) || Pose[Index].ContainsNaN() || !Pose[Index].GetRotation().IsNormalized()) return {};
			Result.Add(Pose[Index].GetRotation());
		}
		return Result;
	}
	struct FPoseWindow
	{
		double FirstTime = -1.0, LastSampleTime = -1.0;
		TArray<FQuat> FirstPose;
		float LeftRange = 0.0f, RightRange = 0.0f;
		int32 Samples = 0;
		bool bComplete = false;
		void Sample(double Now, const TArray<FQuat>& Pose)
		{
			if (bComplete || Pose.Num() != 9 || (LastSampleTime >= 0.0 && Now - LastSampleTime < 0.05)) return;
			if (FirstTime < 0.0) { FirstTime = Now; FirstPose = Pose; }
			LastSampleTime = Now; ++Samples;
			LeftRange = FMath::Max(LeftRange, static_cast<float>(FMath::Max(FirstPose[0].AngularDistance(Pose[0]), FirstPose[1].AngularDistance(Pose[1]))));
			RightRange = FMath::Max(RightRange, static_cast<float>(FMath::Max(FirstPose[2].AngularDistance(Pose[2]), FirstPose[3].AngularDistance(Pose[3]))));
			bComplete = Now - FirstTime >= 0.65 && Samples >= 6;
		}
		bool LegsMoved() const
		{
			// The existing pose fixtures use 0.15 rad to distinguish limb animation from numerical/IK noise.
			// Here BOTH legs must meet it in EACH separated window, after the start blend has settled.
			return bComplete && LeftRange > 0.15f && RightRange > 0.15f;
		}
	};
	struct FMeasurement
	{
		double StableSince = -1.0;
		FPoseWindow Windows[2];
		int32 Window = 0;
		void Reset() { *this = FMeasurement(); }
		void Sample(double Now, const TArray<FQuat>& Pose)
		{
			if (StableSince < 0.0) StableSince = Now;
			// No montage-entry blend or one-frame locomotion transition can qualify as a complete window.
			if (Now - StableSince < 0.35 || Window == 2) return;
			Windows[Window].Sample(Now, Pose);
			if (Windows[Window].bComplete) ++Window;
		}
	};
	struct FPeer
	{
		TWeakObjectPtr<APawn> Character;
		TWeakObjectPtr<USkeletalMeshComponent> Source;
		TWeakObjectPtr<UAnimInstance> Animation;
		TWeakObjectPtr<URpgAbilitySystemComponent> AbilitySystem;
		TWeakObjectPtr<URpgEquipmentInstance> Item;
		TArray<TWeakObjectPtr<AActor>> EquipmentActors;
		FGameplayAbilitySpecHandle BlockHandle;
		FDelegateHandle ActivatedHandle, EndedHandle;
		FMeasurement Measure;
		TArray<FQuat> NeutralPose;
		int32 Activations = 0, Ends = 0, CancelledEnds = 0, LoopInstance = INDEX_NONE;
		float IdleBlockSeconds = 0.0f, UpperBodyChange = 0.0f, MinimumSlotWeight = 1.0f;
		float JumpProbeSeconds = 0.0f;
		bool bLateJoin = false, bSourceChanged = false, bEquipmentChanged = false, bLoopReplaced = false;
		bool bRootMotion = false, bInvalidPose = false, bSawEnd = false;
		bool bJumpEscapedBlock = false;
	};
	/** Observes completed per-world poses without aligning network roles to an artificial common phase. */
	class FScopedObservations final
	{
	public:
		~FScopedObservations() { Stop(); }
		void Start(int32 PlayerId, UAnimMontage* Start, UAnimMontage* Loop, UAnimMontage* End, ERpgEquipmentSlot Slot)
		{
			Subject = PlayerId; StartClip = Start; LoopClip = Loop; EndClip = End; EquipmentSlot = Slot;
			Handle = FWorldDelegates::OnWorldTickEnd.AddRaw(this, &FScopedObservations::Tick);
		}
		bool Add(UWorld* World, bool bLateJoin = false)
		{
			APawn* Character = Pawn(World, Subject);
			if (Peers.Contains(World) || !Mesh(Character) || !ASC(Character) || !Equipment(Character)) return false;
			URpgEquipmentInstance* Item = Equipment(Character)->GetEquipmentInstanceInSlot(EquipmentSlot);
			if (!Item || Item->GetSpawnedActors().IsEmpty()) return false;
			FPeer& Peer = Peers.Add(World);
			Peer.Character = Character; Peer.Source = Mesh(Character); Peer.Animation = Mesh(Character)->GetAnimInstance();
			Peer.AbilitySystem = ASC(Character); Peer.Item = Item; Peer.bLateJoin = bLateJoin;
			if (!bLateJoin) Peer.NeutralPose = ReadPose(Mesh(Character));
			for (AActor* Actor : Item->GetSpawnedActors()) Peer.EquipmentActors.Add(Actor);
			if (const FGameplayAbilitySpec* Spec = BlockSpec(Character)) Peer.BlockHandle = Spec->Handle;
			Peer.ActivatedHandle = ASC(Character)->AbilityActivatedCallbacks.AddLambda([this, World](UGameplayAbility* Ability)
			{
				FPeer& Record = Peers.FindChecked(World);
				if (Ability && Ability->GetCurrentAbilitySpecHandle() == Record.BlockHandle) ++Record.Activations;
			});
			Peer.EndedHandle = ASC(Character)->OnAbilityEnded.AddLambda([this, World](const FAbilityEndedData& Ended)
			{
				FPeer& Record = Peers.FindChecked(World);
				if (Ended.AbilityThatEnded && Ended.AbilityThatEnded->GetCurrentAbilitySpecHandle() == Record.BlockHandle)
				{
					++Record.Ends;
					if (Ended.bWasCancelled) ++Record.CancelledEnds;
					const APawn* EndPawn = Record.Character.Get();
					const APlayerController* EndController = EndPawn ? Cast<APlayerController>(EndPawn->GetController()) : nullptr;
					UE_LOG(LogTemp, Display, TEXT("RpgMovingBlock ability ended world=%s frame=%llu role=%d cancelled=%d local=%d rawRMB=%d"),
						*GetPathNameSafe(World), GFrameCounter, EndPawn ? static_cast<int32>(EndPawn->GetLocalRole()) : -1,
						Ended.bWasCancelled, EndController && EndController->IsLocalController(), EndController && EndController->IsInputKeyDown(EKeys::RightMouseButton));
				}
			});
			return true;
		}
		void SetStage(EStage Next)
		{
			Stage = Next;
			for (auto& Entry : Peers) { Entry.Value.Measure.Reset(); Entry.Value.IdleBlockSeconds = 0.0f; }
		}
		bool AllComplete(int32 Count) const
		{
			if (Peers.Num() != Count) return false;
			for (const auto& Entry : Peers) if (Entry.Value.Measure.Window != 2) return false;
			return true;
		}
		bool AllIdleBlock() const
		{
			if (Peers.Num() != 3) return false;
			for (const auto& Entry : Peers) if (Entry.Value.IdleBlockSeconds < 0.5f) return false;
			return true;
		}
		void BeginJumpProbe() { bProbeJump = true; }
		bool JumpProbeComplete() const
		{
			if (Peers.Num() != 3) return false;
			for (const auto& Entry : Peers) if (Entry.Value.JumpProbeSeconds < 0.5f) return false;
			return true;
		}
		void EndJumpProbe() { bProbeJump = false; }
		bool Released(APawn* Character) const
		{
			if (!ASC(Character) || !Mesh(Character) || !Mesh(Character)->GetAnimInstance() || Blocking(Character)
				|| ASC(Character)->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.PerfectBlockWindow")))
				|| ASC(Character)->GetCurrentMontage()) return false;
			for (const UAnimMontage* Clip : { StartClip.Get(), LoopClip.Get(), EndClip.Get() })
				if (Clip && Mesh(Character)->GetAnimInstance()->Montage_IsPlaying(Clip)) return false;
			const FGameplayAbilitySpec* Spec = BlockSpec(Character);
			return Character->GetLocalRole() == ROLE_SimulatedProxy || (Spec && !Spec->IsActive());
		}
		const TMap<TWeakObjectPtr<UWorld>, FPeer>& GetPeers() const { return Peers; }
		void Report(const TCHAR* Label) const
		{
			for (const auto& Entry : Peers)
			{
				const FPeer& Peer = Entry.Value;
				APawn* Character = Peer.Character.Get();
				UE_LOG(LogTemp, Display, TEXT("RpgMovingBlock stage=%s world=%s role=%d late=%d speed=%.2f grounded=%d tag=%d current=%s loopId=%d windows=%d left=%.4f/%.4f right=%.4f/%.4f samples=%d/%d slotMin=%.3f upperChange=%.4f idle=%.3f activated=%d ended=%d cancelled=%d endClip=%d sourceChanged=%d equipmentChanged=%d loopReplaced=%d rootMotion=%d invalidPose=%d"),
					Label, *GetPathNameSafe(Entry.Key.Get()), Character ? static_cast<int32>(Character->GetLocalRole()) : -1, Peer.bLateJoin,
					Speed(Character), Grounded(Character), Blocking(Character), *GetPathNameSafe(ASC(Character) ? ASC(Character)->GetCurrentMontage() : nullptr),
					Peer.LoopInstance, Peer.Measure.Window, Peer.Measure.Windows[0].LeftRange, Peer.Measure.Windows[1].LeftRange,
					Peer.Measure.Windows[0].RightRange, Peer.Measure.Windows[1].RightRange, Peer.Measure.Windows[0].Samples, Peer.Measure.Windows[1].Samples,
					Peer.MinimumSlotWeight, Peer.UpperBodyChange, Peer.IdleBlockSeconds, Peer.Activations, Peer.Ends, Peer.CancelledEnds, Peer.bSawEnd,
					Peer.bSourceChanged, Peer.bEquipmentChanged, Peer.bLoopReplaced, Peer.bRootMotion, Peer.bInvalidPose);
				if (Peer.JumpProbeSeconds > 0.0f)
					UE_LOG(LogTemp, Display, TEXT("RpgMovingBlock jumpProbe world=%s seconds=%.3f escaped=%d"),
						*GetPathNameSafe(Entry.Key.Get()), Peer.JumpProbeSeconds, Peer.bJumpEscapedBlock);
			}
		}
		void Stop()
		{
			FWorldDelegates::OnWorldTickEnd.Remove(Handle); Handle.Reset();
			for (auto& Entry : Peers) if (URpgAbilitySystemComponent* AbilitySystem = Entry.Value.AbilitySystem.Get())
			{
				AbilitySystem->AbilityActivatedCallbacks.Remove(Entry.Value.ActivatedHandle);
				AbilitySystem->OnAbilityEnded.Remove(Entry.Value.EndedHandle);
			}
			Peers.Empty();
		}
	private:
		bool StableLoop(APawn* Character, FPeer& Peer)
		{
			UAnimInstance* Animation = Peer.Animation.Get();
			FAnimMontageInstance* Instance = Animation ? Animation->GetActiveInstanceForMontage(LoopClip.Get()) : nullptr;
			if (!Blocking(Character) || ASC(Character)->GetCurrentMontage() != LoopClip.Get() || !Instance || !Instance->IsPlaying()) return false;
			if (Peer.LoopInstance == INDEX_NONE) Peer.LoopInstance = Instance->GetInstanceID();
			Peer.bLoopReplaced |= Peer.LoopInstance != Instance->GetInstanceID();
			const FGameplayAbilitySpec* Spec = BlockSpec(Character);
			if (Character->GetLocalRole() != ROLE_SimulatedProxy && (!Spec || !Spec->IsActive())) return false;
			float SlotWeight = 0.0f;
			for (const FSlotAnimationTrack& Track : LoopClip->SlotAnimTracks)
				SlotWeight = FMath::Max(SlotWeight, Animation->GetSlotMontageGlobalWeight(Track.SlotName));
			if (Instance->GetWeight() < 0.99f || SlotWeight < 0.5f) return false;
			Peer.MinimumSlotWeight = FMath::Min(Peer.MinimumSlotWeight, SlotWeight);
			return true;
		}
		void Tick(UWorld* World, ELevelTick, float DeltaSeconds)
		{
			FPeer* Peer = Peers.Find(World);
			if (!Peer || !ActiveWorld(World)) return;
			APawn* Character = Pawn(World, Subject);
			Peer->bSourceChanged |= Character != Peer->Character.Get() || Mesh(Character) != Peer->Source.Get()
				|| ASC(Character) != Peer->AbilitySystem.Get() || !Mesh(Character) || Mesh(Character)->GetAnimInstance() != Peer->Animation.Get();
			if (Peer->bSourceChanged) return;
			const URpgEquipmentInstance* Item = Equipment(Character) ? Equipment(Character)->GetEquipmentInstanceInSlot(EquipmentSlot) : nullptr;
			Peer->bEquipmentChanged |= Item != Peer->Item.Get() || !Item || Item->GetSpawnedActors().Num() != Peer->EquipmentActors.Num();
			for (const TWeakObjectPtr<AActor>& Actor : Peer->EquipmentActors)
				Peer->bEquipmentChanged |= !Actor.IsValid() || !Actor->GetRootComponent() || Actor->GetRootComponent()->GetAttachParent() != Peer->Source.Get();
			UAnimInstance* Animation = Peer->Animation.Get();
			Peer->bRootMotion |= Animation->GetRootMotionMontageInstance() != nullptr
				|| (Mover(Character) && Mover(Character)->FindActiveLayeredMoveByType(FRpgMoverAbilityRootMotion::StaticStruct()) != nullptr);
			Peer->bSawEnd |= EndClip.IsValid() && Animation->Montage_IsPlaying(EndClip.Get());
			const TArray<FQuat> Pose = ReadPose(Mesh(Character));
			Peer->bInvalidPose |= Pose.Num() != 9;
			const bool bStableLoop = StableLoop(Character, *Peer);
			if (bProbeJump)
			{
				Peer->JumpProbeSeconds += DeltaSeconds;
				Peer->bJumpEscapedBlock |= !Grounded(Character) || !bStableLoop;
			}
			if (Stage == EStage::IdleBlock)
			{
				if (bStableLoop && Grounded(Character) && Speed(Character) < 5.0f && Pose.Num() == 9)
				{
					Peer->IdleBlockSeconds += DeltaSeconds;
					if (Peer->NeutralPose.Num() == 9)
						for (int32 Index = 4; Index < 9; ++Index)
							Peer->UpperBodyChange = FMath::Max(Peer->UpperBodyChange, static_cast<float>(Peer->NeutralPose[Index].AngularDistance(Pose[Index])));
				}
				else Peer->IdleBlockSeconds = 0.0f;
				return;
			}
			const bool bContext = Stage == EStage::MovingBlock ? bStableLoop : Released(Character);
			if (bContext && Grounded(Character) && Speed(Character) > 100.0f && Pose.Num() == 9)
				Peer->Measure.Sample(World->GetTimeSeconds(), Pose);
			else Peer->Measure.Reset();
		}
		TMap<TWeakObjectPtr<UWorld>, FPeer> Peers;
		FDelegateHandle Handle;
		TWeakObjectPtr<UAnimMontage> StartClip, LoopClip, EndClip;
		int32 Subject = INDEX_NONE;
		ERpgEquipmentSlot EquipmentSlot = ERpgEquipmentSlot::None;
		EStage Stage = EStage::Locomotion;
		bool bProbeJump = false;
	};
	struct FState : FBasePIENetworkComponentState {};
}

NETWORK_TEST_CLASS(GaspMovingBlockPIE, "SurvivalRpg.GASP.MovingBlock")
{
	using FState = RpgGaspMovingBlockTests::FState;
	using EVariant = RpgGaspMovingBlockTests::EVariant;
	using EStage = RpgGaspMovingBlockTests::EStage;
	RpgGaspMovingBlockTests::FScopedWorld Isolation;
	RpgGaspMovingBlockTests::FScopedInput Input;
	RpgGaspMovingBlockTests::FScopedObservations Observations;
	TStrongObjectPtr<UAnimMontage> BlockStart, BlockLoop, BlockEnd;
	// Construct after selecting the variant, before CQTest starts its isolated PIE worlds.
	TUniquePtr<FPIENetworkComponent<FState>> Network;
	FPrimaryAssetId PreviousExperience;
	EVariant Variant = EVariant::CMC;
	ERpgEquipmentSlot BlockSlot = ERpgEquipmentSlot::None;
	int32 SubjectId = INDEX_NONE;
	bool bConfigured = false;

	AFTER_EACH()
	{
		Input.Stop();
		if (TestRunner->HasAnyErrors()) Observations.Report(TEXT("Failure"));
		Observations.Stop();
		if (bConfigured) GetMutableDefault<URpgDeveloperSettings>()->ExperienceOverride = PreviousExperience;
	}
	TEST_METHOD(CMCMovingBlockKeepsLegsAnimatedThroughLateJoinAndRelease) { Queue(EVariant::CMC); }
	TEST_METHOD(MoverMovingBlockKeepsLegsAnimatedThroughLateJoinAndRelease) { Queue(EVariant::Mover); }

	void VerifyLegs(const TCHAR* Label, int32 Count)
	{
		using namespace RpgGaspMovingBlockTests;
		Observations.Report(Label);
		TestRunner->TestEqual(TEXT("The expected network worlds contributed completed pose windows"), Observations.GetPeers().Num(), Count);
		for (const auto& Entry : Observations.GetPeers())
		{
			const FPeer& Peer = Entry.Value;
			TestRunner->TestTrue(TEXT("Each role retains the original gameplay mesh, ASC and equipped actor attachments"),
				!Peer.bSourceChanged && !Peer.bEquipmentChanged && Ready(Entry.Key.Get(), Peer.Character.Get(), Variant));
			TestRunner->TestTrue(TEXT("Poses are finite and block never creates a root-motion movement owner"), !Peer.bInvalidPose && !Peer.bRootMotion);
			for (int32 Index = 0; Index < 2; ++Index)
				TestRunner->TestTrue(FString::Printf(TEXT("%s %s window %d: both legs animate in the completed local pose"),
					Label, *GetPathNameSafe(Entry.Key.Get()), Index), Peer.Measure.Windows[Index].LegsMoved());
		}
	}
	void Queue(EVariant SelectedVariant)
	{
		using namespace RpgGaspMovingBlockTests;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
			if (Context.WorldType == EWorldType::PIE && IsValid(Context.World()))
			{
				TestRunner->AddError(TEXT("Moving block automation refuses to interrupt an existing PIE session.")); return;
			}
		Variant = SelectedVariant;
		const TCHAR* GameModePath = Variant == EVariant::CMC
			? TEXT("/Game/SurvivalRpg/Maps/Test/GaspMantle/BP_Rpg_GaspMantleTestGameMode.BP_Rpg_GaspMantleTestGameMode_C")
			: TEXT("/Game/SurvivalRpg/Maps/Test/GaspMover/BP_Rpg_GaspMoverTestGameMode.BP_Rpg_GaspMoverTestGameMode_C");
		UClass* GameMode = LoadClass<ARpgGameModeBase>(nullptr, GameModePath);
		ASSERT_THAT(IsNotNull(GameMode));
		if (!GameMode) return;
		Isolation.Start();
		PreviousExperience = GetDefault<URpgDeveloperSettings>()->ExperienceOverride;
		GetMutableDefault<URpgDeveloperSettings>()->ExperienceOverride = ExperienceId(Variant);
		bConfigured = true;
		Network = MakeUnique<FPIENetworkComponent<FState>>(TestRunner, TestCommandBuilder, bInitializing);
		FNetworkComponentBuilder<FState>().WithClients(1).AsListenServer()
			.WithGameInstanceClass(FSoftClassPath(TEXT("/Game/SurvivalRpg/Core/Game/BP_Rpg_GameInstance.BP_Rpg_GameInstance_C")))
			.WithGameMode(GameMode).Build(*Network);
		Network->UntilClient(TEXT("The real remote player receives the selected Experience and block input grant"), 0, [this](FState& PeerState)
			{ APawn* Character = LocalPawn(PeerState.World); return Ready(PeerState.World, Character, Variant) && Grounded(Character) && BlockSpec(Character); }, Timeout())
			.ThenClient(TEXT("Retain the autonomous subject and its real player input"), 0, [this](FState& PeerState)
			{
				APawn* Character = LocalPawn(PeerState.World);
				ASSERT_THAT(IsTrue(Character->GetLocalRole() == ROLE_AutonomousProxy));
				SubjectId = Character->GetPlayerState()->GetPlayerId(); ASSERT_THAT(IsTrue(Input.Start(Character)));
			})
			.UntilServer(TEXT("Authority resolves the same composed pawn and actual equipment block definition"), [this](FState& PeerState)
			{ APawn* Character = Pawn(PeerState.World, SubjectId); return Ready(PeerState.World, Character, Variant) && BlockSpec(Character); }, Timeout())
			.ThenServer(TEXT("Read authored block clips and begin passive observation before input"), [this](FState& PeerState)
			{
				const URpgWeaponInstance* Item = Cast<URpgWeaponInstance>(BlockSpec(Pawn(PeerState.World, SubjectId))->SourceObject.Get());
				ASSERT_THAT(IsNotNull(Item)); if (!Item) return;
				const FRpgWeaponBlockDefinition& Definition = Item->GetBlockDefinition();
				BlockSlot = Item->GetEquippedSlot(); BlockStart.Reset(Definition.BlockStartMontage.Get());
				BlockLoop.Reset(Definition.BlockLoopMontage.Get()); BlockEnd.Reset(Definition.BlockEndMontage.Get());
				ASSERT_THAT(IsTrue(Definition.bCanBlock && BlockLoop.IsValid()));
				if (!BlockLoop.IsValid()) return;
				ASSERT_THAT(IsFalse(BlockLoop->SlotAnimTracks.IsEmpty()));
				for (const UAnimMontage* Clip : { BlockStart.Get(), BlockLoop.Get(), BlockEnd.Get() })
					if (Clip) ASSERT_THAT(IsFalse(Clip->HasRootMotion()));
				UE_LOG(LogTemp, Display, TEXT("RpgMovingBlock content experience=%s start=%s loop=%s end=%s"),
					*ExperienceId(Variant).ToString(), *GetPathNameSafe(BlockStart.Get()), *GetPathNameSafe(BlockLoop.Get()), *GetPathNameSafe(BlockEnd.Get()));
				Observations.Start(SubjectId, BlockStart.Get(), BlockLoop.Get(), BlockEnd.Get(), BlockSlot);
				ASSERT_THAT(IsTrue(Isolation.IsIsolated(PeerState.World) && Observations.Add(PeerState.World)));
			})
			.ThenClient(TEXT("Observe the owner before the block starts"), 0, [this](FState& PeerState)
				{ ASSERT_THAT(IsTrue(Isolation.IsIsolated(PeerState.World) && Observations.Add(PeerState.World))); Input.Move(true); })
			.UntilServer(TEXT("Normal movement produces two completed pose windows on owner and authority"), [this](FState&)
				{ return Observations.AllComplete(2); }, Timeout())
			.ThenServer(TEXT("Confirm the positive locomotion baseline before testing block"), [this](FState&) { VerifyLegs(TEXT("Locomotion"), 2); })
			.ThenClient(TEXT("Hold RMB while continuing the same movement input"), 0, [this](FState&)
				{ Observations.SetStage(EStage::MovingBlock); Input.Block(true); })
			.UntilServer(TEXT("The held loop settles and produces two independent moving pose windows"), [this](FState&)
				{ return Observations.AllComplete(2); }, Timeout())
			.ThenServer(TEXT("Require animated legs under the actual weighted block loop"), [this](FState&) { VerifyLegs(TEXT("MovingBlockBeforeJoin"), 2); })
			.ThenClientJoins()
			.UntilClient(TEXT("A real late join receives the already-blocking moving simulated proxy"), 1, [this](FState& PeerState)
			{
				APawn* Character = Pawn(PeerState.World, SubjectId);
				const URpgEquipmentInstance* Item = Equipment(Character) ? Equipment(Character)->GetEquipmentInstanceInSlot(BlockSlot) : nullptr;
				return Ready(PeerState.World, Character, Variant) && Character->GetLocalRole() == ROLE_SimulatedProxy
					&& Blocking(Character) && ASC(Character)->GetCurrentMontage() == BlockLoop.Get()
					&& Item && !Item->GetSpawnedActors().IsEmpty();
			}, Timeout())
			.ThenClient(TEXT("Observe the late proxy without retriggering the owner's block"), 1, [this](FState& PeerState)
				{ ASSERT_THAT(IsTrue(Isolation.IsIsolated(PeerState.World) && Observations.Add(PeerState.World, true))); Observations.SetStage(EStage::MovingBlock); })
			.UntilServer(TEXT("All three roles produce fresh steady-loop pose windows after late join"), [this](FState&)
				{ return Observations.AllComplete(3); }, Timeout())
			.ThenServer(TEXT("Late join presents animated legs underneath the same held block"), [this](FState&) { VerifyLegs(TEXT("MovingBlockAfterJoin"), 3); })
			.ThenClient(TEXT("Press real Space while continuing the fully weighted held block"), 0, [this](FState&)
				{ Observations.BeginJumpProbe(); Input.PressJump(); })
			.UntilServer(TEXT("Observe every role after the blocked jump input"), [this](FState&)
				{ return Observations.JumpProbeComplete(); }, Timeout())
			.ThenServer(TEXT("Held block consumes jump without leaving the ground or replacing its montage"), [this](FState&)
			{
				Observations.EndJumpProbe();
				for (const auto& Entry : Observations.GetPeers())
					TestRunner->TestFalse(TEXT("Space cannot interrupt the weighted equipment block with jump or traversal"), Entry.Value.bJumpEscapedBlock);
			})
			.ThenClient(TEXT("Stop moving while continuing to hold RMB"), 0, [this](FState&)
				{ Observations.SetStage(EStage::IdleBlock); Input.Move(false); })
			.UntilServer(TEXT("Every role reaches supported idle while retaining the weighted block loop"), [this](FState&)
				{ return Observations.AllIdleBlock(); }, Timeout())
			.ThenServer(TEXT("A non-neutral upper-body pose accompanies the weighted stationary block"), [this](FState&)
			{
				Observations.Report(TEXT("IdleBlock"));
				for (const auto& Entry : Observations.GetPeers())
				{
					const FPeer& Peer = Entry.Value;
					TestRunner->TestTrue(TEXT("The original loop instance remains weighted through move-to-idle"),
						Peer.LoopInstance != INDEX_NONE && !Peer.bLoopReplaced && Peer.MinimumSlotWeight >= 0.5f);
					if (!Peer.bLateJoin)
						// This is no mask or shield-angle oracle; asset review and visual inspection verify the authored block pose.
						TestRunner->TestTrue(TEXT("Weighted block has a finite non-neutral chest/arm pose"), !Peer.bInvalidPose && Peer.UpperBodyChange > 0.15f);
				}
			})
			.ThenClient(TEXT("Resume ordinary movement without releasing or retriggering block"), 0, [this](FState&)
				{ Observations.SetStage(EStage::MovingBlock); Input.Move(true); })
			.UntilServer(TEXT("The same block instance leaves both legs animated after movement resumes"), [this](FState&)
				{ return Observations.AllComplete(3); }, Timeout())
			.ThenServer(TEXT("Verify resumed lower-body animation on all roles"), [this](FState&) { VerifyLegs(TEXT("ResumedBlock"), 3); })
			.ThenClient(TEXT("Release RMB while ordinary movement continues"), 0, [this](FState&)
				{ Observations.SetStage(EStage::Released); Input.Block(false); })
			.UntilServer(TEXT("Authored block release finishes and ordinary animated locomotion returns on every role"), [this](FState&)
				{ return Observations.AllComplete(3); }, Timeout())
			.ThenServer(TEXT("Verify one normal GAS end, presentation cleanup and restored moving poses"), [this](FState&)
			{
				VerifyLegs(TEXT("Released"), 3);
				for (const auto& Entry : Observations.GetPeers())
				{
					const FPeer& Peer = Entry.Value;
					TestRunner->TestTrue(TEXT("Release leaves no held-block state or montage on any role"), Observations.Released(Peer.Character.Get()));
					TestRunner->TestFalse(TEXT("Movement transitions never restarted the block loop"), Peer.bLoopReplaced);
					if (BlockEnd.IsValid()) TestRunner->TestTrue(TEXT("Each role actually played the authored end montage"), Peer.bSawEnd);
					if (!Peer.bLateJoin)
					{
						TestRunner->TestEqual(TEXT("Actual block input activated exactly once"), Peer.Activations, 1);
						TestRunner->TestEqual(TEXT("Release ended the real ability exactly once"), Peer.Ends, 1);
						TestRunner->TestEqual(TEXT("Normal input release was not cancellation"), Peer.CancelledEnds, 0);
					}
				}
			})
			.ThenClient(TEXT("Release movement after the complete block lifecycle"), 0, [this](FState&) { Input.Move(false); })
			.UntilServer(TEXT("All roles return to ordinary supported idle"), [this](FState&)
			{
				for (const auto& Entry : Observations.GetPeers())
					if (!Grounded(Entry.Value.Character.Get()) || Speed(Entry.Value.Character.Get()) >= 5.0f || !Observations.Released(Entry.Value.Character.Get())) return false;
				return Observations.GetPeers().Num() == 3;
			}, Timeout());
	}
};

#endif
#endif
