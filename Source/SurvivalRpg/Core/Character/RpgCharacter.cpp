// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgCharacter.h"

#include "RpgCharacterMovementComponent.h"
#include "RpgDeathComponent.h"
#include "RpgDownedComponent.h"
#include "RpgHealthComponent.h"
#include "RpgPawnExtensionComponent.h"
#include "RpgPawnGameplayComponent.h"
#include "RpgPawnData.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Controller.h"
#include "GameplayTagContainer.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgHealthSet.h"
#include "SurvivalRpg/Camera/RpgCameraComponent.h"
#include "SurvivalRpg/Core/Game/RpgGameModeBase.h"
#include "SurvivalRpg/Core/Corpse/RpgCorpseLifecycleComponent.h"
#include "SurvivalRpg/Equipment/RpgEquipmentManagerComponent.h"
#include "SurvivalRpg/Equipment/RpgWeaponInstance.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "Net/UnrealNetwork.h"

ARpgCharacter::ARpgCharacter(const FObjectInitializer& ObjectInitializer) : 
	Super(ObjectInitializer.SetDefaultSubobjectClass<URpgCharacterMovementComponent>(CharacterMovementComponentName))
{
	// Avoid ticking characters if possible.
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	GetCapsuleComponent()->SetCollisionProfileName(TEXT("RpgPawnCapsule"));
	GetMesh()->SetCollisionProfileName(TEXT("RpgPawnMesh"));
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	GetMesh()->bEnableUpdateRateOptimizations = false;
	
	SetNetCullDistanceSquared(900000000.0f);
	
	URpgCharacterMovementComponent* RpgMoveComp = CastChecked<URpgCharacterMovementComponent>(ACharacter::GetMovementComponent());
	RpgMoveComp->GravityScale = 1.0f;
	RpgMoveComp->MaxAcceleration = 2400.0f;
	RpgMoveComp->BrakingFrictionFactor = 1.0f;
	RpgMoveComp->BrakingFriction = 6.0f;
	RpgMoveComp->GroundFriction = 8.0f;
	RpgMoveComp->BrakingDecelerationWalking = 1400.0f;
	RpgMoveComp->bUseControllerDesiredRotation = false;
	RpgMoveComp->bOrientRotationToMovement = false;
	RpgMoveComp->RotationRate = FRotator(0.0f, 720.0f, 0.0f);
	RpgMoveComp->bAllowPhysicsRotationDuringAnimRootMotion = false;
	RpgMoveComp->GetNavAgentPropertiesRef().bCanCrouch = true;
	RpgMoveComp->bCanWalkOffLedgesWhenCrouching = true;
	RpgMoveComp->SetCrouchedHalfHeight(65.0f);
	

	
	PawnExtensionComponent = CreateDefaultSubobject<URpgPawnExtensionComponent>(TEXT("PawnExtensionComponent"));
	PawnExtensionComponent->OnPawnDataReady_RegisterAndCall(FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::HandlePawnDataReady));
	PawnExtensionComponent->OnAbilitySystemInitialized_RegisterAndCall(FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::OnAbilitySystemInitialized));
	PawnExtensionComponent->OnAbilitySystemUninitialized_Register(FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::OnAbilitySystemUninitialized));
	
	EquipmentManagerComponent = CreateDefaultSubobject<URpgEquipmentManagerComponent>(TEXT("EquipmentManagerComponent"));
	EquipmentManagerComponent->OnEquipmentChanged.AddUObject(this, &ThisClass::HandleEquipmentChanged);
	
	HealthComponent = CreateDefaultSubobject<URpgHealthComponent>(TEXT("HealthComponent"));
	HealthComponent->OnDeathStarted.AddDynamic(this, &ThisClass::OnDeathStarted);
	HealthComponent->OnDeathFinished.AddDynamic(this, &ThisClass::OnDeathFinished);
	
	DeathComponent = CreateDefaultSubobject<URpgDeathComponent>(TEXT("DeathComponent"));
	DownedComponent = CreateDefaultSubobject<URpgDownedComponent>(TEXT("DownedComponent"));
	DownedComponent->OnDownedStateChanged.AddDynamic(this, &ThisClass::OnDownedStateChanged);
	
	CameraComponent = CreateDefaultSubobject<URpgCameraComponent>(TEXT("CameraComponent"));
	CameraComponent->SetRelativeLocation(FVector(-300.0f, 0.0f, 75.0f));
	
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	BaseEyeHeight = 80.0f;
	CrouchedEyeHeight = 50.0f;
}

