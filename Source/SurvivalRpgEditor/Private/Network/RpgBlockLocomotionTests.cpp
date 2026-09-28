// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Components/PIENetworkComponent.h"
#include "Network/RpgCombatNetworkTestTypes.h"
#include "Network/RpgMoverPredictionTestHelpers.h"
#include "Network/RpgMoverPredictionTestTypes.h"
#include "Animation/AnimInstance.h"
#include "SurvivalRpg/Animation/RpgAnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/PlayerCameraManager.h"
#include "Backends/MoverNetworkPredictionLiaison.h"
#include "DefaultMovementSet/Modes/SmoothWalkingMode.h"
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
#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility_Stagger.h"
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

namespace RpgBlockLocomotionTests
{
	constexpr TCHAR SourceMeshPath[] = TEXT("/Game/SurvivalRpg/Characters/GASP/Shared/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin.SKM_UEFN_Mannequin");
	enum class EVariant : uint8 { CMC, Mover };
	enum class EScenario : uint8 { DirectionalFacing, SprintOrdering, FixedCorrection, AuthorityCancel, CapSnapshot, LayerLifecycle, Reactions };
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
	bool ProfileReady(APawn* Character)
	{
		const URpgEquipmentManagerComponent* Manager = Equipment(Character);
		const URpgWeaponInstance* Item = Manager ? Cast<URpgWeaponInstance>(Manager->GetActiveBlockSource()) : nullptr;
		const UAnimInstance* Layer = Manager ? Manager->GetBlockLocomotionLayerInstance() : nullptr;
		return Item && Item->GetBlockDefinition().BlockLocomotionLayer && Layer
			&& Layer->GetClass() == Item->GetBlockDefinition().BlockLocomotionLayer.Get()
			&& EquipmentReady(Character, Item->GetEquippedSlot());
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
				UE_LOG(LogTemp, Display, TEXT("RpgBlockLocomotion unexpected input mode=%s controller=%s"),
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
			UE_LOG(LogTemp, Display, TEXT("RpgBlockLocomotion isolated PIE join focus world=%s originalDefaultFlush=%d controllerFlush=%d"),
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
			if (Controller.IsValid()) UE_LOG(LogTemp, Display, TEXT("RpgBlockLocomotion fixture RMB world=%s frame=%llu requestedHold=%d rawBefore=%d"),
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
				UE_LOG(LogTemp, Display, TEXT("RpgBlockLocomotion restored PIE focus policy defaultFlush=%d"), bOriginalFocusFlush);
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
				UE_LOG(LogTemp, Display, TEXT("RpgBlockLocomotion unexpected held RMB release world=%s frame=%llu specPressed=%d active=%d controllerFlush=%d defaultFlush=%d"),
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
		for (const FName Bone : { FName(TEXT("thigh_l")), FName(TEXT("calf_l")), FName(TEXT("thigh_r")), FName(TEXT("calf_r")) })
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
			if (bComplete || Pose.Num() != 4 || (LastSampleTime >= 0.0 && Now - LastSampleTime < 0.05)) return;
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
	/** The normalized target clips preserve these local joint offsets; rotations remain entirely authored. */
	struct FArmJointWindow
	{
		int32 Samples = 0;
		double FirstTime = -1.0, LastTime = -1.0;
		float MaximumTranslationError = 0.f;
		bool bInvalid = false;
		void Sample(APawn* Character, USkeletalMeshComponent* Source, const UAnimInstance* Layer)
		{
			const FNumericProperty* AlphaProperty = Layer ? FindFProperty<FNumericProperty>(Layer->GetClass(), TEXT("LayerAlpha")) : nullptr;
			if (!AlphaProperty || !AlphaProperty->IsFloatingPoint() || !Source || !Source->GetSkeletalMeshAsset()) { bInvalid = true; return; }
			const double Alpha = AlphaProperty->GetFloatingPointPropertyValue(AlphaProperty->ContainerPtrToValuePtr<void>(Layer));
			if (!FMath::IsFinite(Alpha)) { bInvalid = true; return; }
			if (Alpha < 0.999) return;
			const TArray<FTransform> Pose = Source->GetBoneSpaceTransforms();
			const FReferenceSkeleton& Reference = Source->GetSkeletalMeshAsset()->GetRefSkeleton();
			for (const FName Bone : { FName(TEXT("clavicle_l")), FName(TEXT("upperarm_l")), FName(TEXT("lowerarm_l")),
				FName(TEXT("clavicle_r")), FName(TEXT("upperarm_r")), FName(TEXT("lowerarm_r")) })
			{
				const int32 Index = Reference.FindBoneIndex(Bone);
				if (!Pose.IsValidIndex(Index) || !Reference.GetRefBonePose().IsValidIndex(Index)
					|| Pose[Index].ContainsNaN() || !Pose[Index].GetRotation().IsNormalized()) { bInvalid = true; return; }
				const float Error = static_cast<float>(FVector::Distance(Pose[Index].GetTranslation(), Reference.GetRefBonePose()[Index].GetTranslation()));
				// This excludes pelvis/root motion and does not constrain shoulder angles, IK rotations or cosmetic pose choice.
				// Derived target clips measure <1e-13 cm offset; 0.2 cm allows numerical evaluation noise, not detached joints.
				if (!FMath::IsFinite(Error)) { bInvalid = true; return; }
				if (Error > 0.2f && MaximumTranslationError <= 0.2f)
					UE_LOG(LogTemp, Display, TEXT("RpgBlockLocomotion armJointViolation world=%s role=%d frame=%llu bone=%s alpha=%.4f offsetCm=%.6f local=%s reference=%s"),
						*GetPathNameSafe(Character->GetWorld()), static_cast<int32>(Character->GetLocalRole()), GFrameCounter, *Bone.ToString(), Alpha, Error,
						*Pose[Index].GetTranslation().ToCompactString(), *Reference.GetRefBonePose()[Index].GetTranslation().ToCompactString());
				MaximumTranslationError = FMath::Max(MaximumTranslationError, Error);
			}
			LastTime = Character->GetWorld()->GetTimeSeconds();
			if (Samples++ == 0) FirstTime = LastTime;
		}
		bool Preserved() const { return !bInvalid && Samples >= 6 && LastTime - FirstTime >= 0.35 && MaximumTranslationError <= 0.2f; }
	};
	/** Samples steady movement after a bounded settling interval; failed samples cannot be retried into a pass. */
	struct FMotionProbe
	{
		float SettlingSeconds = 1.0f;
		bool bValidSettlingContract = true;
		float Elapsed = 0.0f, SampleSeconds = 0.0f, SpeedSum = 0.0f;
		float MinimumSpeed = MAX_flt, MaximumSpeed = 0.0f, MaximumYawError = 0.0f, MinimumDirectionDot = 1.0f;
		int32 Samples = 0;
		FVector FirstLocation = FVector::ZeroVector, LastLocation = FVector::ZeroVector;
		FPoseWindow Legs;
		FArmJointWindow Arms;
		bool bWrongBlockState = false, bLostGround = false, bWrongCap = false;
		void Sample(APawn* Character, float DeltaSeconds, bool bExpectedBlock, const FVector& Direction, float FacingYaw,
			bool bCheckFacing, const TArray<FQuat>& Pose, float ExpectedCap)
		{
			Elapsed += DeltaSeconds;
			if (Elapsed < SettlingSeconds || Complete()) return;
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
				bWrongCap |= !BlockState || !FMath::IsNearlyEqual(BlockState->SpeedLimit, bExpectedBlock ? ExpectedCap : 0.f);
			}
			if (Character->GetLocalRole() != ROLE_SimulatedProxy)
				bWrongCap |= !ASC(Character) || !FMath::IsNearlyEqual(ASC(Character)->GetBlockMovementSpeedLimit(), bExpectedBlock ? ExpectedCap : 0.f);
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
	struct FPeer
	{
		TWeakObjectPtr<APawn> Character;
		TWeakObjectPtr<USkeletalMeshComponent> Source;
		TWeakObjectPtr<UAnimInstance> Animation, Layer;
		TWeakObjectPtr<URpgAbilitySystemComponent> AbilitySystem;
		TWeakObjectPtr<URpgEquipmentInstance> Item;
		TArray<TWeakObjectPtr<AActor>> EquipmentActors;
		FGameplayAbilitySpecHandle BlockHandle;
		FDelegateHandle ActivatedHandle, EndedHandle;
		FMotionProbe Motion;
		int32 Activations = 0, Ends = 0, CancelledEnds = 0;
		bool bLateJoin = false, bSourceChanged = false, bEquipmentChanged = false;
		bool bRootMotion = false, bInvalidPose = false, bWrongLayer = false, bLoopPlayed = false;
		struct FReaction
		{
			int32 Samples = 0, InstanceId = INDEX_NONE;
			float FirstPosition = -1.f, LastPosition = -1.f, MaximumWeight = 0.f, MinimumSpeed = MAX_flt;
			float LeftRange = 0.f, RightRange = 0.f;
			TArray<FQuat> FirstPose;
			FVector FirstLocation = FVector::ZeroVector, LastLocation = FVector::ZeroVector;
			bool bInvalid = false, bSawStagger = false, bSawBlockCleared = false;
		} Reaction;
	};
	/** Samples the actual equipment-owned linked instance and finalized movement, without driving animation. */
	class FScopedObservations final
	{
	public:
		~FScopedObservations() { Stop(); }
		void Start(int32 PlayerId, UAnimMontage* Start, UAnimMontage* Loop, UAnimMontage* End, ERpgEquipmentSlot Slot, UClass* Profile, float Cap)
		{
			Subject = PlayerId; StartClip = Start; LoopClip = Loop; EndClip = End; EquipmentSlot = Slot;
			ExpectedClass = Profile; ExpectedCap = Cap;
			Handle = FWorldDelegates::OnWorldTickEnd.AddRaw(this, &FScopedObservations::Tick);
		}
		bool Add(UWorld* World, bool bLateJoin = false)
		{
			APawn* Character = Pawn(World, Subject);
			if (Peers.Contains(World) || !Mesh(Character) || !ASC(Character) || !EquipmentReady(Character, EquipmentSlot)) return false;
			UAnimInstance* Layer = Equipment(Character)->GetBlockLocomotionLayerInstance();
			if (!Layer || Layer->GetClass() != ExpectedClass.Get()) return false;
			FPeer& Peer = Peers.Add(World);
			Peer.Character = Character; Peer.Source = Mesh(Character); Peer.Animation = Mesh(Character)->GetAnimInstance(); Peer.Layer = Layer;
			Peer.AbilitySystem = ASC(Character); Peer.Item = Equipment(Character)->GetEquipmentInstanceInSlot(EquipmentSlot); Peer.bLateJoin = bLateJoin;
			for (AActor* Actor : Peer.Item->GetSpawnedActors()) Peer.EquipmentActors.Add(Actor);
			if (const FGameplayAbilitySpec* Spec = BlockSpec(Character)) Peer.BlockHandle = Spec->Handle;
			Peer.ActivatedHandle = ASC(Character)->AbilityActivatedCallbacks.AddLambda([this, World](UGameplayAbility* Ability)
			{
				FPeer& Peer = Peers.FindChecked(World);
				if (Ability && Ability->GetCurrentAbilitySpecHandle() == Peer.BlockHandle) ++Peer.Activations;
			});
			Peer.EndedHandle = ASC(Character)->OnAbilityEnded.AddLambda([this, World](const FAbilityEndedData& Ended)
			{
				FPeer& Peer = Peers.FindChecked(World);
				if (Ended.AbilityThatEnded && Ended.AbilityThatEnded->GetCurrentAbilitySpecHandle() == Peer.BlockHandle)
				{ ++Peer.Ends; if (Ended.bWasCancelled) ++Peer.CancelledEnds; }
			});
			return true;
		}
		void BeginMotionProbe(bool bHeldBlock, FVector Direction, float Yaw, bool bFacing, float TargetSpeed)
		{
			bMotionProbe = true; bMotionExpectedBlock = bHeldBlock; MotionDirection = Direction; MotionYaw = Yaw; bMotionFacing = bFacing;
			float SettlingSeconds = 1.f;
			bool bValidSettlingContract = true;
			for (const auto& Entry : Peers)
			{
				APawn* Character = Entry.Value.Character.Get();
				const URpgCharacterMoverComponent* Movement = Mover(Character);
				if (!Character || !Character->HasAuthority() || !Movement) continue;
				const USmoothWalkingMode* Mode = Cast<USmoothWalkingMode>(Movement->GetMovementMode());
				auto ReadSetting = [Mode](FName Name)
				{
					const FFloatProperty* Property = Mode ? FindFProperty<FFloatProperty>(Mode->GetClass(), Name) : nullptr;
					return Property ? Property->GetPropertyValue_InContainer(Mode) : -1.f;
				};
				const float Acceleration = ReadSetting(TEXT("Acceleration"));
				const float Deceleration = ReadSetting(TEXT("Deceleration"));
				const float Smoothing = FMath::Max(ReadSetting(TEXT("AccelerationSmoothingTime")), ReadSetting(TEXT("DecelerationSmoothingTime")));
				const float Rate = FMath::Min(Acceleration, Deceleration);
				bValidSettlingContract = FMath::IsFinite(Rate) && Rate > 0.f && FMath::IsFinite(Smoothing) && Smoothing >= 0.f;
				float LargestSpeedChange = 0.f;
				for (const auto& Peer : Peers)
					LargestSpeedChange = FMath::Max(LargestSpeedChange, FMath::Abs(Speed(Peer.Value.Character.Get()) - TargetSpeed));
				// SmoothWalking deliberately integrates a gait change instead of clipping velocity. Sprint 585 -> cap 157
				// alone takes 1.43 s at the authored 300 cm/s^2. Freeze this budget before sampling; never retry bad samples.
				// Retain the original second for input/replication/pose settling and allow four spring smoothing times.
				if (bValidSettlingContract) SettlingSeconds += LargestSpeedChange / Rate + 4.f * Smoothing;
				bValidSettlingContract &= FMath::IsFinite(SettlingSeconds) && SettlingSeconds <= 10.f;
				if (!bValidSettlingContract) SettlingSeconds = 1.f;
				UE_LOG(LogTemp, Display, TEXT("RpgBlockLocomotion settling target=%.2f speedChange=%.2f acceleration=%.2f deceleration=%.2f smoothing=%.3f seconds=%.3f valid=%d"),
					TargetSpeed, LargestSpeedChange, Acceleration, Deceleration, Smoothing, SettlingSeconds, bValidSettlingContract);
			}
			for (auto& Entry : Peers)
			{
				Entry.Value.Motion = FMotionProbe();
				Entry.Value.Motion.SettlingSeconds = SettlingSeconds;
				Entry.Value.Motion.bValidSettlingContract = bValidSettlingContract;
			}
		}
		void EndMotionProbe() { bMotionProbe = false; }
		void BeginBlockEpisode() {}
		void SetExpectedCap(float Cap) { ExpectedCap = Cap; }
		void BeginReaction(UAnimMontage* Clip, bool bStagger)
		{
			ReactionClip = Clip; bReactionProbe = true; bStaggerProbe = bStagger;
			for (auto& Entry : Peers) Entry.Value.Reaction = FPeer::FReaction();
		}
		void EndReaction() { bReactionProbe = bStaggerProbe = false; ReactionClip.Reset(); }
		bool ReactionObserved() const
		{
			if (Peers.Num() != 3 || !ReactionClip.IsValid()) return false;
			for (const auto& Entry : Peers)
			{
				const FPeer& Peer = Entry.Value;
				const FPeer::FReaction& Reaction = Peer.Reaction;
				if (bStaggerProbe)
				{
					if (!Reaction.bSawStagger || !Reaction.bSawBlockCleared) return false;
					// Existing ServerOnly Stagger uses stock GAS playback; its owning-client montage gap is not hidden by a local event.
					if (Peer.Character->GetLocalRole() == ROLE_AutonomousProxy) continue;
				}
				if (Reaction.Samples < 3 || Reaction.LastPosition - Reaction.FirstPosition < 0.1f || Reaction.MaximumWeight < 0.5f) return false;
			}
			return true;
		}
		bool ReactionFinished() const
		{
			if (!ReactionClip.IsValid()) return false;
			for (const auto& Entry : Peers)
				if (Entry.Value.Animation->Montage_IsActive(ReactionClip.Get())
					|| (bStaggerProbe && ASC(Entry.Value.Character.Get())->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.Staggered"))))) return false;
			return true;
		}
		bool MotionProbeComplete(int32 Count) const
		{
			if (Peers.Num() != Count) return false;
			for (const auto& Entry : Peers) if (!Entry.Value.Motion.Complete()) return false;
			return true;
		}
		bool Released(APawn* Character) const
		{
			if (!ASC(Character) || !Equipment(Character) || Blocking(Character) || ASC(Character)->IsBlockMovementActive()) return false;
			const URpgAnimInstance* Layer = Cast<URpgAnimInstance>(Equipment(Character)->GetBlockLocomotionLayerInstance());
			if (!Layer || Layer->bBlockLocomotionActive) return false;
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
				UE_LOG(LogTemp, Display, TEXT("RpgBlockLocomotion stage=%s world=%s layer=%s activated=%d ended=%d cancelled=%d sourceChanged=%d equipmentChanged=%d wrongLayer=%d loopPlayed=%d rootMotion=%d invalidPose=%d"),
					Label, *GetPathNameSafe(Entry.Key.Get()), *GetPathNameSafe(Peer.Layer.Get()), Peer.Activations, Peer.Ends, Peer.CancelledEnds,
					Peer.bSourceChanged, Peer.bEquipmentChanged, Peer.bWrongLayer, Peer.bLoopPlayed, Peer.bRootMotion, Peer.bInvalidPose);
				if (bReactionProbe)
					UE_LOG(LogTemp, Display, TEXT("RpgBlockLocomotion reaction stage=%s world=%s role=%d clip=%s stagger=%d instance=%d samples=%d phase=%.3f..%.3f weight=%.3f legs=%.3f/%.3f minimumSpeed=%.2f distance=%.2f sawStagger=%d clearedBlock=%d invalid=%d"),
						Label, *GetPathNameSafe(Entry.Key.Get()), static_cast<int32>(Peer.Character->GetLocalRole()), *GetPathNameSafe(ReactionClip.Get()), bStaggerProbe,
						Peer.Reaction.InstanceId, Peer.Reaction.Samples, Peer.Reaction.FirstPosition, Peer.Reaction.LastPosition, Peer.Reaction.MaximumWeight,
						Peer.Reaction.LeftRange, Peer.Reaction.RightRange, Peer.Reaction.MinimumSpeed,
						FVector::Dist2D(Peer.Reaction.FirstLocation, Peer.Reaction.LastLocation), Peer.Reaction.bSawStagger, Peer.Reaction.bSawBlockCleared, Peer.Reaction.bInvalid);
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
			URpgAnimInstance* Layer = Cast<URpgAnimInstance>(Equipment(Character)->GetBlockLocomotionLayerInstance());
			Peer->bWrongLayer |= !Layer || Layer != Peer->Layer.Get() || Layer->GetClass() != ExpectedClass.Get();
			Peer->bLoopPlayed |= LoopClip.IsValid() && Animation->Montage_IsPlaying(LoopClip.Get());
			Peer->bRootMotion |= !bStaggerProbe && (Animation->GetRootMotionMontageInstance() != nullptr
				|| (Mover(Character) && Mover(Character)->FindActiveLayeredMoveByType(FRpgMoverAbilityRootMotion::StaticStruct()) != nullptr));
			const TArray<FQuat> Pose = ReadPose(Mesh(Character));
			Peer->bInvalidPose |= Pose.Num() != 4;
			if (bReactionProbe && ReactionClip.IsValid())
			{
				FPeer::FReaction& Reaction = Peer->Reaction;
				if (bStaggerProbe)
				{
					Reaction.bSawStagger |= ASC(Character)->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.Staggered")));
					const FNumericProperty* AlphaProperty = Layer ? FindFProperty<FNumericProperty>(Layer->GetClass(), TEXT("LayerAlpha")) : nullptr;
					const double Alpha = AlphaProperty && AlphaProperty->IsFloatingPoint()
						? AlphaProperty->GetFloatingPointPropertyValue(AlphaProperty->ContainerPtrToValuePtr<void>(Layer)) : -1.0;
					Reaction.bSawBlockCleared |= !Blocking(Character) && !ASC(Character)->IsBlockMovementActive()
						&& Layer && !Layer->bBlockLocomotionActive && FMath::IsFinite(Alpha) && Alpha >= 0.0 && Alpha <= 0.01;
				}
				FAnimMontageInstance* Instance = Animation->GetActiveInstanceForMontage(ReactionClip.Get());
				if (Instance && Instance->IsPlaying() && Instance->GetWeight() > 0.01f && !ReactionClip->SlotAnimTracks.IsEmpty())
				{
					const FName Slot = ReactionClip->SlotAnimTracks[0].SlotName;
					const float Weight = bStaggerProbe ? Animation->GetSlotMontageGlobalWeight(Slot)
						: Layer ? Layer->GetSlotMontageGlobalWeight(Slot) : 0.f;
					if (Reaction.Samples++ == 0)
					{
						Reaction.InstanceId = Instance->GetInstanceID(); Reaction.FirstPosition = Instance->GetPosition();
						Reaction.FirstPose = Pose; Reaction.FirstLocation = Character->GetActorLocation();
					}
					// Stock GAS may correct proxy position within the same instance; identity and net progress matter here.
					Reaction.bInvalid |= Reaction.InstanceId != Instance->GetInstanceID() || Pose.Num() != 4 || !FMath::IsFinite(Weight);
					Reaction.LastPosition = Instance->GetPosition(); Reaction.LastLocation = Character->GetActorLocation();
					Reaction.MaximumWeight = FMath::Max(Reaction.MaximumWeight, Weight);
					Reaction.MinimumSpeed = FMath::Min(Reaction.MinimumSpeed, Speed(Character));
					if (Pose.Num() == 4 && Reaction.FirstPose.Num() == 4)
					{
						Reaction.LeftRange = FMath::Max(Reaction.LeftRange, static_cast<float>(FMath::Max(Reaction.FirstPose[0].AngularDistance(Pose[0]), Reaction.FirstPose[1].AngularDistance(Pose[1]))));
						Reaction.RightRange = FMath::Max(Reaction.RightRange, static_cast<float>(FMath::Max(Reaction.FirstPose[2].AngularDistance(Pose[2]), Reaction.FirstPose[3].AngularDistance(Pose[3]))));
					}
					if (!bStaggerProbe)
						Reaction.bInvalid |= !Blocking(Character) || !ASC(Character)->IsBlockMovementActive()
							|| !Layer || !Layer->bBlockLocomotionActive || !Grounded(Character)
							|| (Character->GetLocalRole() != ROLE_SimulatedProxy && (!BlockSpec(Character) || !BlockSpec(Character)->IsActive()));
				}
			}
			if (bMotionProbe)
			{
				const bool bSampleArms = bMotionExpectedBlock && Peer->Motion.Elapsed + DeltaSeconds >= Peer->Motion.SettlingSeconds && !Peer->Motion.Complete();
				Peer->Motion.Sample(Character, DeltaSeconds, bMotionExpectedBlock, MotionDirection, MotionYaw, bMotionFacing, Pose, ExpectedCap);
				if (bSampleArms) Peer->Motion.Arms.Sample(Character, Mesh(Character), Layer);
				// Only settled samples compare replicated movement/tag clocks against animation's game-thread snapshot.
				if (Peer->Motion.Elapsed >= Peer->Motion.SettlingSeconds && Layer)
					Peer->bWrongLayer |= Layer->bBlockLocomotionActive != bMotionExpectedBlock;
			}
		}
		TMap<TWeakObjectPtr<UWorld>, FPeer> Peers;
		FDelegateHandle Handle;
		TWeakObjectPtr<UAnimMontage> StartClip, LoopClip, EndClip;
		TWeakObjectPtr<UAnimMontage> ReactionClip;
		TWeakObjectPtr<UClass> ExpectedClass;
		int32 Subject = INDEX_NONE;
		ERpgEquipmentSlot EquipmentSlot = ERpgEquipmentSlot::None;
		bool bMotionProbe = false, bMotionExpectedBlock = false, bMotionFacing = false;
		bool bReactionProbe = false, bStaggerProbe = false;
		FVector MotionDirection = FVector::ZeroVector;
		float MotionYaw = 0.f, ExpectedCap = 0.f;
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
		bool Inject(APawn* Character, bool bAcrossRelease, bool bObserveAuthorityCancellation = false)
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
			URpgAnimInstance* Instance = Cast<URpgAnimInstance>(Equipment(Character)->GetBlockLocomotionLayerInstance());
			if (!Default || !BlockState || !BlockState->bBlocking || !Ability || !Ability->IsActive() || !Instance || !Instance->bBlockLocomotionActive) return false;
			Owner = Character; Liaison = Backend; Layer = Instance; ExpectedCap = BlockState->SpeedLimit; AbilityHandle = Ability->Handle;
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
			UE_LOG(LogTemp, Display, TEXT("RpgBlockLocomotion correction armed local=%d offset=%d lastServerFrame=%d crossRelease=%d authorityCancel=%d positionInjected=%d position=%s cap=%.2f"),
				InjectedFrame, Prediction->GetFixedTickState().Offset, Mover(Character)->GetLastTimeStep().ServerFrame,
				bCrossRelease, bAuthorityCancellation, !bAuthorityCancellation, *Default->GetLocation_WorldSpace().ToCompactString(), ExpectedCap);
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
				UE_LOG(LogTemp, Display, TEXT("RpgBlockLocomotion correction epoch frame=%llu crossRelease=%d authorityCancel=%d callbacks=%d head=%d tracked=%d restored=%d replaced=%d hasReplacement=%d replaySteps=%d blockRestored=%d blockPredicted=%d blockReplacement=%d blockBefore=%d blockAfter=%d ownerLease=%d delta=%s predictedSpeed2D=%.3f replacementSpeed2D=%.3f deltaV=%s"),
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
			const FRpgMoverBlockMovementSyncState* PredictedPolicy = Observer->PredictedSync.SyncStateCollection.FindDataByType<FRpgMoverBlockMovementSyncState>();
			const FRpgMoverBlockMovementSyncState* ReplacedPolicy = Observer->ReplacementSync.SyncStateCollection.FindDataByType<FRpgMoverBlockMovementSyncState>();
			bBlockHistory &= PredictedPolicy && ReplacedPolicy && FMath::IsNearlyEqual(PredictedPolicy->SpeedLimit, ExpectedCap)
				&& FMath::IsNearlyEqual(ReplacedPolicy->SpeedLimit, bAuthorityCancellation ? 0.f : ExpectedCap);
			const FGameplayAbilitySpec* Ability = BlockSpec(Owner.Get());
			const UAnimInstance* Instance = Equipment(Owner.Get())->GetBlockLocomotionLayerInstance();
			bAbilityPreserved = Ability && Ability->Handle == AbilityHandle
				&& Ability->GetPrimaryInstance()->GetCurrentActivationInfo().GetActivationPredictionKey() == ActivationKey
				&& (bCrossRelease ? !Ability->IsActive() : Ability->IsActive() && Instance && Instance == Layer.Get());
			UE_LOG(LogTemp, Display, TEXT("RpgBlockLocomotion correction observed crossRelease=%d authorityCancel=%d tracked=%d restored=%d replaced=%d replaySteps=%d delta=%s predictedSpeed2D=%.3f replacementSpeed2D=%.3f speedGain=%.3f deltaV=%s sameHead=%d expectedHistory=%d releasedHead=%d abilityPreserved=%d"),
				bCrossRelease, bAuthorityCancellation, InjectedFrame, Observer->RestoredLocalFrame, Observer->ReplacedLocalFrame, Observer->ReplaySteps, *Delta.ToCompactString(),
				PredictedSpeed, ReplacedSpeed, SpeedGain, *(Replaced->GetVelocity_WorldSpace() - Predicted->GetVelocity_WorldSpace()).ToCompactString(),
				bSameHead, bBlockHistory, bReleasedHead, bAbilityPreserved);
		}
		TWeakObjectPtr<APawn> Owner;
		TWeakObjectPtr<UMoverNetworkPredictionLiaisonComponent> Liaison;
		TWeakObjectPtr<UAnimInstance> Layer;
		float ExpectedCap = 0.f;
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
		int32 BeforeCount = 0, InjectedFrame = INDEX_NONE, DiagnosticEpochs = 0;
		bool bInjected = false, bObserved = false, bCrossRelease = false, bBeforeValid = false;
		bool bAuthorityCancellation = false;
		bool bSameHead = false, bBlockHistory = false, bAbilityPreserved = false;
	};
	struct FState : FBasePIENetworkComponentState {};
	/** Local instance-only tuning for lifecycle tests; the authored asset/CDO is never changed. */
	class FScopedProfiles final
	{
	public:
		~FScopedProfiles() { Restore(); }
		bool Configure(APawn* Character, UClass* LayerClass, float Cap)
		{
			URpgWeaponInstance* Weapon = Equipment(Character) ? Cast<URpgWeaponInstance>(Equipment(Character)->GetActiveBlockSource()) : nullptr;
			if (!Weapon) return false;
			if (!Saved.Contains(Weapon)) Saved.Add(Weapon, Weapon->GetBlockDefinition());
			return Weapon->ConfigureBlockLocomotionForTests(LayerClass, Cap);
		}
		void Restore()
		{
			for (const auto& Entry : Saved) if (URpgWeaponInstance* Weapon = Entry.Key.Get())
				Weapon->ConfigureBlockLocomotionForTests(Entry.Value.BlockLocomotionLayer, Entry.Value.MovementSpeedLimit);
			Saved.Empty();
		}
	private:
		TMap<TWeakObjectPtr<URpgWeaponInstance>, FRpgWeaponBlockDefinition> Saved;
	};
	struct FBindingRecord
	{
		TWeakObjectPtr<APawn> Character;
		TWeakObjectPtr<USkeletalMeshComponent> Mesh;
		TWeakObjectPtr<URpgAbilitySystemComponent> AbilitySystem;
		TWeakObjectPtr<URpgEquipmentInstance> Item;
		TWeakObjectPtr<UAnimInstance> Main, Layer;
		TArray<TWeakObjectPtr<AActor>> Actors;
	};
}

NETWORK_TEST_CLASS(BlockLocomotionPIE, "SurvivalRpg.Combat.BlockLocomotion")
{
	using FState = RpgBlockLocomotionTests::FState;
	using EVariant = RpgBlockLocomotionTests::EVariant;
	using EScenario = RpgBlockLocomotionTests::EScenario;
	RpgBlockLocomotionTests::FScopedWorld Isolation;
	RpgBlockLocomotionTests::FScopedInput Input;
	RpgBlockLocomotionTests::FScopedObservations Observations;
	RpgBlockLocomotionTests::FBlockCorrection HeldCorrection, ReleaseCorrection;
	RpgBlockLocomotionTests::FScopedProfiles Profiles;
	TArray<RpgBlockLocomotionTests::FBindingRecord> BindingRecords;
	TStrongObjectPtr<UClass> VariantProfile;
	TStrongObjectPtr<UAnimMontage> BlockStart, BlockLoop, BlockEnd;
	TStrongObjectPtr<UClass> BlockProfile;
	TWeakObjectPtr<UGameplayAbility> TunedStagger;
	TStrongObjectPtr<UAnimMontage> OriginalStaggerMontage;
	FObjectPropertyBase* StaggerMontageProperty = nullptr;
	float BlockCap = 0.f;
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
		if (TunedStagger.IsValid() && StaggerMontageProperty)
			StaggerMontageProperty->SetObjectPropertyValue_InContainer(TunedStagger.Get(), OriginalStaggerMontage.Get());
		TunedStagger.Reset(); OriginalStaggerMontage.Reset(); StaggerMontageProperty = nullptr;
		Profiles.Restore();
		if (bConfigured) GetMutableDefault<URpgDeveloperSettings>()->ExperienceOverride = PreviousExperience;
	}
	TEST_METHOD(CMCProfileCapsMovementFacesCameraAndSupportsLateJoin) { Queue(EVariant::CMC, EScenario::DirectionalFacing); }
	TEST_METHOD(MoverProfileCapsMovementFacesCameraAndSupportsLateJoin) { Queue(EVariant::Mover, EScenario::DirectionalFacing); }
	TEST_METHOD(MoverBlockPreventsSprintInBothInputOrdersAndResumesHeldSprintOnRelease) { Queue(EVariant::Mover, EScenario::SprintOrdering); }
	TEST_METHOD(MoverFixedRollbackPreservesHeldBlockAndReplaysBlockedHistoryAcrossRelease) { Queue(EVariant::Mover, EScenario::FixedCorrection); }
	TEST_METHOD(MoverAuthorityCancelUsesRawSprintWhileOwnerStillPredictsBlockAndThenCorrects) { Queue(EVariant::Mover, EScenario::AuthorityCancel); }
	TEST_METHOD(CMCActivationSnapshotsCapAndZeroCapPreservesNormalSpeed) { Queue(EVariant::CMC, EScenario::CapSnapshot); }
	TEST_METHOD(MoverActivationSnapshotsCapAndZeroCapPreservesNormalSpeed) { Queue(EVariant::Mover, EScenario::CapSnapshot); }
	TEST_METHOD(CMCEquipmentLayerSwapsAndRebindsAfterAnimationReinitialization) { Queue(EVariant::CMC, EScenario::LayerLifecycle); }
	TEST_METHOD(MoverEquipmentLayerSwapsAndRebindsAfterAnimationReinitialization) { Queue(EVariant::Mover, EScenario::LayerLifecycle); }
	TEST_METHOD(CMCServerBlockReactionsPreserveMovingLegsAndStaggerCancelsBlock) { Queue(EVariant::CMC, EScenario::Reactions); }
	TEST_METHOD(MoverServerBlockReactionsPreserveMovingLegsAndStaggerCancelsBlock) { Queue(EVariant::Mover, EScenario::Reactions); }

	void Queue(EVariant SelectedVariant, EScenario Scenario = EScenario::DirectionalFacing)
	{
		using namespace RpgBlockLocomotionTests;
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
			{ APawn* Character = LocalPawn(PeerState.World); return Ready(PeerState.World, Character, Variant) && Grounded(Character) && BlockSpec(Character) && ProfileReady(Character); }, Timeout())
			.ThenClient(TEXT("Retain the autonomous subject and its real player input"), 0, [this](FState& PeerState)
			{
				APawn* Character = LocalPawn(PeerState.World);
				ASSERT_THAT(IsTrue(Character->GetLocalRole() == ROLE_AutonomousProxy));
				SubjectId = Character->GetPlayerState()->GetPlayerId(); ASSERT_THAT(IsTrue(Input.Start(Character)));
			})
			.UntilServer(TEXT("Authority resolves the same composed pawn and actual equipment block definition"), [this](FState& PeerState)
			{ APawn* Character = Pawn(PeerState.World, SubjectId); return Ready(PeerState.World, Character, Variant) && BlockSpec(Character) && ProfileReady(Character); }, Timeout())
			.ThenServer(TEXT("Read authored block clips and begin passive observation before input"), [this](FState& PeerState)
			{
				const URpgWeaponInstance* Item = Cast<URpgWeaponInstance>(BlockSpec(Pawn(PeerState.World, SubjectId))->SourceObject.Get());
				ASSERT_THAT(IsNotNull(Item)); if (!Item) return;
				const FRpgWeaponBlockDefinition& Definition = Item->GetBlockDefinition();
				BlockSlot = Item->GetEquippedSlot(); BlockStart.Reset(Definition.BlockStartMontage.Get());
				BlockLoop.Reset(Definition.BlockLoopMontage.Get()); BlockEnd.Reset(Definition.BlockEndMontage.Get());
				BlockProfile.Reset(Definition.BlockLocomotionLayer.Get()); BlockCap = Definition.MovementSpeedLimit;
				ASSERT_THAT(IsTrue(Definition.bCanBlock && BlockProfile.IsValid() && FMath::IsFinite(BlockCap) && BlockCap > 0.f && BlockCap < NormalSpeed()));
				if (!BlockProfile.IsValid()) return;
				for (const UAnimMontage* Clip : { BlockStart.Get(), BlockLoop.Get(), BlockEnd.Get() })
					if (Clip) ASSERT_THAT(IsFalse(Clip->HasRootMotion()));
				UE_LOG(LogTemp, Display, TEXT("RpgBlockLocomotion content experience=%s start=%s loop=%s end=%s"),
					*ExperienceId(Variant).ToString(), *GetPathNameSafe(BlockStart.Get()), *GetPathNameSafe(BlockLoop.Get()), *GetPathNameSafe(BlockEnd.Get()));
				Observations.Start(SubjectId, BlockStart.Get(), BlockLoop.Get(), BlockEnd.Get(), BlockSlot, BlockProfile.Get(), BlockCap);
				ASSERT_THAT(IsTrue(Isolation.IsIsolated(PeerState.World) && Observations.Add(PeerState.World)));
			})
			.ThenClient(TEXT("Observe the owner before the block starts"), 0, [this, Scenario](FState& PeerState)
				{ ASSERT_THAT(IsTrue(Isolation.IsIsolated(PeerState.World) && Observations.Add(PeerState.World))); Input.Move(false); });
		if (Scenario == EScenario::DirectionalFacing) { QueueDirectionalFacing(); return; }
		if (Scenario == EScenario::SprintOrdering) { QueueSprintOrdering(); return; }
		if (Scenario == EScenario::FixedCorrection) { QueueFixedCorrection(); return; }
		if (Scenario == EScenario::AuthorityCancel) { QueueAuthorityCancel(); return; }
		if (Scenario == EScenario::CapSnapshot) { QueueCapSnapshot(); return; }
		if (Scenario == EScenario::LayerLifecycle) { QueueLayerLifecycle(); return; }
		if (Scenario == EScenario::Reactions) { QueueReactions(); return; }
	}

	void QueueReaction(bool bPerfect)
	{
		using namespace RpgBlockLocomotionTests;
		Network->ThenServer(TEXT("Resolve an authoritative block reaction through the active GAS event task"), [this, bPerfect](FState& PeerState)
			{
				APawn* Character = Pawn(PeerState.World, SubjectId);
				const FGameplayAbilitySpec* Spec = BlockSpec(Character);
				const URpgWeaponInstance* Weapon = Spec ? Cast<URpgWeaponInstance>(Spec->SourceObject.Get()) : nullptr;
				ASSERT_THAT(IsTrue(Character && Character->HasAuthority() && Spec && Spec->IsActive() && Weapon)); if (!Weapon) return;
				const FRpgWeaponBlockDefinition& Definition = Weapon->GetBlockDefinition();
				UAnimMontage* Montage = bPerfect ? Definition.PerfectBlockMontage.Get() : Definition.BlockHitMontage.Get();
				ASSERT_THAT(IsTrue(Montage && !Montage->HasRootMotion() && !Montage->SlotAnimTracks.IsEmpty())); if (!Montage) return;
				Observations.BeginReaction(Montage, false);
				// This is the authority event boundary used by DamageExecution::SendCombatEvent. It exercises
				// the real active ability/owner confirmation/stock proxy path, not damage arithmetic or a forged local montage.
				const FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(bPerfect ? TEXT("GameplayEvent.PerfectBlock") : TEXT("GameplayEvent.Block"));
				FGameplayEventData Payload; Payload.EventTag = EventTag; Payload.Target = Character;
				UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Character, EventTag, Payload);
				ASSERT_THAT(IsTrue(ASC(Character)->GetCurrentMontage() == Montage && Spec->IsActive()));
			})
			.UntilServer(TEXT("Owner, authority and late observer play the actual short reaction"), [this](FState&) { return Observations.ReactionObserved(); }, Timeout())
			.UntilServer(TEXT("The short reaction completes on all roles while the original block remains held"), [this](FState&) { return Observations.ReactionFinished(); }, Timeout())
			.ThenServer(TEXT("Short GAS reactions preserve real leg motion and the same block episode"), [this, bPerfect](FState&)
			{
				Observations.Report(bPerfect ? TEXT("PerfectBlockReaction") : TEXT("BlockHitReaction"));
				for (const auto& Entry : Observations.GetPeers())
				{
					const FPeer& Peer = Entry.Value;
					const FPeer::FReaction& Reaction = Peer.Reaction;
					TestRunner->TestTrue(TEXT("The exact configured short montage advances once with actual linked-slot weight"),
						!Reaction.bInvalid && Reaction.Samples >= 3 && Reaction.LastPosition - Reaction.FirstPosition >= 0.1f && Reaction.MaximumWeight >= 0.5f);
					TestRunner->TestTrue(TEXT("Both legs continue local animation and movement throughout the short upper-body reaction"),
						Reaction.LeftRange > 0.15f && Reaction.RightRange > 0.15f && Reaction.MinimumSpeed > BlockCap * 0.8f
						&& FVector::Dist2D(Reaction.FirstLocation, Reaction.LastLocation) > 2.f);
					TestRunner->TestTrue(TEXT("The reaction preserves block, movement lease, mesh, equipment and layer ownership"),
						Blocking(Peer.Character.Get()) && ASC(Peer.Character.Get())->IsBlockMovementActive()
						&& !Peer.bSourceChanged && !Peer.bEquipmentChanged && !Peer.bWrongLayer && !Peer.bLoopPlayed && !Peer.bRootMotion);
					if (!Peer.bLateJoin)
					{
						TestRunner->TestEqual(TEXT("Reactions reuse the original block activation"), Peer.Activations, 1);
						TestRunner->TestEqual(TEXT("Short reactions never end the held block"), Peer.Ends, 0);
					}
				}
				Observations.EndReaction();
			});
		QueueMotion(bPerfect ? TEXT("AfterPerfectBlockReaction") : TEXT("AfterBlockHitReaction"), FVector2D(0, 1), true, true, 3);
	}

	void QueueReactions()
	{
		using namespace RpgBlockLocomotionTests;
		Network->ThenClient(TEXT("Hold block with real forward input before authoritative combat events"), 0, [this](FState&) { Input.Block(true); });
		QueueMotion(TEXT("BeforeBlockReactions"), FVector2D(0, 1), true, true, 2);
		QueueLateJoin();
		QueueMotion(TEXT("LateObserverBeforeBlockReactions"), FVector2D(0, 1), true, true, 3);
		QueueReaction(false);
		QueueReaction(true);
		Network->ThenServer(TEXT("Configure only the transient Stagger instance's fixture montage, then trigger its actual GAS event"), [this](FState& PeerState)
			{
				APawn* Character = Pawn(PeerState.World, SubjectId);
				FGameplayAbilitySpec* Stagger = nullptr;
				for (FGameplayAbilitySpec& Spec : ASC(Character)->GetActivatableAbilities())
					if (!Spec.PendingRemove && Cast<URpgGameplayAbility_Stagger>(Spec.GetPrimaryInstance())) { Stagger = &Spec; break; }
				ASSERT_THAT(IsTrue(Character->HasAuthority() && Stagger && !Stagger->IsActive() && Blocking(Character))); if (!Stagger) return;
				UGameplayAbility* StaggerInstance = Stagger->GetPrimaryInstance();
				const URpgWeaponInstance* Weapon = Cast<URpgWeaponInstance>(BlockSpec(Character)->SourceObject.Get());
				UAnimMontage* FixtureMontage = Weapon ? Weapon->GetBlockDefinition().GuardBreakMontage.Get() : nullptr;
				StaggerMontageProperty = FindFProperty<FObjectPropertyBase>(StaggerInstance->GetClass(), TEXT("GuardBreakMontage"));
				ASSERT_THAT(IsTrue(StaggerInstance && !StaggerInstance->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject)
					&& StaggerMontageProperty && StaggerMontageProperty->PropertyClass == UAnimMontage::StaticClass()
					&& FixtureMontage && FixtureMontage->IsValidSlot(TEXT("DefaultSlot"))));
				if (!StaggerMontageProperty || !FixtureMontage) return;
				// Global Stagger currently has no authored montage. Supply the real compatible shield-break
				// asset only on this spawned ability instance to exercise full-body slot priority through GAS.
				// No asset/CDO/other pawn is changed; AFTER_EACH restores the exact original instance value.
				TunedStagger = StaggerInstance;
				OriginalStaggerMontage.Reset(Cast<UAnimMontage>(StaggerMontageProperty->GetObjectPropertyValue_InContainer(StaggerInstance)));
				StaggerMontageProperty->SetObjectPropertyValue_InContainer(StaggerInstance, FixtureMontage);
				const FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(TEXT("GameplayEvent.Stagger"));
				FGameplayEventData Payload; Payload.EventTag = EventTag; Payload.Target = Character;
				UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Character, EventTag, Payload);
				UAnimMontage* Montage = ASC(Character)->GetCurrentMontage();
				const auto AuthoredMontage = [StaggerInstance](FName PropertyName) -> UObject*
				{
					const FObjectPropertyBase* Property = StaggerInstance ? FindFProperty<FObjectPropertyBase>(StaggerInstance->GetClass(), PropertyName) : nullptr;
					return Property ? Property->GetObjectPropertyValue_InContainer(StaggerInstance) : nullptr;
				};
				UE_LOG(LogTemp, Display, TEXT("RpgBlockLocomotion staggerEntry world=%s frame=%llu active=%d blockActive=%d lease=%d tag=%d montage=%s isBlockEnd=%d defaultSlot=%d configuredGuardBreak=%s configuredStagger=%s"),
					*GetPathNameSafe(PeerState.World), GFrameCounter, Stagger->IsActive(), BlockSpec(Character)->IsActive(), ASC(Character)->IsBlockMovementActive(),
					ASC(Character)->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.Staggered"))), *GetPathNameSafe(Montage),
					Montage == BlockEnd.Get(), Montage && Montage->IsValidSlot(TEXT("DefaultSlot")),
					*GetPathNameSafe(AuthoredMontage(TEXT("GuardBreakMontage"))), *GetPathNameSafe(AuthoredMontage(TEXT("StaggerMontage"))));
				ASSERT_THAT(IsTrue(Stagger->IsActive() && !BlockSpec(Character)->IsActive() && !ASC(Character)->IsBlockMovementActive()
					&& Montage == FixtureMontage && Montage != BlockEnd.Get() && Montage->IsValidSlot(TEXT("DefaultSlot"))));
				if (Montage) Observations.BeginReaction(Montage, true);
			})
			.UntilServer(TEXT("Real Stagger cancels block on all roles and overrides the server/proxy full-body slot"), [this](FState&) { return Observations.ReactionObserved(); }, Timeout())
			.ThenClient(TEXT("Release the still-held physical RMB after the authoritative interruption is observed"), 0, [this](FState&) { Input.Block(false); })
			.UntilServer(TEXT("The real Stagger lifecycle and its server/proxy montage complete"), [this](FState&) { return Observations.ReactionFinished(); }, Timeout())
			.ThenServer(TEXT("Verify Stagger priority and exact block cancellation without synthesizing owner playback"), [this](FState&)
			{
				Observations.Report(TEXT("StaggerInterrupt"));
				for (const auto& Entry : Observations.GetPeers())
				{
					const FPeer& Peer = Entry.Value;
					TestRunner->TestTrue(TEXT("Every role observed the genuine Stagger tag and cleared the active block layer"),
						Peer.Reaction.bSawStagger && Peer.Reaction.bSawBlockCleared && !Peer.Reaction.bInvalid);
					if (!Peer.bLateJoin)
					{
						TestRunner->TestEqual(TEXT("Stagger cancels the one original block activation"), Peer.CancelledEnds, 1);
						TestRunner->TestEqual(TEXT("The interrupted block ends exactly once"), Peer.Ends, 1);
						TestRunner->TestEqual(TEXT("Held input did not resurrect block through Stagger"), Peer.Activations, 1);
					}
					if (Peer.Character->GetLocalRole() == ROLE_AutonomousProxy)
					{
						UE_LOG(LogTemp, Display, TEXT("RpgBlockLocomotion known limit: ServerOnly Stagger owner montage is outside this contract; observedSamples=%d"), Peer.Reaction.Samples);
					}
					else
					{
						TestRunner->TestTrue(TEXT("Authority and simulated proxy evaluated the real full-body Stagger montage"),
							Peer.Reaction.Samples >= 3 && Peer.Reaction.MaximumWeight >= 0.5f && Peer.Reaction.LastPosition - Peer.Reaction.FirstPosition >= 0.1f);
					}
				}
				Observations.EndReaction();
			});
		QueueMotion(TEXT("OrdinaryMovementAfterStagger"), FVector2D(0, 1), false, false, 3);
		Network->ThenClient(TEXT("Leave the recovered pawn idle with no held test inputs"), 0, [this](FState&) { Input.MoveKeys(FVector2D::ZeroVector); })
			.UntilServer(TEXT("All reaction-test peers return to supported idle with block fully released"), [this](FState&)
			{
				if (Observations.GetPeers().Num() != 3) return false;
				for (const auto& Entry : Observations.GetPeers())
					if (!Grounded(Entry.Value.Character.Get()) || Speed(Entry.Value.Character.Get()) >= 5.f
						|| !Observations.Released(Entry.Value.Character.Get())) return false;
				return true;
			}, Timeout());
	}

	void QueueCapSnapshot()
	{
		using namespace RpgBlockLocomotionTests;
		QueueMotion(TEXT("SnapshotOrdinaryBaseline"), FVector2D(0, 1), false, false, 2);
		Network->ThenClient(TEXT("Activate the authored cap through actual RMB input"), 0, [this](FState&) { Input.Block(true); });
		QueueMotion(TEXT("SnapshotOriginalCap"), FVector2D(0, 1), true, true, 2);
		Network->ThenServer(TEXT("Change only the authority instance's future tuning while this activation remains active"), [this](FState& State)
			{ ASSERT_THAT(IsTrue(Profiles.Configure(Pawn(State.World, SubjectId), BlockProfile.Get(), BlockCap * 0.5f))); })
			.ThenClient(TEXT("Apply the same future definition on the owning test instance without changing its live lease"), 0, [this](FState& State)
			{ ASSERT_THAT(IsTrue(Profiles.Configure(Pawn(State.World, SubjectId), BlockProfile.Get(), BlockCap * 0.5f))); });
		QueueMotion(TEXT("ActiveLeaseRetainsOriginalCap"), FVector2D(0, 1), true, true, 2);
		Network->ThenClient(TEXT("Release the first immutable activation"), 0, [this](FState&) { Input.Block(false); });
		QueueMotion(TEXT("BetweenSnapshotActivations"), FVector2D(0, 1), false, false, 2);
		Network->ThenClient(TEXT("The next real activation snapshots the changed item tuning"), 0, [this](FState&)
			{ BlockCap *= 0.5f; Observations.SetExpectedCap(BlockCap); Input.Block(true); });
		QueueMotion(TEXT("NextActivationUsesNewCap"), FVector2D(0, 1), true, true, 2);
		Network->ThenClient(TEXT("Release the second capped activation"), 0, [this](FState&) { Input.Block(false); });
		QueueMotion(TEXT("AfterSecondSnapshotRelease"), FVector2D(0, 1), false, false, 2);
		Network->ThenServer(TEXT("Configure the backwards-compatible zero cap on the authority instance"), [this](FState& State)
			{ ASSERT_THAT(IsTrue(Profiles.Configure(Pawn(State.World, SubjectId), BlockProfile.Get(), 0.f))); })
			.ThenClient(TEXT("Configure zero cap locally, then activate normally"), 0, [this](FState& State)
			{
				ASSERT_THAT(IsTrue(Profiles.Configure(Pawn(State.World, SubjectId), BlockProfile.Get(), 0.f)));
				BlockCap = 0.f; Observations.SetExpectedCap(0.f); Input.Block(true);
			});
		QueueMotion(TEXT("ZeroCapPreservesOrdinarySpeed"), FVector2D(0, 1), true, true, 2);
		Network->ThenClient(TEXT("Release the zero-cap activation"), 0, [this](FState&) { Input.Block(false); });
		QueueMotion(TEXT("AfterZeroCapRelease"), FVector2D(0, 1), false, false, 2);
		QueuePolicyCleanup(3, 2);
	}

	bool BindingsReady(UClass* ExpectedClass, bool bRequireNewMain) const
	{
		using namespace RpgBlockLocomotionTests;
		if (BindingRecords.Num() != 3 || !ExpectedClass) return false;
		for (const FBindingRecord& Record : BindingRecords)
		{
			APawn* Character = Record.Character.Get();
			USkeletalMeshComponent* Source = Mesh(Character);
			if (!Character || !Source || !Equipment(Character) || Source != Record.Mesh.Get()
				|| ASC(Character) != Record.AbilitySystem.Get() || Equipment(Character)->GetActiveBlockSource() != Record.Item.Get()) return false;
			UAnimInstance* Layer = Equipment(Character)->GetBlockLocomotionLayerInstance();
			if (!Layer || Layer->GetClass() != ExpectedClass || Layer == Record.Layer.Get()
				|| !Source->GetAnimInstance() || (bRequireNewMain && Source->GetAnimInstance() == Record.Main.Get())) return false;
			if (!bRequireNewMain && Source->GetAnimInstance() != Record.Main.Get()) return false;
			for (const TWeakObjectPtr<AActor>& Actor : Record.Actors)
				if (!Actor.IsValid() || !Actor->GetRootComponent() || Actor->GetRootComponent()->GetAttachParent() != Source) return false;
		}
		return true;
	}

	void ReinitializePresentation(APawn* Character)
	{
		using namespace RpgBlockLocomotionTests;
		USkeletalMeshComponent* Source = Mesh(Character);
		ASSERT_THAT(IsNotNull(Source)); if (!Source) return;
		UClass* MainClass = Source->GetAnimClass();
		ASSERT_THAT(IsNotNull(MainClass)); if (!MainClass) return;
		// Deliberately replace only this transient pawn's animation instance while no ability owns a montage.
		Source->SetAnimInstanceClass(nullptr);
		Source->SetAnimInstanceClass(MainClass);
	}

	void QueueLayerLifecycle()
	{
		using namespace RpgBlockLocomotionTests;
		Network->ThenClient(TEXT("Begin the ordinary equipment profile for a real active-block late join"), 0, [this](FState&) { Input.Block(true); });
		QueueMotion(TEXT("LayerBeforeLateJoin"), FVector2D(0, 1), true, true, 2);
		QueueLateJoin();
		QueueMotion(TEXT("LayerAfterLateJoin"), FVector2D(0, 1), true, true, 3);
		Network->ThenClient(TEXT("Release before replacing any presentation instance"), 0, [this](FState&) { Input.Block(false); });
		QueueMotion(TEXT("LayerBeforeSwap"), FVector2D::ZeroVector, false, false, 3);
		QueuePolicyCleanup(1, 3);
		Network->ThenServer(TEXT("Retain all three real bindings and load the second designer profile"), [this](FState&)
			{
				VariantProfile.Reset(LoadClass<UAnimInstance>(nullptr, TEXT("/GF_Combat_Core/Animations/BlockLocomotion/ABP_Block_TestVariant.ABP_Block_TestVariant_C")));
				ASSERT_THAT(IsTrue(VariantProfile.IsValid() && VariantProfile.Get() != BlockProfile.Get()));
				for (const auto& Entry : Observations.GetPeers())
				{
					const FPeer& Peer = Entry.Value;
					FBindingRecord& Record = BindingRecords.AddDefaulted_GetRef();
					Record.Character = Peer.Character; Record.Mesh = Peer.Source; Record.AbilitySystem = Peer.AbilitySystem;
					Record.Item = Peer.Item; Record.Main = Peer.Animation; Record.Layer = Peer.Layer; Record.Actors = Peer.EquipmentActors;
				}
				Observations.Stop();
				// These are explicit instance-only presentation overrides on each peer, not a claim that mutable tuning replicates.
				for (const FBindingRecord& Record : BindingRecords)
					ASSERT_THAT(IsTrue(Profiles.Configure(Record.Character.Get(), VariantProfile.Get(), BlockCap)));
			})
			.UntilServer(TEXT("Every equipment manager binds the second actual layer on its unchanged gameplay mesh"), [this](FState&)
				{ return BindingsReady(VariantProfile.Get(), false); }, Timeout())
			.ThenServer(TEXT("Only the old profile is unlinked; unrelated main animation ownership is preserved"), [this](FState&)
			{
				for (FBindingRecord& Record : BindingRecords)
				{
					ASSERT_THAT(IsNull(Record.Main->GetLinkedAnimLayerInstanceByClass(BlockProfile.Get())));
					Record.Layer = Equipment(Record.Character.Get())->GetBlockLocomotionLayerInstance();
				}
			})
			.ThenServer(TEXT("Reinitialize the authority gameplay animation while keeping mesh and equipment"), [this](FState& State) { ReinitializePresentation(Pawn(State.World, SubjectId)); })
			.ThenClient(TEXT("Reinitialize the owning gameplay animation"), 0, [this](FState& State) { ReinitializePresentation(Pawn(State.World, SubjectId)); })
			.ThenClient(TEXT("Reinitialize the late simulated proxy gameplay animation"), 1, [this](FState& State) { ReinitializePresentation(Pawn(State.World, SubjectId)); })
			.UntilServer(TEXT("All new main instances regain the configured layer without a new equipment grant"), [this](FState&)
				{ return BindingsReady(VariantProfile.Get(), true); }, Timeout())
			.ThenClient(TEXT("Activate the same equipment after its main and linked animation instances were recreated"), 0, [this](FState&) { Input.Block(true); })
			.UntilServer(TEXT("All recreated layers consume the real active-block snapshot"), [this](FState&)
			{
				if (BindingRecords.Num() != 3) return false;
				for (const FBindingRecord& Record : BindingRecords)
				{
					APawn* Character = Record.Character.Get();
					const URpgAnimInstance* Layer = Character && Equipment(Character) ? Cast<URpgAnimInstance>(Equipment(Character)->GetBlockLocomotionLayerInstance()) : nullptr;
					if (!Layer || Layer->GetClass() != VariantProfile.Get() || !Layer->bBlockLocomotionActive || !Blocking(Character)) return false;
				}
				return true;
			}, Timeout())
			.ThenServer(TEXT("Unequip the exact active source on authority, cancelling its actual granted block"), [this](FState& State)
			{
				APawn* Character = Pawn(State.World, SubjectId);
				URpgEquipmentManagerComponent* Manager = Equipment(Character);
				ASSERT_THAT(IsNotNull(Manager)); if (!Manager) return;
				URpgEquipmentInstance* Removed = Manager->GetActiveBlockSource();
				ASSERT_THAT(IsTrue(Removed && ASC(Character)->IsBlockMovementActive()));
				Manager->UnequipItem(Removed);
				ASSERT_THAT(IsFalse(ASC(Character)->IsBlockMovementActive()));
			})
			.ThenClient(TEXT("Release only the actual held key after authority removed its source"), 0, [this](FState&) { Input.Block(false); })
			.UntilServer(TEXT("FastArray removal releases the old profile and block state on all roles"), [this](FState&)
			{
				for (const FBindingRecord& Record : BindingRecords)
				{
					APawn* Character = Record.Character.Get();
					URpgEquipmentManagerComponent* Manager = Equipment(Character);
					if (!Character || !Manager || !ASC(Character) || ASC(Character) != Record.AbilitySystem.Get() || Mesh(Character) != Record.Mesh.Get()
						|| Blocking(Character) || ASC(Character)->IsBlockMovementActive()
						|| (Record.Item.IsValid() && Manager->GetActiveBlockSource() == Record.Item.Get())) return false;
					const URpgWeaponInstance* Current = Cast<URpgWeaponInstance>(Manager->GetActiveBlockSource());
					UClass* Desired = Current ? Current->GetBlockDefinition().BlockLocomotionLayer.Get() : nullptr;
					const UAnimInstance* Layer = Manager->GetBlockLocomotionLayerInstance();
					if (Desired ? !Layer || Layer->GetClass() != Desired : Layer != nullptr) return false;
				}
				return BindingRecords.Num() == 3;
			}, Timeout());
	}

	float NormalSpeed() const { return 375.0f; }
	void QueueMotion(const FString& Label, FVector2D Keys, bool bHeldBlock, bool bFacing, int32 Count,
		bool bCaptureSprint = false, bool bUseSprintBaseline = false)
	{
		using namespace RpgBlockLocomotionTests;
		// CQTest retains description pointers until latent execution; use static labels here.
		Network->ThenClient(TEXT("Send actual directional keys for the next movement probe"), 0,
			[this, Keys, bHeldBlock, bFacing, bUseSprintBaseline](FState&)
			{
				const APlayerController* Controller = Input.Subject() ? Cast<APlayerController>(Input.Subject()->GetController()) : nullptr;
				ASSERT_THAT(IsNotNull(Controller)); if (!Controller) return;
				ProbeYaw = Controller->GetControlRotation().Yaw;
				ProbeDirection = FRotator(0.0f, ProbeYaw, 0.0f).RotateVector(FVector(Keys.Y, Keys.X, 0.0)).GetSafeNormal2D();
				Input.MoveKeys(Keys);
				const float TargetSpeed = Keys.IsNearlyZero() ? 0.f : (bHeldBlock && BlockCap > 0.f ? FMath::Min(NormalSpeed(), BlockCap)
					: (bUseSprintBaseline ? SprintBaseline : NormalSpeed()));
				Observations.BeginMotionProbe(bHeldBlock, ProbeDirection, ProbeYaw, bFacing, TargetSpeed);
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
				const float ExpectedSpeed = Keys.IsNearlyZero() ? 0.0f : (bHeldBlock && BlockCap > 0.f ? FMath::Min(NormalSpeed(), BlockCap) : (bUseSprintBaseline || bCaptureSprint ? SprintBaseline : NormalSpeed()));
				const float SpeedTolerance = ExpectedSpeed > 0.0f ? FMath::Max(20.0f, ExpectedSpeed * 0.05f) : 5.0f;
				if (bCaptureSprint || bUseSprintBaseline)
					TestRunner->TestTrue(TEXT("Real unblocked Shift input has positively demonstrated faster sprint movement"), SprintBaseline > NormalSpeed() + 100.0f);
				for (const auto& Entry : Observations.GetPeers())
				{
					const FPeer& Peer = Entry.Value;
					const FMotionProbe& Probe = Peer.Motion;
					TestRunner->TestTrue(TEXT("The fixed settling budget comes from finite authored movement settings"), Probe.bValidSettlingContract);
					UE_LOG(LogTemp, Display, TEXT("RpgBlockLocomotion motion stage=%s world=%s role=%d samples=%d speed=%.2f/%.2f/%.2f expected=%.2f yawError=%.3f directionDot=%.4f distance=%.2f wrongBlock=%d offGround=%d"),
						*Label, *GetPathNameSafe(Entry.Key.Get()), static_cast<int32>(Peer.Character->GetLocalRole()), Probe.Samples,
						Probe.MinimumSpeed, Probe.MeanSpeed(), Probe.MaximumSpeed, ExpectedSpeed, Probe.MaximumYawError, Probe.MinimumDirectionDot,
						FVector::Distance(Probe.FirstLocation, Probe.LastLocation), Probe.bWrongBlockState, Probe.bLostGround);
					if (bHeldBlock)
					{
						UE_LOG(LogTemp, Display, TEXT("RpgBlockLocomotion armJoints stage=%s world=%s role=%d stableSamples=%d seconds=%.4f maximumOffsetCm=%.6f invalid=%d"),
							*Label, *GetPathNameSafe(Entry.Key.Get()), static_cast<int32>(Peer.Character->GetLocalRole()), Probe.Arms.Samples,
							Probe.Arms.LastTime - Probe.Arms.FirstTime, Probe.Arms.MaximumTranslationError, Probe.Arms.bInvalid);
						TestRunner->TestTrue(TEXT("Stable full-weight block preserves all six shoulder/arm local joint offsets on the target mesh"), Probe.Arms.Preserved());
					}
					TestRunner->TestTrue(TEXT("Movement samples retain the expected GAS and finalized movement block state and ground support"), !Probe.bWrongBlockState && !Probe.bLostGround && !Probe.bWrongCap);
					TestRunner->TestTrue(TEXT("Actual movement follows the activation's cap, or its uncapped ordinary gait after release"),
						Probe.MinimumSpeed >= ExpectedSpeed - SpeedTolerance && Probe.MaximumSpeed <= ExpectedSpeed + SpeedTolerance);
					if (bFacing) TestRunner->TestTrue(TEXT("After settling, the blocked body faces camera yaw independently of travel direction"), Probe.MaximumYawError <= 10.0f);
					if (ExpectedSpeed > 0.0f)
					{
						TestRunner->TestTrue(TEXT("Real displacement and velocity follow the camera-relative WASD direction"),
							Probe.MinimumDirectionDot > 0.95f && FVector::DotProduct(Probe.LastLocation - Probe.FirstLocation, ProbeDirection) > ExpectedSpeed * 0.5f);
						TestRunner->TestTrue(TEXT("Both legs actually animate during directional movement"), Probe.Legs.LegsMoved());
					}
					TestRunner->TestTrue(TEXT("Input changes preserve the gameplay mesh, ASC and equipment attachments"), !Peer.bSourceChanged && !Peer.bEquipmentChanged && !Peer.bRootMotion && !Peer.bInvalidPose && !Peer.bWrongLayer && !Peer.bLoopPlayed);
					if (!bHeldBlock) TestRunner->TestTrue(TEXT("The unblocked sample has cleared actual block presentation"), Observations.Released(Peer.Character.Get()));
				}
			});
	}
	void QueueLateJoin()
	{
		using namespace RpgBlockLocomotionTests;
		Network->ThenClientJoins()
			.UntilClient(TEXT("Late join receives the same already-held block"), 1, [this](FState& PeerState)
			{
				APawn* Character = Pawn(PeerState.World, SubjectId);
				return Ready(PeerState.World, Character, Variant) && Character->GetLocalRole() == ROLE_SimulatedProxy
					&& Blocking(Character) && Equipment(Character)->GetBlockLocomotionLayerInstance() && Equipment(Character)->GetBlockLocomotionLayerInstance()->GetClass() == BlockProfile.Get()
					&& EquipmentReady(Character, BlockSlot);
			}, Timeout())
			.ThenClient(TEXT("Observe the late simulated proxy without a second block press"), 1, [this](FState& PeerState)
				{ ASSERT_THAT(IsTrue(Isolation.IsIsolated(PeerState.World) && Observations.Add(PeerState.World, true))); });
	}
	void QueueLookWhileBlocking()
	{
		using namespace RpgBlockLocomotionTests;
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
		using namespace RpgBlockLocomotionTests;
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
					TestRunner->TestTrue(TEXT("Policy transitions preserve mesh, equipment and the selected layer without playing the legacy loop"), !Peer.bSourceChanged && !Peer.bEquipmentChanged && !Peer.bWrongLayer && !Peer.bLoopPlayed);
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
		using namespace RpgBlockLocomotionTests;
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
		using namespace RpgBlockLocomotionTests;
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
		using namespace RpgBlockLocomotionTests;
		QueueMotion(TEXT("CorrectionRunBaseline"), FVector2D(0, 1), false, false, 2);
		Network->ThenClient(TEXT("Retain real sprint input and temporarily delay only this owner's incoming packets"), 0, [this](FState&)
			{ ASSERT_THAT(IsTrue(ReleaseCorrection.DelayOwnerReceipts(Input.Subject()))); Input.Sprint(true); });
		QueueMotion(TEXT("CorrectionSprintBaseline"), FVector2D(0, 1), false, false, 2, true);
		Network->ThenClient(TEXT("Block the still-sprinting input through the real ability"), 0, [this](FState&)
			{ Observations.BeginBlockEpisode(); Input.Block(true); });
		QueueMotion(TEXT("CorrectionHeldBaseline"), FVector2D(0, 1), true, true, 2);
		Network->ThenClient(TEXT("Inject one coherent owner prediction error during the same active block"), 0, [this](FState&)
			{ ASSERT_THAT(IsTrue(HeldCorrection.Inject(Input.Subject(), false))); })
			.UntilClient(TEXT("A real Fixed restore/resimulation replaces the affected blocked history frame"), 0,
				[this](FState&) { return HeldCorrection.Observed(); }, Timeout())
			.ThenClient(TEXT("Rollback preserves the same active block, instance and simulation head"), 0,
				[this](FState&) { ASSERT_THAT(IsTrue(HeldCorrection.Preserved())); });
		QueueMotion(TEXT("AfterHeldCorrection"), FVector2D(0, 1), true, true, 2);
		Network->ThenClient(TEXT("Inject a second prediction error then release only RMB before its correction arrives"), 0, [this](FState&)
			{ ASSERT_THAT(IsTrue(ReleaseCorrection.Inject(Input.Subject(), true))); Input.Block(false); })
			.UntilClient(TEXT("Fixed replay reconstructs old blocked history while today's head and GAS activation are already released"), 0,
				[this](FState&) { return ReleaseCorrection.Observed(); }, Timeout())
			.ThenClient(TEXT("Release-crossing rollback preserves historical block and the newer unblocked head"), 0,
				[this](FState&) { ASSERT_THAT(IsTrue(ReleaseCorrection.Preserved())); });
		QueueMotion(TEXT("HeldSprintAfterReleaseCorrection"), FVector2D(0, 1), false, false, 2, false, true);
		QueuePolicyCleanup(1, 2);
	}
	void QueueAuthorityCancel()
	{
		using namespace RpgBlockLocomotionTests;
		QueueMotion(TEXT("AuthorityCancelRunBaseline"), FVector2D(0, 1), false, false, 2);
		Network->ThenClient(TEXT("Delay only the owner's real cancellation receipt and hold sprint"), 0, [this](FState&)
			{ ASSERT_THAT(IsTrue(ReleaseCorrection.DelayOwnerReceipts(Input.Subject(), 400))); Input.Sprint(true); });
		QueueMotion(TEXT("AuthorityCancelSprintBaseline"), FVector2D(0, 1), false, false, 2, true);
		Network->ThenClient(TEXT("Enter predicted block with unchanged held Shift"), 0, [this](FState&)
			{ Observations.BeginBlockEpisode(); Input.Block(true); });
		QueueMotion(TEXT("AuthorityCancelHeldBaseline"), FVector2D(0, 1), true, true, 2);
		Network->ThenClient(TEXT("Passively retain the owner's real blocked prediction history before server cancellation"), 0, [this](FState&)
			{ ASSERT_THAT(IsTrue(ReleaseCorrection.Inject(Input.Subject(), true, true))); })
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
					UE_LOG(LogTemp, Display, TEXT("RpgBlockLocomotion authorityCancel staleWindow expired samples=%d maximumAuthoritySpeed=%.2f requiredAbove=%.2f gaitMismatch=%d"),
						CancelSamples, MaximumAuthoritySpeedBeforeReceipt, BlockCap + 10.0f, bCancelGaitMismatch);
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
				UE_LOG(LogTemp, Display, TEXT("RpgBlockLocomotion authorityCancel staleWindow sample=%d serverFrame=%d ownerLease=%d ownerHistory=%d authoritySync=%d rawGait=%s serverGait=%s serverSpeed=%.2f"),
					CancelSamples, Frame, ASC(Owner)->IsBlockMovementActive(), OwnerHistory->bBlocking, AuthorityBlock->bBlocking, *RawGait, *ServerGait, Speed(Character));
				// Frame cadence and smoothing vary: require the actual acceleration witness inside the
				// still-predicted window, rather than assuming that exactly three frames already suffice.
				return CancelSamples >= 3 && MaximumAuthoritySpeedBeforeReceipt > BlockCap + 10.0f;
			}, Timeout())
			.ThenClient(TEXT("Release the actual owner RMB before delayed cancellation arrives; leave Shift held"), 0,
				[this](FState&) { Input.Block(false); })
			.ThenServer(TEXT("Authority ignored the stale client block state without consuming its raw sprint intent"), [this](FState&)
			{
				TestRunner->TestTrue(TEXT("The witness contains at least three real authority frames before owner cancellation receipt"), !bCancelWindowExpired && CancelSamples >= 3);
				TestRunner->TestFalse(TEXT("Both actual raw owner input and unblocked server simulation retain Sprint"), bCancelGaitMismatch);
				// Sprint accelerates at 300 cm/s²; the deliberately short pre-receipt window cannot reach full sprint.
				TestRunner->TestTrue(TEXT("Authority has really accelerated above blocked Run before owner cancellation receipt"), MaximumAuthoritySpeedBeforeReceipt > BlockCap + 10.0f);
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
