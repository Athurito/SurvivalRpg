// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"
#include "Network/RpgCombatNetworkTestTypes.h"
#include "Network/RpgMoverPredictionTestHelpers.h"
#include "Network/RpgMoverPredictionTestTypes.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/PlayerCameraManager.h"
#include "Backends/MoverNetworkPredictionLiaison.h"
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
#include "MoveLibrary/MoverBlackboard.h"
#include "NetworkPredictionWorldManager.h"
#include "UserDefinedStructSupport.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
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
	enum class EScenario : uint8 { LegLifecycle, DirectionalFacing, SprintOrdering, FixedCorrection, AuthorityCancel };
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
	bool EquipmentReady(const APawn* Character, ERpgEquipmentSlot Slot)
	{
		const USkeletalMeshComponent* Source = Mesh(Character);
		const URpgEquipmentInstance* Item = Equipment(Character) ? Equipment(Character)->GetEquipmentInstanceInSlot(Slot) : nullptr;
		if (!Source || !Item || Item->GetSpawnedActors().IsEmpty()) return false;
		// Replication may expose an array with unresolved actor references before their channels arrive.
		for (const AActor* Actor : Item->GetSpawnedActors())
			if (!IsValid(Actor) || !Actor->GetRootComponent() || Actor->GetRootComponent()->GetAttachParent() != Source) return false;
		return true;
	}
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
	/** Reads the real Blueprint input schema; never writes or reconstructs a command. */
	FString InputGait(const FMoverInputCmdContext* Input)
	{
		if (!Input) return {};
		for (const TSharedPtr<FMoverDataStructBase>& Entry : Input->InputCollection.GetDataArray())
		{
			if (!Entry.IsValid() || Entry->GetScriptStruct() != FMoverUserDefinedDataStruct::StaticStruct()
				|| !Entry->GetDataScriptStruct() || Entry->GetDataScriptStruct()->GetFName() != TEXT("S_MoverCustomInputs")) continue;
			const FInstancedStruct& Instance = static_cast<const FMoverUserDefinedDataStruct*>(Entry.Get())->StructInstance;
			for (TFieldIterator<FProperty> Property(Instance.GetScriptStruct()); Property; ++Property)
			{
				if (Property->GetAuthoredName() != TEXT("Gait")) continue;
				const void* Value = Property->ContainerPtrToValuePtr<void>(Instance.GetMemory());
				if (const FByteProperty* Byte = CastField<FByteProperty>(*Property); Byte && Byte->Enum)
					return Byte->Enum->GetDisplayNameTextByValue(Byte->GetPropertyValue(Value)).ToString();
				if (const FEnumProperty* Enum = CastField<FEnumProperty>(*Property))
					return Enum->GetEnum()->GetDisplayNameTextByValue(Enum->GetUnderlyingProperty()->GetSignedIntPropertyValue(Value)).ToString();
			}
		}
		return {};
	}
	const FMoverInputCmdContext* PendingInput(APawn* Character)
	{
		const UMoverNetworkPredictionLiaisonComponent* Backend = Character ? Character->FindComponentByClass<UMoverNetworkPredictionLiaisonComponent>() : nullptr;
		// NetworkPrediction exposes no component getter for its proxy. Reflect the existing value read-only,
		// then use the engine's public typed reader, rather than touching native frame buffers.
		const FStructProperty* Property = FindFProperty<FStructProperty>(UNetworkPredictionComponent::StaticClass(), TEXT("NetworkPredictionProxy"));
		const FNetworkPredictionProxy* Proxy = Backend && Property && Property->Struct == FNetworkPredictionProxy::StaticStruct()
			? Property->ContainerPtrToValuePtr<FNetworkPredictionProxy>(Backend) : nullptr;
		return Proxy ? Proxy->ReadInputCmd<FMoverInputCmdContext>() : nullptr;
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
			// Keep input-driven routes clear of idle host/late-join capsules while all lanes remain within normal pawn net relevancy.
			for (int32 Index = 0; Index < 3; ++Index)
				Mode->GetWorld()->SpawnActor<APlayerStart>(FVector(0.0, (Index - 1) * 5000.0, 120.0), FRotator::ZeroRotator, Spawn);
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
			bIgnoringLook = true;
			Handle = FWorldDelegates::OnWorldTickStart.AddRaw(this, &FScopedInput::Tick);
			UE_LOG(LogTemp, Display, TEXT("RpgMovingBlock isolated PIE join focus world=%s originalDefaultFlush=%d controllerFlush=%d"),
				*GetPathNameSafe(Controller->GetWorld()), bOriginalFocusFlush, Controller->ShouldFlushKeysWhenViewportFocusChanges());
			return true;
		}
		void Move(bool bMove) { Axis = bMove ? 1.0f : 0.0f; }
		APawn* Subject() const { return Controller.IsValid() ? Controller->GetPawn() : nullptr; }
		void MoveKeys(FVector2D Direction)
		{
			Axis = 0.0f;
			Key(EKeys::W, Direction.Y > 0.0); Key(EKeys::S, Direction.Y < 0.0);
			Key(EKeys::D, Direction.X > 0.0); Key(EKeys::A, Direction.X < 0.0);
		}
		void Sprint(bool bHold) { Key(EKeys::LeftShift, bHold); }
		void Look(float Amount)
		{
			LookAxis = Amount;
			if (!Controller.IsValid()) return;
			if (Amount != 0.0f && bIgnoringLook) { Controller->SetIgnoreLookInput(false); bIgnoringLook = false; }
			else if (Amount == 0.0f && !bIgnoringLook) { Controller->SetIgnoreLookInput(true); bIgnoringLook = true; }
		}
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
				Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_RightX, IE_Axis, 0.0f, 1));
				MoveKeys(FVector2D::ZeroVector); Sprint(false);
				if (bIgnoringLook) Controller->SetIgnoreLookInput(false);
				if (bFocusIsolated) Controller->SetInputMode(FInputModeGameOnly());
			}
			if (bFocusIsolated)
			{
				GetMutableDefault<UInputSettings>()->bShouldFlushPressedKeysOnViewportFocusLost = bOriginalFocusFlush;
				bFocusIsolated = false;
				UE_LOG(LogTemp, Display, TEXT("RpgMovingBlock restored PIE focus policy defaultFlush=%d"), bOriginalFocusFlush);
			}
			Controller.Reset(); Axis = LookAxis = 0.0f; JumpFrames = 0; bHoldingBlock = false; bPreviousRawBlock = false; bIgnoringLook = false;
		}
	private:
		void Key(FKey KeyName, bool bHold)
		{
			if (Controller.IsValid() && Controller->IsInputKeyDown(KeyName) != bHold)
				Controller->InputKey(FInputKeyEventArgs::CreateSimulated(KeyName, bHold ? IE_Pressed : IE_Released, bHold ? 1.0f : 0.0f));
		}
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
			Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_RightX, IE_Axis, LookAxis, 1));
			if (JumpFrames > 0 && --JumpFrames == 0)
				Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::SpaceBar, IE_Released, 0.0f));
		}
		TWeakObjectPtr<APlayerController> Controller;
		FDelegateHandle Handle;
		float Axis = 0.0f, LookAxis = 0.0f;
		int32 JumpFrames = 0;
		bool bFocusIsolated = false, bOriginalFocusFlush = false, bHoldingBlock = false, bPreviousRawBlock = false;
		bool bIgnoringLook = false;
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
	/** Samples steady movement after a bounded settling interval; failed samples cannot be retried into a pass. */
	struct FMotionProbe
	{
		float Elapsed = 0.0f, SampleSeconds = 0.0f, SpeedSum = 0.0f;
		float MinimumSpeed = MAX_flt, MaximumSpeed = 0.0f, MaximumYawError = 0.0f, MinimumDirectionDot = 1.0f;
		int32 Samples = 0;
		FVector FirstLocation = FVector::ZeroVector, LastLocation = FVector::ZeroVector;
		FPoseWindow Legs;
		bool bWrongBlockState = false, bLostGround = false;
		void Sample(APawn* Character, float DeltaSeconds, bool bExpectedBlock, const FVector& Direction, float FacingYaw,
			bool bCheckFacing, const TArray<FQuat>& Pose)
		{
			Elapsed += DeltaSeconds;
			if (Elapsed < 1.0f || Complete()) return;
			if (Samples == 0) FirstLocation = Character->GetActorLocation();
			LastLocation = Character->GetActorLocation();
			++Samples; SampleSeconds += DeltaSeconds;
			const float CurrentSpeed = Speed(Character);
			MinimumSpeed = FMath::Min(MinimumSpeed, CurrentSpeed); MaximumSpeed = FMath::Max(MaximumSpeed, CurrentSpeed); SpeedSum += CurrentSpeed;
			bWrongBlockState |= Blocking(Character) != bExpectedBlock || !ASC(Character)
				|| ASC(Character)->IsBlockMovementActive() != bExpectedBlock;
			if (const URpgCharacterMoverComponent* Movement = Mover(Character))
			{
				const FRpgMoverBlockMovementSyncState* BlockState = Movement->GetSyncState().SyncStateCollection.FindDataByType<FRpgMoverBlockMovementSyncState>();
				bWrongBlockState |= !BlockState || BlockState->bBlocking != bExpectedBlock;
			}
			bLostGround |= !Grounded(Character);
			if (bCheckFacing)
				MaximumYawError = FMath::Max(MaximumYawError, static_cast<float>(FMath::Abs(FMath::FindDeltaAngleDegrees(FacingYaw, Character->GetActorRotation().Yaw))));
			if (!Direction.IsNearlyZero())
			{
				const FVector Velocity = Mover(Character) ? Mover(Character)->GetVelocity() : Character->GetVelocity();
				MinimumDirectionDot = FMath::Min(MinimumDirectionDot, static_cast<float>(FVector::DotProduct(Velocity.GetSafeNormal2D(), Direction)));
				Legs.Sample(Character->GetWorld()->GetTimeSeconds(), Pose);
			}
		}
		bool Complete() const { return SampleSeconds >= 0.75f && Samples >= 6; }
		float MeanSpeed() const { return Samples > 0 ? SpeedSum / Samples : 0.0f; }
	};
	/** Geometry diagnostics include entry/exit blends, unlike the deliberately settled locomotion windows. */
	struct FArmGeometry
	{
		TArray<FVector> PreviousTranslations;
		float MaxReferenceOffset = 0.0f, MaxLengthError = 0.0f, MaxScaleError = 0.0f, MaxFrameStep = 0.0f;
		FName WorstOffsetBone;
		int32 Samples = 0;
		bool bInvalid = false;
		void Sample(USkeletalMeshComponent* Source)
		{
			if (!Source || !Source->GetSkeletalMeshAsset()) { bInvalid = true; return; }
			const TArray<FTransform> Pose = Source->GetBoneSpaceTransforms();
			const TArray<FTransform>& Reference = Source->GetSkeletalMeshAsset()->GetRefSkeleton().GetRefBonePose();
			TArray<FVector> Translations;
			for (FName Bone : { FName(TEXT("clavicle_l")), FName(TEXT("upperarm_l")), FName(TEXT("lowerarm_l")), FName(TEXT("hand_l")),
				FName(TEXT("clavicle_r")), FName(TEXT("upperarm_r")), FName(TEXT("lowerarm_r")), FName(TEXT("hand_r")) })
			{
				const int32 Index = Source->GetBoneIndex(Bone);
				if (!Pose.IsValidIndex(Index) || !Reference.IsValidIndex(Index) || Pose[Index].ContainsNaN()) { bInvalid = true; return; }
				const FVector Translation = Pose[Index].GetTranslation();
				const FVector ReferenceTranslation = Reference[Index].GetTranslation();
				const float Offset = FVector::Distance(Translation, ReferenceTranslation);
				if (Offset > MaxReferenceOffset) { MaxReferenceOffset = Offset; WorstOffsetBone = Bone; }
				MaxLengthError = FMath::Max(MaxLengthError, static_cast<float>(FMath::Abs(Translation.Size() - ReferenceTranslation.Size())));
				MaxScaleError = FMath::Max(MaxScaleError, static_cast<float>((Pose[Index].GetScale3D() - Reference[Index].GetScale3D()).GetAbsMax()));
				const int32 Ordinal = Translations.Add(Translation);
				if (PreviousTranslations.IsValidIndex(Ordinal)) MaxFrameStep = FMath::Max(MaxFrameStep, static_cast<float>(FVector::Distance(Translation, PreviousTranslations[Ordinal])));
			}
			PreviousTranslations = MoveTemp(Translations); ++Samples;
		}
		bool Preserved() const
		{
			// The approved twelve-clip target-mesh audit preserves these eight local offsets exactly.
			// Allow 1 mm in the actual graph/PIE pose for numerical evaluation, not altered anatomy.
			return Samples > 0 && !bInvalid && MaxReferenceOffset <= 0.1f && MaxScaleError <= UE_KINDA_SMALL_NUMBER;
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
		FMotionProbe Motion;
		FArmGeometry Arms;
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
			if (!EquipmentReady(Character, EquipmentSlot)) return false;
			FPeer& Peer = Peers.Add(World);
			Peer.Character = Character; Peer.Source = Mesh(Character); Peer.Animation = Mesh(Character)->GetAnimInstance();
			Peer.AbilitySystem = ASC(Character); Peer.Item = Item; Peer.bLateJoin = bLateJoin;
			if (!bLateJoin) Peer.NeutralPose = ReadPose(Mesh(Character));
			for (AActor* Actor : Item->GetSpawnedActors())
			{
				Peer.EquipmentActors.Add(Actor);
				UE_LOG(LogTemp, Display, TEXT("RpgMovingBlock equipment baseline world=%s frame=%llu role=%d late=%d item=%s actor=%s parent=%s expectedParent=%s"),
					*GetPathNameSafe(World), GFrameCounter, static_cast<int32>(Character->GetLocalRole()), bLateJoin,
					*GetPathNameSafe(Item), *GetPathNameSafe(Actor),
					*GetPathNameSafe(Actor && Actor->GetRootComponent() ? Actor->GetRootComponent()->GetAttachParent() : nullptr), *GetPathNameSafe(Peer.Source.Get()));
			}
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
		void BeginMotionProbe(bool bHeldBlock, FVector Direction, float Yaw, bool bFacing)
		{
			bMotionProbe = true; bMotionExpectedBlock = bHeldBlock; MotionDirection = Direction; MotionYaw = Yaw; bMotionFacing = bFacing;
			for (auto& Entry : Peers) Entry.Value.Motion = FMotionProbe();
		}
		void EndMotionProbe() { bMotionProbe = false; }
		bool MotionProbeComplete(int32 Count) const
		{
			if (Peers.Num() != Count) return false;
			for (const auto& Entry : Peers) if (!Entry.Value.Motion.Complete()) return false;
			return true;
		}
		void BeginBlockEpisode()
		{
			SetStage(EStage::MovingBlock);
			for (auto& Entry : Peers) Entry.Value.LoopInstance = INDEX_NONE;
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
				UE_LOG(LogTemp, Display, TEXT("RpgMovingBlock geometry stage=%s world=%s samples=%d invalid=%d maxRefOffsetCm=%.4f bone=%s maxLengthErrorCm=%.4f maxScaleError=%.6f maxFrameTranslationCm=%.4f"),
					Label, *GetPathNameSafe(Entry.Key.Get()), Peer.Arms.Samples, Peer.Arms.bInvalid, Peer.Arms.MaxReferenceOffset,
					*Peer.Arms.WorstOffsetBone.ToString(), Peer.Arms.MaxLengthError, Peer.Arms.MaxScaleError, Peer.Arms.MaxFrameStep);
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
			bool bEquipmentChanged = Item != Peer->Item.Get() || !Item || Item->GetSpawnedActors().Num() != Peer->EquipmentActors.Num();
			for (const TWeakObjectPtr<AActor>& Actor : Peer->EquipmentActors)
				bEquipmentChanged |= !Actor.IsValid() || !Actor->GetRootComponent() || Actor->GetRootComponent()->GetAttachParent() != Peer->Source.Get();
			if (bEquipmentChanged && !Peer->bEquipmentChanged)
			{
				UE_LOG(LogTemp, Display, TEXT("RpgMovingBlock first equipment violation world=%s frame=%llu role=%d late=%d item=%s expectedItem=%s actors=%d expectedActors=%d"),
					*GetPathNameSafe(World), GFrameCounter, static_cast<int32>(Character->GetLocalRole()), Peer->bLateJoin,
					*GetPathNameSafe(Item), *GetPathNameSafe(Peer->Item.Get()), Item ? Item->GetSpawnedActors().Num() : -1, Peer->EquipmentActors.Num());
				for (const TWeakObjectPtr<AActor>& Actor : Peer->EquipmentActors)
					UE_LOG(LogTemp, Display, TEXT("RpgMovingBlock first equipment violation actor=%s parent=%s expectedParent=%s"),
						*GetPathNameSafe(Actor.Get()), *GetPathNameSafe(Actor.IsValid() && Actor->GetRootComponent() ? Actor->GetRootComponent()->GetAttachParent() : nullptr),
						*GetPathNameSafe(Peer->Source.Get()));
			}
			Peer->bEquipmentChanged |= bEquipmentChanged;
			UAnimInstance* Animation = Peer->Animation.Get();
			Peer->bRootMotion |= Animation->GetRootMotionMontageInstance() != nullptr
				|| (Mover(Character) && Mover(Character)->FindActiveLayeredMoveByType(FRpgMoverAbilityRootMotion::StaticStruct()) != nullptr);
			Peer->bSawEnd |= EndClip.IsValid() && Animation->Montage_IsPlaying(EndClip.Get());
			const TArray<FQuat> Pose = ReadPose(Mesh(Character));
			Peer->bInvalidPose |= Pose.Num() != 9;
			Peer->Arms.Sample(Mesh(Character));
			Peer->bInvalidPose |= Peer->Arms.bInvalid;
			if (bMotionProbe) Peer->Motion.Sample(Character, DeltaSeconds, bMotionExpectedBlock, MotionDirection, MotionYaw, bMotionFacing, Pose);
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
		bool bMotionProbe = false, bMotionExpectedBlock = false, bMotionFacing = false;
		FVector MotionDirection = FVector::ZeroVector;
		float MotionYaw = 0.0f;
	};
	/** One deliberate prediction error, with the same Fixed restore/replay witness used by traversal tests. */
	class FBlockCorrection final
	{
	public:
		~FBlockCorrection() { Stop(); }
		bool DelayOwnerReceipts(APawn* Character, int32 Milliseconds = 150)
		{
#if DO_ENABLE_NET_TEST
			UNetDriver* Driver = Character && Character->GetWorld() ? Character->GetWorld()->GetNetDriver() : nullptr;
			if (!Driver || LagDriver.IsValid()) return false;
			LagDriver = Driver; OriginalPackets = Driver->PacketSimulationSettings;
			FPacketSimulationSettings Packets = OriginalPackets;
			// This leaves enough real input frames between injection and correction to cross RMB release.
			Packets.PktIncomingLagMin = Packets.PktIncomingLagMax = Milliseconds;
			Driver->SetPacketSimulationSettings(Packets);
			return true;
#else
			return false;
#endif
		}
		bool Inject(APawn* Character, UAnimMontage* Loop, bool bAcrossRelease, bool bObserveAuthorityCancellation = false)
		{
			if (bInjected || !Character || Character->GetLocalRole() != ROLE_AutonomousProxy || !Mover(Character) || !Blocking(Character)) return false;
			const UNetworkPredictionWorldManager* Prediction = Character->GetWorld()->GetSubsystem<UNetworkPredictionWorldManager>();
			UMoverNetworkPredictionLiaisonComponent* Backend = Character->FindComponentByClass<UMoverNetworkPredictionLiaisonComponent>();
			FMoverSyncState Sync;
			if (!Prediction || Prediction->GetSettings().PreferredTickingPolicy != ENetworkPredictionTickingPolicy::Fixed
				|| !Backend || !Backend->ReadPendingSyncState(Sync)) return false;
			FMoverDefaultSyncState* Default = Sync.SyncStateCollection.FindMutableDataByType<FMoverDefaultSyncState>();
			const FRpgMoverBlockMovementSyncState* BlockState = Sync.SyncStateCollection.FindDataByType<FRpgMoverBlockMovementSyncState>();
			FGameplayAbilitySpec* Ability = BlockSpec(Character);
			FAnimMontageInstance* Instance = Mesh(Character)->GetAnimInstance()->GetActiveInstanceForMontage(Loop);
			if (!Default || !BlockState || !BlockState->bBlocking || !Ability || !Ability->IsActive() || !Instance || Instance->GetWeight() < 0.99f) return false;
			Owner = Character; Liaison = Backend; Montage = Loop; InstanceId = Instance->GetInstanceID(); AbilityHandle = Ability->Handle;
			ActivationKey = Ability->GetPrimaryInstance()->GetCurrentActivationInfo().GetActivationPredictionKey();
			bCrossRelease = bAcrossRelease;
			bAuthorityCancellation = bObserveAuthorityCancellation;
			CrossDirection = Character->GetActorRightVector().GetSafeNormal2D();
			InjectedFrame = Prediction->GetFixedTickState().PendingFrame;
			// Authority cancellation creates its own real mismatch; that scenario does not inject a pose error.
			if (!bAuthorityCancellation)
			{
				Default->SetTransforms_WorldSpace(Default->GetLocation_WorldSpace() + CrossDirection * 50.0,
					Default->GetOrientation_WorldSpace(), Default->GetVelocity_WorldSpace(), Default->GetAngularVelocityDegrees_WorldSpace(), nullptr);
				if (!Backend->WritePendingSyncState(Sync)) return false;
				Mover(Character)->GetUpdatedComponent()->SetWorldLocation(Default->GetLocation_WorldSpace(), false, nullptr, ETeleportType::TeleportPhysics);
				if (UMoverBlackboard* Blackboard = Mover(Character)->GetSimBlackboard_Mutable())
				{
					Blackboard->Invalidate(CommonBlackboard::LastFloorResult);
					Blackboard->Invalidate(CommonBlackboard::LastFoundDynamicMovementBase);
				}
			}
			Observer.Reset(NewObject<URpgMoverRollbackTestObserver>());
			Observer->TrackPredictedFrame(Mover(Character), InjectedFrame, Sync, bAuthorityCancellation);
			Mover(Character)->OnPostSimulationRollback.AddDynamic(Observer.Get(), &URpgMoverRollbackTestObserver::ObserveRollback);
			BeforeHandle = FWorldDelegates::OnWorldTickStart.AddRaw(this, &FBlockCorrection::BeforeDispatch);
			AfterHandle = FWorldDelegates::OnWorldPreActorTick.AddRaw(this, &FBlockCorrection::AfterDispatch);
			bInjected = true;
			UE_LOG(LogTemp, Display, TEXT("RpgMovingBlock correction armed local=%d offset=%d lastServerFrame=%d crossRelease=%d authorityCancel=%d positionInjected=%d position=%s instance=%d"),
				InjectedFrame, Prediction->GetFixedTickState().Offset, Mover(Character)->GetLastTimeStep().ServerFrame,
				bCrossRelease, bAuthorityCancellation, !bAuthorityCancellation, *Default->GetLocation_WorldSpace().ToCompactString(), InstanceId);
			return true;
		}
		bool Observed() const { return bObserved; }
		bool Preserved() const { return bObserved && bSameHead && bBlockHistory && bAbilityPreserved; }
		void Stop()
		{
			FWorldDelegates::OnWorldTickStart.Remove(BeforeHandle); FWorldDelegates::OnWorldPreActorTick.Remove(AfterHandle);
			BeforeHandle.Reset(); AfterHandle.Reset();
			if (Owner.IsValid() && Mover(Owner.Get()) && Observer.IsValid())
				Mover(Owner.Get())->OnPostSimulationRollback.RemoveDynamic(Observer.Get(), &URpgMoverRollbackTestObserver::ObserveRollback);
			if (Observer.IsValid()) Observer->StopTrackingFrame();
			Observer.Reset(); BeforeSync = FMoverSyncState();
#if DO_ENABLE_NET_TEST
			if (LagDriver.IsValid()) LagDriver->SetPacketSimulationSettings(OriginalPackets);
#endif
			LagDriver.Reset();
		}
	private:
		static bool IsBlocking(const FMoverSyncState& Sync)
		{
			const FRpgMoverBlockMovementSyncState* BlockState = Sync.SyncStateCollection.FindDataByType<FRpgMoverBlockMovementSyncState>();
			return BlockState && BlockState->bBlocking;
		}
		void BeforeDispatch(UWorld* World, ELevelTick, float)
		{
			if (bObserved || !Owner.IsValid() || Owner->GetWorld() != World || !Liaison.IsValid() || !Observer.IsValid()) return;
			bBeforeValid = Liaison->ReadPendingSyncState(BeforeSync);
			BeforeCount = Observer->Count;
			BeforeClock = RpgMoverPredictionTests::FFixedPredictionHeadSnapshot::Capture(World, Liaison.Get());
			if (bBeforeValid) Observer->BeginDispatch(BeforeClock.LocalPendingFrame); else Observer->EndDispatch();
		}
		void AfterDispatch(UWorld* World, ELevelTick, float)
		{
			if (bObserved || !Owner.IsValid() || Owner->GetWorld() != World || !Liaison.IsValid() || !Observer.IsValid()) return;
			Observer->EndDispatch();
			if (Observer->Count > BeforeCount && DiagnosticEpochs++ < 12)
			{
				FMoverSyncState DiagnosticHead;
				const bool bHeadRead = Liaison->ReadPendingSyncState(DiagnosticHead);
				const FMoverDefaultSyncState* OldPosition = Observer->PredictedSync.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
				const FMoverDefaultSyncState* NewPosition = Observer->ReplacementSync.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
				UE_LOG(LogTemp, Display, TEXT("RpgMovingBlock correction epoch frame=%llu crossRelease=%d authorityCancel=%d callbacks=%d head=%d tracked=%d restored=%d replaced=%d hasReplacement=%d replaySteps=%d blockRestored=%d blockPredicted=%d blockReplacement=%d blockBefore=%d blockAfter=%d ownerLease=%d delta=%s predictedSpeed2D=%.3f replacementSpeed2D=%.3f deltaV=%s"),
					GFrameCounter, bCrossRelease, bAuthorityCancellation, Observer->Count - BeforeCount, BeforeClock.LocalPendingFrame,
					InjectedFrame, Observer->RestoredLocalFrame, Observer->ReplacedLocalFrame, Observer->bHasReplacement, Observer->ReplaySteps,
					IsBlocking(Observer->RestoredSync), IsBlocking(Observer->PredictedSync), IsBlocking(Observer->ReplacementSync),
					IsBlocking(BeforeSync), bHeadRead && IsBlocking(DiagnosticHead), ASC(Owner.Get())->IsBlockMovementActive(),
					OldPosition && NewPosition ? *(NewPosition->GetLocation_WorldSpace() - OldPosition->GetLocation_WorldSpace()).ToCompactString() : TEXT("unavailable"),
					OldPosition ? OldPosition->GetVelocity_WorldSpace().Size2D() : -1.0, NewPosition ? NewPosition->GetVelocity_WorldSpace().Size2D() : -1.0,
					OldPosition && NewPosition ? *(NewPosition->GetVelocity_WorldSpace() - OldPosition->GetVelocity_WorldSpace()).ToCompactString() : TEXT("unavailable"));
			}
			if (!bBeforeValid || Observer->Count <= BeforeCount || !Observer->bHasReplacement || Observer->ReplaySteps <= 0) return;
			const FMoverDefaultSyncState* Predicted = Observer->PredictedSync.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
			const FMoverDefaultSyncState* Replaced = Observer->ReplacementSync.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
			FMoverSyncState After;
			if (!Predicted || !Replaced || !Liaison->ReadPendingSyncState(After)) return;
			const FVector Delta = Replaced->GetLocation_WorldSpace() - Predicted->GetLocation_WorldSpace();
			const double PredictedSpeed = Predicted->GetVelocity_WorldSpace().Size2D();
			const double ReplacedSpeed = Replaced->GetVelocity_WorldSpace().Size2D();
			const double SpeedGain = ReplacedSpeed - PredictedSpeed;
			// Cancellation removes the Run cap immediately in velocity. Frequent legitimate
			// corrections need not accumulate a centimetre of positional error between epochs.
			if (bAuthorityCancellation ? (!FMath::IsFinite(SpeedGain) || SpeedGain < 1.0)
				: FVector::DotProduct(Delta, CrossDirection) > -1.0) return;
			if (bAuthorityCancellation && (Observer->ReplacedLocalFrame != Observer->RestoredLocalFrame
				|| !IsBlocking(Observer->PredictedSync) || IsBlocking(Observer->ReplacementSync))) return;
			// At F == R the replacement is only an authority snapshot; crossing release must
			// witness an actual replay output that consumed the owner's historical blocked input.
			if (bCrossRelease && !bAuthorityCancellation && Observer->ReplacedLocalFrame <= Observer->RestoredLocalFrame) return;
			const bool bReleasedHead = !IsBlocking(BeforeSync) && !IsBlocking(After) && !ASC(Owner.Get())->IsBlockMovementActive();
			// A release test must really replay old blocked history while today's GAS/head is already unblocked.
			if (bCrossRelease && !bReleasedHead) return;
			bObserved = true;
			bSameHead = BeforeClock.IsSameLocalHead(RpgMoverPredictionTests::FFixedPredictionHeadSnapshot::Capture(World, Liaison.Get()));
			// After release the authority may restore an already-unblocked R before the owner's
			// recorded blocked F. The real F replay must still reproduce its historical block input;
			// requiring the earlier authority snapshot itself to remain blocked is a different contract.
			bBlockHistory = bAuthorityCancellation
				? !IsBlocking(Observer->RestoredSync) && IsBlocking(Observer->PredictedSync) && !IsBlocking(Observer->ReplacementSync) && bReleasedHead
				: IsBlocking(Observer->PredictedSync) && IsBlocking(Observer->ReplacementSync)
					&& (bCrossRelease ? bReleasedHead : IsBlocking(Observer->RestoredSync) && IsBlocking(BeforeSync) && IsBlocking(After));
			const FGameplayAbilitySpec* Ability = BlockSpec(Owner.Get());
			const FAnimMontageInstance* Instance = Mesh(Owner.Get())->GetAnimInstance()->GetActiveInstanceForMontage(Montage.Get());
			bAbilityPreserved = Ability && Ability->Handle == AbilityHandle
				&& Ability->GetPrimaryInstance()->GetCurrentActivationInfo().GetActivationPredictionKey() == ActivationKey
				&& (bCrossRelease ? !Ability->IsActive() : Ability->IsActive() && Instance && Instance->GetInstanceID() == InstanceId);
			UE_LOG(LogTemp, Display, TEXT("RpgMovingBlock correction observed crossRelease=%d authorityCancel=%d tracked=%d restored=%d replaced=%d replaySteps=%d delta=%s predictedSpeed2D=%.3f replacementSpeed2D=%.3f speedGain=%.3f deltaV=%s sameHead=%d expectedHistory=%d releasedHead=%d abilityPreserved=%d"),
				bCrossRelease, bAuthorityCancellation, InjectedFrame, Observer->RestoredLocalFrame, Observer->ReplacedLocalFrame, Observer->ReplaySteps, *Delta.ToCompactString(),
				PredictedSpeed, ReplacedSpeed, SpeedGain, *(Replaced->GetVelocity_WorldSpace() - Predicted->GetVelocity_WorldSpace()).ToCompactString(),
				bSameHead, bBlockHistory, bReleasedHead, bAbilityPreserved);
		}
		TWeakObjectPtr<APawn> Owner;
		TWeakObjectPtr<UMoverNetworkPredictionLiaisonComponent> Liaison;
		TWeakObjectPtr<UAnimMontage> Montage;
		TWeakObjectPtr<UNetDriver> LagDriver;
#if DO_ENABLE_NET_TEST
		FPacketSimulationSettings OriginalPackets;
#endif
		TStrongObjectPtr<URpgMoverRollbackTestObserver> Observer;
		FDelegateHandle BeforeHandle, AfterHandle;
		FMoverSyncState BeforeSync;
		RpgMoverPredictionTests::FFixedPredictionHeadSnapshot BeforeClock;
		FGameplayAbilitySpecHandle AbilityHandle;
		FPredictionKey ActivationKey;
		FVector CrossDirection = FVector::ZeroVector;
		int32 BeforeCount = 0, InjectedFrame = INDEX_NONE, InstanceId = INDEX_NONE, DiagnosticEpochs = 0;
		bool bInjected = false, bObserved = false, bCrossRelease = false, bBeforeValid = false;
		bool bAuthorityCancellation = false;
		bool bSameHead = false, bBlockHistory = false, bAbilityPreserved = false;
	};
	struct FState : FBasePIENetworkComponentState {};
}

NETWORK_TEST_CLASS(GaspMovingBlockPIE, "SurvivalRpg.GASP.MovingBlock")
{
	using FState = RpgGaspMovingBlockTests::FState;
	using EVariant = RpgGaspMovingBlockTests::EVariant;
	using EStage = RpgGaspMovingBlockTests::EStage;
	using EScenario = RpgGaspMovingBlockTests::EScenario;
	RpgGaspMovingBlockTests::FScopedWorld Isolation;
	RpgGaspMovingBlockTests::FScopedInput Input;
	RpgGaspMovingBlockTests::FScopedObservations Observations;
	RpgGaspMovingBlockTests::FBlockCorrection HeldCorrection, ReleaseCorrection;
	TStrongObjectPtr<UAnimMontage> BlockStart, BlockLoop, BlockEnd;
	// Construct after selecting the variant, before CQTest starts its isolated PIE worlds.
	TUniquePtr<FPIENetworkComponent<FState>> Network;
	FPrimaryAssetId PreviousExperience;
	EVariant Variant = EVariant::CMC;
	ERpgEquipmentSlot BlockSlot = ERpgEquipmentSlot::None;
	int32 SubjectId = INDEX_NONE;
	bool bConfigured = false;
	float InitialControlYaw = 0.0f, InitialCameraYaw = 0.0f, ProbeYaw = 0.0f, SprintBaseline = 0.0f;
	FVector ProbeDirection = FVector::ZeroVector;
	int32 CancelServerFrame = INDEX_NONE, CancelSamples = 0;
	float MaximumAuthoritySpeedBeforeReceipt = 0.0f;
	bool bCancelGaitMismatch = false, bCancelWindowExpired = false;

	AFTER_EACH()
	{
		Input.Stop();
		HeldCorrection.Stop(); ReleaseCorrection.Stop();
		if (TestRunner->HasAnyErrors()) Observations.Report(TEXT("Failure"));
		Observations.Stop();
		if (bConfigured) GetMutableDefault<URpgDeveloperSettings>()->ExperienceOverride = PreviousExperience;
	}
	TEST_METHOD(CMCMovingBlockKeepsLegsAnimatedThroughLateJoinAndRelease) { Queue(EVariant::CMC); }
	TEST_METHOD(MoverMovingBlockKeepsLegsAnimatedThroughLateJoinAndRelease) { Queue(EVariant::Mover); }
	TEST_METHOD(CMCBlockFacesCameraAndPreservesDirectionalMovementSpeed) { Queue(EVariant::CMC, EScenario::DirectionalFacing); }
	TEST_METHOD(MoverBlockFacesCameraAndPreservesDirectionalMovementSpeed) { Queue(EVariant::Mover, EScenario::DirectionalFacing); }
	TEST_METHOD(MoverBlockPreventsSprintInBothInputOrdersAndResumesHeldSprintOnRelease) { Queue(EVariant::Mover, EScenario::SprintOrdering); }
	TEST_METHOD(MoverFixedRollbackPreservesHeldBlockAndReplaysBlockedHistoryAcrossRelease) { Queue(EVariant::Mover, EScenario::FixedCorrection); }
	TEST_METHOD(MoverAuthorityCancelUsesRawSprintWhileOwnerStillPredictsBlockAndThenCorrects) { Queue(EVariant::Mover, EScenario::AuthorityCancel); }

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
			TestRunner->TestTrue(TEXT("All eight arm joints preserve target translation and scale throughout observed transitions"), Peer.Arms.Preserved());
			for (int32 Index = 0; Index < 2; ++Index)
				TestRunner->TestTrue(FString::Printf(TEXT("%s %s window %d: both legs animate in the completed local pose"),
					Label, *GetPathNameSafe(Entry.Key.Get()), Index), Peer.Measure.Windows[Index].LegsMoved());
		}
	}
	void Queue(EVariant SelectedVariant, EScenario Scenario = EScenario::LegLifecycle)
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
			.ThenClient(TEXT("Observe the owner before the block starts"), 0, [this, Scenario](FState& PeerState)
				{ ASSERT_THAT(IsTrue(Isolation.IsIsolated(PeerState.World) && Observations.Add(PeerState.World))); Input.Move(Scenario == EScenario::LegLifecycle); });
		if (Scenario == EScenario::DirectionalFacing) { QueueDirectionalFacing(); return; }
		if (Scenario == EScenario::SprintOrdering) { QueueSprintOrdering(); return; }
		if (Scenario == EScenario::FixedCorrection) { QueueFixedCorrection(); return; }
		if (Scenario == EScenario::AuthorityCancel) { QueueAuthorityCancel(); return; }
		Network->UntilServer(TEXT("Normal movement produces two completed pose windows on owner and authority"), [this](FState&)
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
				return Ready(PeerState.World, Character, Variant) && Character->GetLocalRole() == ROLE_SimulatedProxy
					&& Blocking(Character) && ASC(Character)->GetCurrentMontage() == BlockLoop.Get()
					&& EquipmentReady(Character, BlockSlot);
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
	float NormalSpeed() const { return Variant == EVariant::CMC ? 600.0f : 375.0f; }
	void QueueMotion(const FString& Label, FVector2D Keys, bool bHeldBlock, bool bFacing, int32 Count,
		bool bCaptureSprint = false, bool bUseSprintBaseline = false)
	{
		using namespace RpgGaspMovingBlockTests;
		// CQTest retains description pointers until latent execution; use static labels here.
		Network->ThenClient(TEXT("Send actual directional keys for the next movement probe"), 0,
			[this, Keys, bHeldBlock, bFacing](FState&)
			{
				const APlayerController* Controller = Input.Subject() ? Cast<APlayerController>(Input.Subject()->GetController()) : nullptr;
				ASSERT_THAT(IsNotNull(Controller)); if (!Controller) return;
				ProbeYaw = Controller->GetControlRotation().Yaw;
				ProbeDirection = FRotator(0.0f, ProbeYaw, 0.0f).RotateVector(FVector(Keys.Y, Keys.X, 0.0)).GetSafeNormal2D();
				Input.MoveKeys(Keys);
				Observations.BeginMotionProbe(bHeldBlock, ProbeDirection, ProbeYaw, bFacing);
			})
			.UntilServer(TEXT("Collect bounded steady movement samples on every role"),
				[this, Count](FState&) { return Observations.MotionProbeComplete(Count); }, Timeout())
			.ThenServer(TEXT("Verify actual movement and facing for this probe"),
				[this, Label, Keys, bHeldBlock, bFacing, Count, bCaptureSprint, bUseSprintBaseline](FState&)
			{
				Observations.EndMotionProbe(); Observations.Report(*Label);
				TestRunner->TestEqual(TEXT("Every expected network role contributed movement samples"), Observations.GetPeers().Num(), Count);
				if (bCaptureSprint)
					for (const auto& Entry : Observations.GetPeers())
						if (Entry.Value.Character.IsValid() && Entry.Value.Character->HasAuthority()) SprintBaseline = Entry.Value.Motion.MeanSpeed();
				const float ExpectedSpeed = Keys.IsNearlyZero() ? 0.0f : (bUseSprintBaseline || bCaptureSprint ? SprintBaseline : NormalSpeed());
				const float SpeedTolerance = ExpectedSpeed > 0.0f ? FMath::Max(20.0f, ExpectedSpeed * 0.05f) : 5.0f;
				if (bCaptureSprint || bUseSprintBaseline)
					TestRunner->TestTrue(TEXT("Real unblocked Shift input has positively demonstrated faster sprint movement"), SprintBaseline > NormalSpeed() + 100.0f);
				for (const auto& Entry : Observations.GetPeers())
				{
					const FPeer& Peer = Entry.Value;
					const FMotionProbe& Probe = Peer.Motion;
					UE_LOG(LogTemp, Display, TEXT("RpgMovingBlock motion stage=%s world=%s role=%d samples=%d speed=%.2f/%.2f/%.2f expected=%.2f yawError=%.3f directionDot=%.4f distance=%.2f wrongBlock=%d offGround=%d"),
						*Label, *GetPathNameSafe(Entry.Key.Get()), static_cast<int32>(Peer.Character->GetLocalRole()), Probe.Samples,
						Probe.MinimumSpeed, Probe.MeanSpeed(), Probe.MaximumSpeed, ExpectedSpeed, Probe.MaximumYawError, Probe.MinimumDirectionDot,
						FVector::Distance(Probe.FirstLocation, Probe.LastLocation), Probe.bWrongBlockState, Probe.bLostGround);
					TestRunner->TestTrue(TEXT("Movement samples retain the expected GAS and finalized movement block state and ground support"), !Probe.bWrongBlockState && !Probe.bLostGround);
					TestRunner->TestTrue(TEXT("The same directional input preserves the ordinary movement speed without sprint leakage"),
						Probe.MinimumSpeed >= ExpectedSpeed - SpeedTolerance && Probe.MaximumSpeed <= ExpectedSpeed + SpeedTolerance);
					if (bFacing) TestRunner->TestTrue(TEXT("After settling, the blocked body faces camera yaw independently of travel direction"), Probe.MaximumYawError <= 10.0f);
					if (ExpectedSpeed > 0.0f)
					{
						TestRunner->TestTrue(TEXT("Real displacement and velocity follow the camera-relative WASD direction"),
							Probe.MinimumDirectionDot > 0.95f && FVector::DotProduct(Probe.LastLocation - Probe.FirstLocation, ProbeDirection) > ExpectedSpeed * 0.5f);
						TestRunner->TestTrue(TEXT("Both legs actually animate during directional movement"), Probe.Legs.LegsMoved());
					}
					TestRunner->TestTrue(TEXT("Input changes preserve the gameplay mesh, ASC and equipment attachments"), !Peer.bSourceChanged && !Peer.bEquipmentChanged && !Peer.bRootMotion && !Peer.bInvalidPose);
					TestRunner->TestTrue(TEXT("Camera and directional transitions preserve all eight target arm joint offsets and scales"), Peer.Arms.Preserved());
					if (!bHeldBlock) TestRunner->TestTrue(TEXT("The unblocked sample has cleared actual block presentation"), Observations.Released(Peer.Character.Get()));
				}
			});
	}
	void QueueLateJoin()
	{
		using namespace RpgGaspMovingBlockTests;
		Network->ThenClientJoins()
			.UntilClient(TEXT("Late join receives the same already-held block"), 1, [this](FState& PeerState)
			{
				APawn* Character = Pawn(PeerState.World, SubjectId);
				return Ready(PeerState.World, Character, Variant) && Character->GetLocalRole() == ROLE_SimulatedProxy
					&& Blocking(Character) && ASC(Character)->GetCurrentMontage() == BlockLoop.Get()
					&& EquipmentReady(Character, BlockSlot);
			}, Timeout())
			.ThenClient(TEXT("Observe the late simulated proxy without a second block press"), 1, [this](FState& PeerState)
				{ ASSERT_THAT(IsTrue(Isolation.IsIsolated(PeerState.World) && Observations.Add(PeerState.World, true))); });
	}
	void QueueLookWhileBlocking()
	{
		using namespace RpgGaspMovingBlockTests;
		Network->ThenClient(TEXT("Turn the real camera while keeping RMB held"), 0, [this](FState&)
			{
				const APlayerController* Controller = Input.Subject() ? Cast<APlayerController>(Input.Subject()->GetController()) : nullptr;
				ASSERT_THAT(IsTrue(Controller && Controller->PlayerCameraManager)); if (!Controller || !Controller->PlayerCameraManager) return;
				InitialControlYaw = Controller->GetControlRotation().Yaw; InitialCameraYaw = Controller->PlayerCameraManager->GetCameraRotation().Yaw;
				Input.Look(0.5f);
			})
			.UntilClient(TEXT("Both control yaw and the rendered camera respond to actual look input"), 0, [this](FState&)
			{
				const APlayerController* Controller = Input.Subject() ? Cast<APlayerController>(Input.Subject()->GetController()) : nullptr;
				return Controller && Controller->PlayerCameraManager
					&& FMath::Abs(FMath::FindDeltaAngleDegrees(InitialControlYaw, Controller->GetControlRotation().Yaw)) > 45.0f
					&& FMath::Abs(FMath::FindDeltaAngleDegrees(InitialCameraYaw, Controller->PlayerCameraManager->GetCameraRotation().Yaw)) > 40.0f;
			}, Timeout())
			.ThenClient(TEXT("Stop only the look axis, preserving held block"), 0, [this](FState&) { Input.Look(0.0f); });
	}
	void QueuePolicyCleanup(int32 ExpectedEpisodes, int32 Count, bool bAuthorityCancelled = false)
	{
		using namespace RpgGaspMovingBlockTests;
		Network->ThenClient(TEXT("Release all movement and sprint keys"), 0, [this](FState&) { Input.MoveKeys(FVector2D::ZeroVector); Input.Sprint(false); })
			.UntilServer(TEXT("All policy-test peers return to supported idle and clear the block presentation"), [this, Count](FState&)
			{
				if (Observations.GetPeers().Num() != Count) return false;
				for (const auto& Entry : Observations.GetPeers())
					if (!Grounded(Entry.Value.Character.Get()) || Speed(Entry.Value.Character.Get()) >= 5.0f || !Observations.Released(Entry.Value.Character.Get())) return false;
				return true;
			}, Timeout())
			.ThenServer(TEXT("Every real block episode ends with the requested lifecycle and source ownership"), [this, ExpectedEpisodes, bAuthorityCancelled](FState&)
			{
				Observations.Report(TEXT("PolicyCleanup"));
				for (const auto& Entry : Observations.GetPeers())
				{
					const FPeer& Peer = Entry.Value;
					TestRunner->TestTrue(TEXT("Policy transitions preserve mesh and equipment and do not restart a held montage"), !Peer.bSourceChanged && !Peer.bEquipmentChanged && !Peer.bLoopReplaced);
					TestRunner->TestTrue(TEXT("The complete policy lifecycle preserves target arm joint translations and scales"), Peer.Arms.Preserved());
					if (!Peer.bLateJoin)
					{
						TestRunner->TestEqual(TEXT("Real block activations match the requested input episodes"), Peer.Activations, ExpectedEpisodes);
						TestRunner->TestEqual(TEXT("Every block ends through real input release"), Peer.Ends, ExpectedEpisodes);
						TestRunner->TestEqual(TEXT("Only the deliberately cancelled authority activation reports cancellation"), Peer.CancelledEnds,
							bAuthorityCancelled && Peer.Character->HasAuthority() ? 1 : 0);
					}
				}
			});
	}
	void QueueDirectionalFacing()
	{
		using namespace RpgGaspMovingBlockTests;
		const FVector2D Directions[] = { {0, 1}, {1, 0}, {0, -1}, {-1, 0}, {1, 1} };
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Directions); ++Index)
			QueueMotion(FString::Printf(TEXT("NormalDirection%d"), Index), Directions[Index], false, false, 2);
		Network->ThenClient(TEXT("Enter block from ordinary WASD input and settle at rest"), 0, [this](FState&)
			{ Input.MoveKeys(FVector2D::ZeroVector); Observations.BeginBlockEpisode(); Input.Block(true); });
		QueueMotion(TEXT("BlockIdleBeforeLook"), FVector2D::ZeroVector, true, true, 2);
		QueueLookWhileBlocking();
		QueueMotion(TEXT("BlockIdleAfterLook"), FVector2D::ZeroVector, true, true, 2);
		QueueLateJoin();
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Directions); ++Index)
			QueueMotion(FString::Printf(TEXT("BlockedDirection%d"), Index), Directions[Index], true, true, 3);
		Network->ThenClient(TEXT("Release block while preserving directional movement"), 0, [this](FState&) { Input.Block(false); });
		QueueMotion(TEXT("ReleasedDirection"), FVector2D(1, 0), false, false, 3);
		QueuePolicyCleanup(1, 3);
	}
	void QueueSprintOrdering()
	{
		using namespace RpgGaspMovingBlockTests;
		QueueMotion(TEXT("RunBeforeSprint"), FVector2D(0, 1), false, false, 2);
		Network->ThenClient(TEXT("Press real Shift while moving without block"), 0, [this](FState&) { Input.Sprint(true); });
		QueueMotion(TEXT("PositiveSprintBaseline"), FVector2D(0, 1), false, false, 2, true);
		Network->ThenClient(TEXT("Enter block while sprint input remains held"), 0, [this](FState&) { Observations.BeginBlockEpisode(); Input.Block(true); });
		QueueMotion(TEXT("SprintThenBlock"), FVector2D(0, 1), true, true, 2);
		QueueLateJoin();
		QueueMotion(TEXT("LateJoinSprintSuppressed"), FVector2D(0, 1), true, true, 3);
		Network->ThenClient(TEXT("Release only block while Shift remains held"), 0, [this](FState&) { Input.Block(false); });
		QueueMotion(TEXT("HeldSprintResumesAfterFirstRelease"), FVector2D(0, 1), false, false, 3, false, true);
		Network->ThenClient(TEXT("Release sprint before the next independent block activation"), 0, [this](FState&) { Input.Sprint(false); });
		QueueMotion(TEXT("RunBetweenBlockEpisodes"), FVector2D(0, 1), false, false, 3);
		Network->ThenClient(TEXT("Enter block before pressing Shift"), 0, [this](FState&) { Observations.BeginBlockEpisode(); Input.Block(true); });
		QueueMotion(TEXT("BlockBeforeSprint"), FVector2D(0, 1), true, true, 3);
		Network->ThenClient(TEXT("Press real Shift during the existing block activation"), 0, [this](FState&) { Input.Sprint(true); });
		QueueMotion(TEXT("BlockThenSprint"), FVector2D(0, 1), true, true, 3);
		Network->ThenClient(TEXT("Release only the second block with Shift still held"), 0, [this](FState&) { Input.Block(false); });
		QueueMotion(TEXT("HeldSprintResumesAfterSecondRelease"), FVector2D(0, 1), false, false, 3, false, true);
		QueuePolicyCleanup(2, 3);
	}
	void QueueFixedCorrection()
	{
		using namespace RpgGaspMovingBlockTests;
		QueueMotion(TEXT("CorrectionRunBaseline"), FVector2D(0, 1), false, false, 2);
		Network->ThenClient(TEXT("Retain real sprint input and temporarily delay only this owner's incoming packets"), 0, [this](FState&)
			{ ASSERT_THAT(IsTrue(ReleaseCorrection.DelayOwnerReceipts(Input.Subject()))); Input.Sprint(true); });
		QueueMotion(TEXT("CorrectionSprintBaseline"), FVector2D(0, 1), false, false, 2, true);
		Network->ThenClient(TEXT("Block the still-sprinting input through the real ability"), 0, [this](FState&)
			{ Observations.BeginBlockEpisode(); Input.Block(true); });
		QueueMotion(TEXT("CorrectionHeldBaseline"), FVector2D(0, 1), true, true, 2);
		Network->ThenClient(TEXT("Inject one coherent owner prediction error during the same active block"), 0, [this](FState&)
			{ ASSERT_THAT(IsTrue(HeldCorrection.Inject(Input.Subject(), BlockLoop.Get(), false))); })
			.UntilClient(TEXT("A real Fixed restore/resimulation replaces the affected blocked history frame"), 0,
				[this](FState&) { return HeldCorrection.Observed(); }, Timeout())
			.ThenClient(TEXT("Rollback preserves the same active block, instance and simulation head"), 0,
				[this](FState&) { ASSERT_THAT(IsTrue(HeldCorrection.Preserved())); });
		QueueMotion(TEXT("AfterHeldCorrection"), FVector2D(0, 1), true, true, 2);
		Network->ThenClient(TEXT("Inject a second prediction error then release only RMB before its correction arrives"), 0, [this](FState&)
			{ ASSERT_THAT(IsTrue(ReleaseCorrection.Inject(Input.Subject(), BlockLoop.Get(), true))); Input.Block(false); })
			.UntilClient(TEXT("Fixed replay reconstructs old blocked history while today's head and GAS activation are already released"), 0,
				[this](FState&) { return ReleaseCorrection.Observed(); }, Timeout())
			.ThenClient(TEXT("Release-crossing rollback preserves historical block and the newer unblocked head"), 0,
				[this](FState&) { ASSERT_THAT(IsTrue(ReleaseCorrection.Preserved())); });
		QueueMotion(TEXT("HeldSprintAfterReleaseCorrection"), FVector2D(0, 1), false, false, 2, false, true);
		QueuePolicyCleanup(1, 2);
	}
	void QueueAuthorityCancel()
	{
		using namespace RpgGaspMovingBlockTests;
		QueueMotion(TEXT("AuthorityCancelRunBaseline"), FVector2D(0, 1), false, false, 2);
		Network->ThenClient(TEXT("Delay only the owner's real cancellation receipt and hold sprint"), 0, [this](FState&)
			{ ASSERT_THAT(IsTrue(ReleaseCorrection.DelayOwnerReceipts(Input.Subject(), 400))); Input.Sprint(true); });
		QueueMotion(TEXT("AuthorityCancelSprintBaseline"), FVector2D(0, 1), false, false, 2, true);
		Network->ThenClient(TEXT("Enter predicted block with unchanged held Shift"), 0, [this](FState&)
			{ Observations.BeginBlockEpisode(); Input.Block(true); });
		QueueMotion(TEXT("AuthorityCancelHeldBaseline"), FVector2D(0, 1), true, true, 2);
		Network->ThenClient(TEXT("Passively retain the owner's real blocked prediction history before server cancellation"), 0, [this](FState&)
			{ ASSERT_THAT(IsTrue(ReleaseCorrection.Inject(Input.Subject(), BlockLoop.Get(), true, true))); })
			.ThenServer(TEXT("Authority cancels the actual granted block while the owner has not received it"), [this](FState& PeerState)
			{
				APawn* Character = Pawn(PeerState.World, SubjectId);
				FGameplayAbilitySpec* Ability = BlockSpec(Character);
				ASSERT_THAT(IsTrue(Ability && Ability->IsActive() && ASC(Input.Subject())->IsBlockMovementActive()));
				if (!Ability) return;
				CancelServerFrame = Mover(Character)->GetLastTimeStep().ServerFrame;
				ASC(Character)->CancelAbilityHandle(Ability->Handle);
				ASSERT_THAT(IsFalse(ASC(Character)->IsBlockMovementActive()));
			})
			.UntilServer(TEXT("Three actual server frames consume raw sprint while the owner still has blocked history"), [this](FState& PeerState)
			{
				APawn* Character = Pawn(PeerState.World, SubjectId);
				APawn* Owner = Input.Subject();
				if (!Mover(Character) || !ASC(Owner)) return false;
				if (!ASC(Owner)->IsBlockMovementActive())
				{
					UE_LOG(LogTemp, Display, TEXT("RpgMovingBlock authorityCancel staleWindow expired samples=%d maximumAuthoritySpeed=%.2f requiredAbove=%.2f gaitMismatch=%d"),
						CancelSamples, MaximumAuthoritySpeedBeforeReceipt, NormalSpeed() + 10.0f, bCancelGaitMismatch);
					bCancelWindowExpired = true;
					return true;
				}
				const int32 Frame = Mover(Character)->GetLastTimeStep().ServerFrame;
				if (Frame == CancelServerFrame) return false;
				CancelServerFrame = Frame;
				const FRpgMoverBlockMovementSyncState* AuthorityBlock = Mover(Character)->GetSyncState().SyncStateCollection.FindDataByType<FRpgMoverBlockMovementSyncState>();
				const FMoverInputCmdContext* RawOwnerInput = PendingInput(Owner);
				const FRpgMoverAbilityRootMotionInputs* OwnerHistory = RawOwnerInput ? RawOwnerInput->InputCollection.FindDataByType<FRpgMoverAbilityRootMotionInputs>() : nullptr;
				if (!AuthorityBlock || AuthorityBlock->bBlocking || !OwnerHistory || !OwnerHistory->bBlocking) return false;
				const FString RawGait = InputGait(RawOwnerInput);
				const FString ServerGait = InputGait(&Mover(Character)->GetLastInputCmd());
				bCancelGaitMismatch |= RawGait != TEXT("Sprint") || ServerGait != TEXT("Sprint");
				MaximumAuthoritySpeedBeforeReceipt = FMath::Max(MaximumAuthoritySpeedBeforeReceipt, Speed(Character));
				++CancelSamples;
				UE_LOG(LogTemp, Display, TEXT("RpgMovingBlock authorityCancel staleWindow sample=%d serverFrame=%d ownerLease=%d ownerHistory=%d authoritySync=%d rawGait=%s serverGait=%s serverSpeed=%.2f"),
					CancelSamples, Frame, ASC(Owner)->IsBlockMovementActive(), OwnerHistory->bBlocking, AuthorityBlock->bBlocking, *RawGait, *ServerGait, Speed(Character));
				// Frame cadence and smoothing vary: require the actual acceleration witness inside the
				// still-predicted window, rather than assuming that exactly three frames already suffice.
				return CancelSamples >= 3 && MaximumAuthoritySpeedBeforeReceipt > NormalSpeed() + 10.0f;
			}, Timeout())
			.ThenClient(TEXT("Release the actual owner RMB before delayed cancellation arrives; leave Shift held"), 0,
				[this](FState&) { Input.Block(false); })
			.ThenServer(TEXT("Authority ignored the stale client block state without consuming its raw sprint intent"), [this](FState&)
			{
				TestRunner->TestTrue(TEXT("The witness contains at least three real authority frames before owner cancellation receipt"), !bCancelWindowExpired && CancelSamples >= 3);
				TestRunner->TestFalse(TEXT("Both actual raw owner input and unblocked server simulation retain Sprint"), bCancelGaitMismatch);
				// Sprint accelerates at 300 cm/s²; the deliberately short pre-receipt window cannot reach full sprint.
				TestRunner->TestTrue(TEXT("Authority has really accelerated above blocked Run before owner cancellation receipt"), MaximumAuthoritySpeedBeforeReceipt > NormalSpeed() + 10.0f);
			})
			.UntilClient(TEXT("Actual Fixed reconciliation replaces a predicted blocked frame with authoritative cancellation"), 0,
				[this](FState&) { return ReleaseCorrection.Observed(); }, Timeout())
			.ThenClient(TEXT("Cancellation correction has frame-matched history, replay and a released current head"), 0,
				[this](FState&) { ASSERT_THAT(IsTrue(ReleaseCorrection.Preserved())); });
		QueueMotion(TEXT("SprintAfterAuthorityCancelCorrection"), FVector2D(0, 1), false, false, 2, false, true);
		QueuePolicyCleanup(1, 2, true);
	}
};

#endif
#endif