ARpgPlayerController* ARpgCharacter::GetRpgPlayerController() const
{
	return Cast<ARpgPlayerController>(GetController());
}

ARpgPlayerState* ARpgCharacter::GetRpgPlayerState() const
{
	return Cast<ARpgPlayerState>(GetPlayerState());
}

URpgAbilitySystemComponent* ARpgCharacter::GetRpgAbilitySystemComponent() const
{
	check(PawnExtensionComponent);
	return Cast<URpgAbilitySystemComponent>(GetAbilitySystemComponent());
}

UAbilitySystemComponent* ARpgCharacter::GetAbilitySystemComponent() const
{
	if (PawnExtensionComponent == nullptr) return nullptr;
	
	return PawnExtensionComponent->GetRpgAbilitySystemComponent();
}

void ARpgCharacter::ToggleCrouch()
{
	const URpgCharacterMovementComponent* RpgMoveComp = CastChecked<URpgCharacterMovementComponent>(GetCharacterMovement());

	if (IsCrouched() || RpgMoveComp->bWantsToCrouch)
	{
		UnCrouch();
	}
	else if (RpgMoveComp->IsMovingOnGround())
	{
		Crouch();
	}
}

ERpgCharacterRotationMode ARpgCharacter::GetRotationMode() const
{
	// Predicted activation-owned tags may upgrade the owning client's presentation before the
	// replicated server mode arrives. In their absence the server value remains the reconciliation baseline.
	if (GetLocalRole() == ROLE_AutonomousProxy && IsLocallyControlled())
	{
		if (RotationMode == ERpgCharacterRotationMode::Aim)
		{
			return ERpgCharacterRotationMode::Aim;
		}

		if (const URpgAbilitySystemComponent* Asc = RotationModeAbilitySystem.Get())
		{
			if (Asc->HasMatchingGameplayTag(RpgGameplayTags::State_Rotation_Aim))
			{
				return ERpgCharacterRotationMode::Aim;
			}

			if (Asc->HasMatchingGameplayTag(RpgGameplayTags::State_Rotation_CombatStrafe))
			{
				return ERpgCharacterRotationMode::CombatStrafe;
			}
		}
	}

	return RotationMode;
}

ERpgCharacterRotationMode ARpgCharacter::ResolveRotationMode(
	bool bAimRequested,
	bool bCombatStrafeRequested,
	ERpgCharacterRotationMode DefaultMode)
{
	if (bAimRequested || DefaultMode == ERpgCharacterRotationMode::Aim)
	{
		return ERpgCharacterRotationMode::Aim;
	}

	if (bCombatStrafeRequested || DefaultMode == ERpgCharacterRotationMode::CombatStrafe)
	{
		return ERpgCharacterRotationMode::CombatStrafe;
	}

	return ERpgCharacterRotationMode::Free;
}

FRpgCharacterRotationPolicy ARpgCharacter::GetRotationPolicy(
	ERpgCharacterRotationMode InRotationMode,
	float FreeRotationRateYaw)
{
	FRpgCharacterRotationPolicy Policy;
	if (InRotationMode == ERpgCharacterRotationMode::Free)
	{
		Policy.bUseControllerRotationYaw = false;
		Policy.bOrientRotationToMovement = true;
		Policy.bUseControllerDesiredRotation = false;
		Policy.RotationRateYaw = FMath::IsFinite(FreeRotationRateYaw) &&
			(FreeRotationRateYaw == -1.0f || FreeRotationRateYaw > 0.0f)
			? FreeRotationRateYaw
			: -1.0f;
	}
	else
	{
		Policy.bUseControllerRotationYaw = true;
		Policy.bOrientRotationToMovement = false;
		Policy.bUseControllerDesiredRotation = false;
		Policy.RotationRateYaw = 720.0f;
	}

	return Policy;
}

bool ARpgCharacter::CanApplyExplicitCombatStanceRequest(
	bool bEnable,
	bool bHasWeapon,
	bool bHasBlockingState,
	bool bIsMovingOnGround,
	bool bIsCrouched,
	bool bWantsToCrouch,
	bool bIsAnyMontagePlaying)
{
	return !bEnable ||
		(bHasWeapon &&
			!bHasBlockingState &&
			bIsMovingOnGround &&
			!bIsCrouched &&
			!bWantsToCrouch &&
			!bIsAnyMontagePlaying);
}

void ARpgCharacter::ToggleCombatStance()
{
	if (HasAuthority())
	{
		SetExplicitCombatStance(!bExplicitCombatStanceRequested);
		return;
	}

	if (GetLocalRole() == ROLE_AutonomousProxy && IsLocallyControlled())
	{
		ServerToggleCombatStance();
	}
}

void ARpgCharacter::ServerToggleCombatStance_Implementation()
{
	SetExplicitCombatStance(!bExplicitCombatStanceRequested);
}

ERpgCharacterRotationMode ARpgCharacter::ResolveRequestedRotationMode() const
{
	const URpgAbilitySystemComponent* Asc = RotationModeAbilitySystem.Get();
	return ResolveRotationMode(
		Asc && Asc->HasMatchingGameplayTag(RpgGameplayTags::State_Rotation_Aim),
		Asc && Asc->HasMatchingGameplayTag(RpgGameplayTags::State_Rotation_CombatStrafe),
		GetDefaultRotationMode());
}

ERpgCharacterRotationMode ARpgCharacter::GetDefaultRotationMode() const
{
	if (PawnExtensionComponent)
	{
		if (const URpgPawnData* PawnData = PawnExtensionComponent->GetPawnData<URpgPawnData>())
		{
			return PawnData->DefaultRotationMode;
		}
	}

	return ERpgCharacterRotationMode::CombatStrafe;
}

void ARpgCharacter::RefreshRotationMode()
{
	if (HasAuthority())
	{
		const ERpgCharacterRotationMode ResolvedMode = ResolveRequestedRotationMode();
		if (RotationMode != ResolvedMode)
		{
			RotationMode = ResolvedMode;
			AdvanceRotationModeRevision();
		}
	}

	ApplyRotationPolicy(GetRotationMode());
}

void ARpgCharacter::ApplyRotationPolicy(ERpgCharacterRotationMode InRotationMode)
{
	URpgCharacterMovementComponent* MovementComponent =
		Cast<URpgCharacterMovementComponent>(GetCharacterMovement());
	float FreeRotationRateYaw = -1.0f;
	if (MovementComponent)
	{
		const FRpgCharacterMovementProfile& MovementProfile =
			MovementComponent->GetMovementProfile();
		if (MovementProfile.bOverrideCharacterMovement)
		{
			FreeRotationRateYaw = MovementProfile.FreeRotationRateYaw;
		}
	}
	const FRpgCharacterRotationPolicy Policy = GetRotationPolicy(
		InRotationMode,
		FreeRotationRateYaw);
	const bool bEnteringControllerFacingMode =
		bHasAppliedRotationPolicy &&
		LastAppliedRotationMode == ERpgCharacterRotationMode::Free &&
		InRotationMode != ERpgCharacterRotationMode::Free;

	bUseControllerRotationYaw = Policy.bUseControllerRotationYaw;
	if (MovementComponent)
	{
		MovementComponent->bOrientRotationToMovement = Policy.bOrientRotationToMovement;
		MovementComponent->bUseControllerDesiredRotation = Policy.bUseControllerDesiredRotation;
		MovementComponent->RotationRate.Yaw = Policy.RotationRateYaw;
	}

	if (bEnteringControllerFacingMode &&
		(HasAuthority() || GetLocalRole() == ROLE_AutonomousProxy) &&
		GetController())
	{
		FaceRotation(GetController()->GetControlRotation(), 0.0f);
	}

	LastAppliedRotationMode = InRotationMode;
	bHasAppliedRotationPolicy = true;
	RequestFreeRotationSynchronization(InRotationMode);
}

void ARpgCharacter::AdvanceRotationModeRevision()
{
	check(HasAuthority());
	if (++RotationModeRevision == 0)
	{
		++RotationModeRevision;
	}
	if (URpgCharacterMovementComponent* MovementComponent =
		Cast<URpgCharacterMovementComponent>(GetCharacterMovement()))
	{
		MovementComponent->CancelOwnerRotationSynchronization();
	}
	ForceNetUpdate();
}

void ARpgCharacter::RequestFreeRotationSynchronization(ERpgCharacterRotationMode AppliedMode)
{
	if (GetLocalRole() != ROLE_AutonomousProxy || !IsLocallyControlled())
	{
		return;
	}
	URpgCharacterMovementComponent* MovementComponent =
		Cast<URpgCharacterMovementComponent>(GetCharacterMovement());
	if (!MovementComponent)
	{
		return;
	}
	if (AppliedMode != ERpgCharacterRotationMode::Free ||
		RotationMode != ERpgCharacterRotationMode::Free || RotationModeRevision == 0)
	{
		// A predicted combat tag may temporarily override replicated Free. A later rejection
		// must be allowed to request a fresh handoff even if the authority revision is unchanged.
		RequestedFreeRotationRevision = 0;
		MovementComponent->CancelOwnerRotationSynchronization();
		return;
	}
	if (RequestedFreeRotationRevision == RotationModeRevision)
	{
		return;
	}
	const float ClientMoveTimeStamp = MovementComponent->GetCurrentOwnerMoveTimeStamp();
	if (!FMath::IsFinite(ClientMoveTimeStamp) || ClientMoveTimeStamp < 0.0f)
	{
		return;
	}
	RequestedFreeRotationRevision = RotationModeRevision;
	MovementComponent->RequestOwnerRotationSynchronization(ClientMoveTimeStamp);
	ServerAcknowledgeFreeRotationMode(RotationModeRevision, ClientMoveTimeStamp);
}

void ARpgCharacter::ServerAcknowledgeFreeRotationMode_Implementation(
	uint16 Revision, float ClientMoveTimeStamp)
{
	if (Revision == 0 || Revision != RotationModeRevision ||
		RotationMode != ERpgCharacterRotationMode::Free)
	{
		return;
	}
	if (URpgCharacterMovementComponent* MovementComponent =
		Cast<URpgCharacterMovementComponent>(GetCharacterMovement()))
	{
		// The next strictly newer move was authored after Free was applied on the owner.
		// CMC corrects at that move's timestamp so replay never starts at an OnRep-only yaw.
		MovementComponent->RequestOwnerRotationSynchronization(ClientMoveTimeStamp);
	}
}

void ARpgCharacter::NotifyOwnerRotationCorrectionReceived(float ClientMoveTimeStamp)
{
	URpgCharacterMovementComponent* MovementComponent =
		Cast<URpgCharacterMovementComponent>(GetCharacterMovement());
	if (GetLocalRole() != ROLE_AutonomousProxy || !IsLocallyControlled() ||
		RequestedFreeRotationRevision == 0 ||
		RequestedFreeRotationRevision != RotationModeRevision ||
		GetRotationMode() != ERpgCharacterRotationMode::Free || !MovementComponent ||
		!MovementComponent->IsOwnerRotationSynchronizationCorrection(ClientMoveTimeStamp))
	{
		return;
	}
	ServerConfirmFreeRotationSynchronization(RotationModeRevision, ClientMoveTimeStamp);
	MovementComponent->CancelOwnerRotationSynchronization();
}

void ARpgCharacter::ServerConfirmFreeRotationSynchronization_Implementation(
	uint16 Revision, float ClientMoveTimeStamp)
{
	URpgCharacterMovementComponent* MovementComponent =
		Cast<URpgCharacterMovementComponent>(GetCharacterMovement());
	if (Revision != 0 && Revision == RotationModeRevision &&
		RotationMode == ERpgCharacterRotationMode::Free && MovementComponent &&
		MovementComponent->IsOwnerRotationSynchronizationCorrection(ClientMoveTimeStamp))
	{
		MovementComponent->CancelOwnerRotationSynchronization();
	}
}

void ARpgCharacter::HandlePawnDataReady()
{
	if (!PawnExtensionComponent)
	{
		return;
	}

	const URpgPawnData* PawnData = PawnExtensionComponent->GetPawnData<URpgPawnData>();
	URpgCharacterMovementComponent* MovementComponent =
		Cast<URpgCharacterMovementComponent>(GetCharacterMovement());
	if (!PawnData)
	{
		return;
	}

	const bool bMovementProfileApplied = MovementComponent &&
		MovementComponent->ApplyMovementProfile(PawnData->MovementProfile);
	ensureMsgf(
		bMovementProfileApplied,
		TEXT("PawnData '%s' supplied an invalid movement profile or an incompatible movement component for '%s'."),
		*GetNameSafe(PawnData),
		*GetNameSafe(this));

	// Rotation composition is independent of movement-profile validity. A malformed physical
	// field must not suppress the PawnData-selected Free/CombatStrafe/Aim policy.
	RefreshRotationMode();
}

void ARpgCharacter::BindRotationModeAbilitySystem(URpgAbilitySystemComponent* AbilitySystemComponent)
{
	if (RotationModeAbilitySystem.Get() == AbilitySystemComponent)
	{
		RefreshRotationMode();
		return;
	}

	UnbindRotationModeAbilitySystem();
	RotationModeAbilitySystem = AbilitySystemComponent;
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->RegisterGameplayTagEvent(
			RpgGameplayTags::State_Rotation_CombatStrafe,
			EGameplayTagEventType::NewOrRemoved).AddUObject(
				this,
				&ThisClass::HandleRotationRequestTagChanged);
		AbilitySystemComponent->RegisterGameplayTagEvent(
			RpgGameplayTags::State_Rotation_Aim,
			EGameplayTagEventType::NewOrRemoved).AddUObject(
				this,
				&ThisClass::HandleRotationRequestTagChanged);
	}

	RefreshRotationMode();
}

void ARpgCharacter::UnbindRotationModeAbilitySystem()
{
	if (URpgAbilitySystemComponent* AbilitySystemComponent = RotationModeAbilitySystem.Get())
	{
		AbilitySystemComponent->RegisterGameplayTagEvent(
			RpgGameplayTags::State_Rotation_CombatStrafe,
			EGameplayTagEventType::NewOrRemoved).RemoveAll(this);
		AbilitySystemComponent->RegisterGameplayTagEvent(
			RpgGameplayTags::State_Rotation_Aim,
			EGameplayTagEventType::NewOrRemoved).RemoveAll(this);
	}

	RotationModeAbilitySystem.Reset();
}

void ARpgCharacter::HandleRotationRequestTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	(void)Tag;
	(void)NewCount;
	RefreshRotationMode();
}

bool ARpgCharacter::CanEnterExplicitCombatStance() const
{
	if (!HasAuthority())
	{
		return false;
	}

	const URpgAbilitySystemComponent* Asc = RotationModeAbilitySystem.Get();
	static const FGameplayTag MovementStoppedTag =
		FGameplayTag::RequestGameplayTag(FName(TEXT("Gameplay.MovementStopped")), false);
	const bool bHasBlockingState = !Asc ||
		Asc->HasMatchingGameplayTag(RpgGameplayTags::State_Dead) ||
		Asc->HasMatchingGameplayTag(RpgGameplayTags::State_Staggered) ||
		Asc->HasMatchingGameplayTag(RpgGameplayTags::State_GuardBroken) ||
		Asc->HasMatchingGameplayTag(RpgGameplayTags::Status_Downed) ||
		(MovementStoppedTag.IsValid() && Asc->HasMatchingGameplayTag(MovementStoppedTag));
	const UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	const UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;

	return CanApplyExplicitCombatStanceRequest(
		/*bEnable=*/ true,
		EquipmentManagerComponent &&
			EquipmentManagerComponent->GetFirstInstanceOfType(URpgWeaponInstance::StaticClass()) != nullptr,
		bHasBlockingState,
		MovementComponent && MovementComponent->IsMovingOnGround(),
		IsCrouched(),
		MovementComponent && MovementComponent->bWantsToCrouch,
		AnimInstance && AnimInstance->IsAnyMontagePlaying());
}

void ARpgCharacter::SetExplicitCombatStance(bool bEnabled)
{
	if (!HasAuthority() || bExplicitCombatStanceRequested == bEnabled)
	{
		return;
	}

	URpgAbilitySystemComponent* Asc = RotationModeAbilitySystem.Get();
	if (bEnabled)
	{
		if (!Asc || !CanEnterExplicitCombatStance())
		{
			return;
		}

		bExplicitCombatStanceRequested = true;
		Asc->AddLooseGameplayTag(
			RpgGameplayTags::State_Rotation_CombatStrafe,
			1,
			EGameplayTagReplicationState::None);
	}
	else
	{
		bExplicitCombatStanceRequested = false;
		if (Asc)
		{
			Asc->RemoveLooseGameplayTag(
				RpgGameplayTags::State_Rotation_CombatStrafe,
				1,
				EGameplayTagReplicationState::None);
		}
	}

	RefreshRotationMode();
}

void ARpgCharacter::HandleEquipmentChanged()
{
	if (HasAuthority() &&
		bExplicitCombatStanceRequested &&
		(!EquipmentManagerComponent ||
			!EquipmentManagerComponent->GetFirstInstanceOfType(URpgWeaponInstance::StaticClass())))
	{
		SetExplicitCombatStance(false);
	}
}

void ARpgCharacter::OnRep_RotationMode()
{
	ApplyRotationPolicy(GetRotationMode());
}

// Called when the game starts or when spawned
void ARpgCharacter::BeginPlay()
{
	Super::BeginPlay();
	RefreshRotationMode();
}

void ARpgCharacter::TeleportSucceeded(bool bIsATest)
{
	Super::TeleportSucceeded(bIsATest);
	if (bIsATest || !HasAuthority())
	{
		return;
	}

	++AnimationTeleportEpoch;
	if (AnimationTeleportEpoch == 0)
	{
		++AnimationTeleportEpoch;
	}
	ForceNetUpdate();
}

void ARpgCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ThisClass, AnimationTeleportEpoch, COND_SimulatedOnly);
	DOREPLIFETIME_CONDITION_NOTIFY(
		ThisClass,
		GroundMovementGait,
		COND_SimulatedOnly,
		REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(ThisClass, RotationMode, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(ThisClass, RotationModeRevision, COND_OwnerOnly, REPNOTIFY_Always);
}

bool ARpgCharacter::ShouldReplicateAcceleration() const
{
	return true;
}

void ARpgCharacter::OnRep_AnimationTeleportEpoch()
{
	if (URpgCharacterMovementComponent* MovementComponent =
		Cast<URpgCharacterMovementComponent>(GetCharacterMovement()))
	{
		MovementComponent->NotifyReplicatedAnimationTeleport();
	}
}

void ARpgCharacter::OnRep_GroundMovementGait()
{
	if (URpgCharacterMovementComponent* MovementComponent =
		Cast<URpgCharacterMovementComponent>(GetCharacterMovement()))
	{
		MovementComponent->NotifyReplicatedGroundMovementGait(GroundMovementGait);
	}
}

void ARpgCharacter::SetAuthoritativeGroundMovementGait(
	ERpgLocomotionGait NewCoastGait)
{
	if (!HasAuthority())
	{
		return;
	}

	if (NewCoastGait != ERpgLocomotionGait::Walk &&
		NewCoastGait != ERpgLocomotionGait::Run)
	{
		NewCoastGait = ERpgLocomotionGait::Idle;
	}

	if (GroundMovementGait != NewCoastGait)
	{
		GroundMovementGait = NewCoastGait;
		ForceNetUpdate();
	}
}

void ARpgCharacter::OnAbilitySystemInitialized()
{
	URpgAbilitySystemComponent* Asc = GetRpgAbilitySystemComponent();
	check(Asc);
	BindRotationModeAbilitySystem(Asc);

	HealthComponent->InitializeWithAbilitySystem(Asc);
	DeathComponent->InitializeWithAbilitySystem(Asc);
	DownedComponent->InitializeWithAbilitySystem(Asc);

	if (HasAuthority())
	{
		if (const URpgHealthSet* HealthSet = Asc->GetSet<URpgHealthSet>())
		{
			Asc->SetNumericAttributeBase(URpgHealthSet::GetHealthAttribute(), HealthSet->GetMaxHealth());
		}
	}

	Asc->SetLooseGameplayTagCount(RpgGameplayTags::Status_Crouching, IsCrouched() ? 1 : 0);
}

void ARpgCharacter::OnAbilitySystemUninitialized()
{
	if (HasAuthority())
	{
		SetExplicitCombatStance(false);
	}
	UnbindRotationModeAbilitySystem();

	HealthComponent->UninitializeFromAbilitySystem();
	DeathComponent->UninitializeFromAbilitySystem();
	DownedComponent->UninitializeFromAbilitySystem();

	if (URpgAbilitySystemComponent* Asc = GetRpgAbilitySystemComponent())
	{
		Asc->SetLooseGameplayTagCount(RpgGameplayTags::Status_Crouching, 0);
	}
}

void ARpgCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	PawnExtensionComponent->HandleControllerChanged();
	AdvanceRotationModeRevision();
	RefreshRotationMode();
}

void ARpgCharacter::UnPossessed()
{
	RequestedFreeRotationRevision = 0;
	if (URpgCharacterMovementComponent* MovementComponent =
		Cast<URpgCharacterMovementComponent>(GetCharacterMovement()))
	{
		MovementComponent->CancelOwnerRotationSynchronization();
	}
	Super::UnPossessed();
	PawnExtensionComponent->HandleControllerChanged();
}

void ARpgCharacter::OnRep_Controller()
{
	Super::OnRep_Controller();
	RequestedFreeRotationRevision = 0;
	PawnExtensionComponent->HandleControllerChanged();
	RefreshRotationMode();
}

void ARpgCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	PawnExtensionComponent->HandlePlayerStateReplicated();

	if (URpgPawnGameplayComponent* PawnGameplayComponent = FindComponentByClass<URpgPawnGameplayComponent>())
	{
		PawnGameplayComponent->CheckDefaultInitialization();
	}

	RefreshRotationMode();
}

void ARpgCharacter::FellOutOfWorld(const class UDamageType& dmgType)
{
	HealthComponent->DamageSelfDestruct(/*bFellOutOfWorld=*/ true);
}

void ARpgCharacter::OnDeathStarted(AActor* OwningActor)
{
	if (HasAuthority())
	{
		SetExplicitCombatStance(false);
	}
	DisableMovementAndCollision();
}

void ARpgCharacter::OnDeathFinished(AActor* OwningActor)
{
	if (!HasAuthority())
	{
		return;
	}

	if (ARpgPlayerController* PC = GetRpgPlayerController())
	{
		if (ARpgGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ARpgGameModeBase>() : nullptr)
		{
			GameMode->NotifyPlayerDeath(PC);
		}

		return;
	}

	EnterDeadState();

	// Preserve the pre-corpse behavior for legacy non-player pawn classes. All RPG AI characters
	// own a corpse lifecycle component and therefore use its configurable hard lifetime instead.
	if (!FindComponentByClass<URpgCorpseLifecycleComponent>())
	{
		SetLifeSpan(8.0f);
	}
}

void ARpgCharacter::OnDownedStateChanged(ERpgDownedState NewState)
{
	if (NewState == ERpgDownedState::Downed)
	{
		if (HasAuthority())
		{
			SetExplicitCombatStance(false);
		}
		DisableMovementForDowned();
		return;
	}

	if (!HealthComponent->IsDeadOrDying())
	{
		RestoreMovementAndCollision();
	}
}

void ARpgCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	if (URpgAbilitySystemComponent* Asc = GetRpgAbilitySystemComponent())
	{
		Asc->SetLooseGameplayTagCount(RpgGameplayTags::Status_Crouching, 1);
	}

	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
}

void ARpgCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	if (URpgAbilitySystemComponent* Asc = GetRpgAbilitySystemComponent())
	{
		Asc->SetLooseGameplayTagCount(RpgGameplayTags::Status_Crouching, 0);
	}

	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
}

bool ARpgCharacter::CanJumpInternal_Implementation() const
{
	return JumpIsAllowedInternal();
}

void ARpgCharacter::DisableMovementAndCollision() const
{
	if (GetController())
	{
		GetController()->SetIgnoreMoveInput(true);
	}

	UCapsuleComponent* CapsuleComp = GetCapsuleComponent();
	check(CapsuleComp);
	CapsuleComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CapsuleComp->SetCollisionResponseToAllChannels(ECR_Ignore);

	URpgCharacterMovementComponent* MoveComp = Cast<URpgCharacterMovementComponent>(GetCharacterMovement());
	MoveComp->StopMovementImmediately();
	MoveComp->DisableMovement();
}

void ARpgCharacter::DisableMovementForDowned() const
{
	if (GetController())
	{
		GetController()->SetIgnoreMoveInput(true);
	}

	if (URpgCharacterMovementComponent* MoveComp = Cast<URpgCharacterMovementComponent>(GetCharacterMovement()))
	{
		MoveComp->StopMovementImmediately();
		MoveComp->DisableMovement();
	}
}

void ARpgCharacter::RestoreMovementAndCollision() const
{
	if (GetController())
	{
		GetController()->SetIgnoreMoveInput(false);
	}

	UCapsuleComponent* CapsuleComp = GetCapsuleComponent();
	check(CapsuleComp);
	CapsuleComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	if (URpgCharacterMovementComponent* MoveComp = Cast<URpgCharacterMovementComponent>(GetCharacterMovement()))
	{
		MoveComp->SetMovementMode(MOVE_Walking);
	}
}

void ARpgCharacter::EnterDeadState()
{
	DetachFromControllerPendingDestroy();
}

// Called to bind functionality to input
void ARpgCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	PawnExtensionComponent->SetupPlayerInputComponent();
}



