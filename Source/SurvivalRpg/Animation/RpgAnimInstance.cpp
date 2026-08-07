// Copyright Epic Games, Inc. All Rights Reserved.

#include "RpgAnimInstance.h"
#include "AbilitySystemGlobals.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PoseSearch/AnimNode_MotionMatching.h"
#include "PoseSearch/PoseSearchTrajectoryLibrary.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "SurvivalRpg/Core/Character/RpgCharacter.h"
#include "SurvivalRpg/Core/Character/RpgCharacterMovementComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgAnimInstance)


URpgAnimInstance::URpgAnimInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool URpgAnimInstance::CanRunParallelWork() const
{
	if (!Super::CanRunParallelWork())
	{
		return false;
	}

	const ARpgCharacter* Character = Cast<ARpgCharacter>(TryGetPawnOwner());
	const USkeletalMeshComponent* MeshComponent = GetSkelMeshComponent();
	const UWorld* World = GetWorld();

	const bool bIsRemoteAutonomousMoveOnListenServer =
		World &&
		World->GetNetMode() == NM_ListenServer &&
		Character &&
		Character->GetLocalRole() == ROLE_Authority &&
		Character->GetRemoteRole() == ROLE_AutonomousProxy &&
		MeshComponent &&
		MeshComponent->bOnlyAllowAutonomousTickPose &&
		MeshComponent->bIsAutonomousTickPose;

	// Parallel updates collapse several autonomous move pose ticks into the last move delta.
	// Updating this narrow path immediately preserves the full animation time and notify order.
	return !bIsRemoteAutonomousMoveOnListenServer;
}

void FRpgAnimInstanceProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	Super::PreUpdate(InAnimInstance, DeltaSeconds);

	WorldVelocity = FVector::ZeroVector;
	LocalVelocity = FVector::ZeroVector;
	WorldAcceleration = FVector::ZeroVector;
	LocalAcceleration = FVector::ZeroVector;
	GroundSpeed = 0.0f;
	VerticalVelocity = 0.0f;
	GroundDistance = -1.0f;
	AimYaw = 0.0f;
	AimPitch = 0.0f;
	LocomotionAngle = 0.0f;
	ProceduralLocomotionAlpha = 0.0f;
	Gait = ERpgLocomotionGait::Idle;
	Stance = ERpgLocomotionStance::Standing;
	MovementState = ERpgLocomotionMovementState::None;
	bHasVelocity = false;
	bHasAcceleration = false;
	bIsFalling = false;
	bIsMovingOnGround = false;
	bIsCrouching = false;
	bIsAnyMontagePlaying = false;

	const URpgAnimInstance* RpgAnimInstance = Cast<URpgAnimInstance>(InAnimInstance);
	const ARpgCharacter* Character = RpgAnimInstance ? Cast<ARpgCharacter>(RpgAnimInstance->TryGetPawnOwner()) : nullptr;
	if (!Character)
	{
		TransformTrajectory.Samples.Reset();
		DesiredControllerYawLastUpdate = 0.0f;
		return;
	}

	URpgCharacterMovementComponent* MovementComponent = Cast<URpgCharacterMovementComponent>(Character->GetCharacterMovement());
	if (!MovementComponent)
	{
		TransformTrajectory.Samples.Reset();
		DesiredControllerYawLastUpdate = 0.0f;
		return;
	}

	const FQuat ActorRotation = Character->GetActorQuat();
	WorldVelocity = MovementComponent->Velocity;
	LocalVelocity = ActorRotation.UnrotateVector(WorldVelocity);
	WorldAcceleration = MovementComponent->GetCurrentAcceleration();
	LocalAcceleration = ActorRotation.UnrotateVector(WorldAcceleration);
	GroundSpeed = WorldVelocity.Size2D();
	VerticalVelocity = WorldVelocity.Z;
	GroundDistance = MovementComponent->GetGroundInfo().GroundDistance;
	bHasVelocity = !WorldVelocity.IsNearlyZero();
	bHasAcceleration = !WorldAcceleration.IsNearlyZero();
	bIsFalling = MovementComponent->IsFalling();
	bIsMovingOnGround = MovementComponent->IsMovingOnGround();
	bIsCrouching = Character->bIsCrouched;
	bIsAnyMontagePlaying = RpgAnimInstance->IsAnyMontagePlaying();
	Stance = bIsCrouching ? ERpgLocomotionStance::Crouching : ERpgLocomotionStance::Standing;

	switch (MovementComponent->MovementMode)
	{
	case MOVE_Walking:
	case MOVE_NavWalking:
		MovementState = ERpgLocomotionMovementState::Grounded;
		break;
	case MOVE_Falling:
		MovementState = ERpgLocomotionMovementState::Airborne;
		break;
	case MOVE_Swimming:
		MovementState = ERpgLocomotionMovementState::Swimming;
		break;
	case MOVE_Flying:
		MovementState = ERpgLocomotionMovementState::Flying;
		break;
	case MOVE_Custom:
		MovementState = ERpgLocomotionMovementState::Custom;
		break;
	default:
		MovementState = ERpgLocomotionMovementState::None;
		break;
	}

	const float MaxGroundSpeed = FMath::Max(MovementComponent->GetMaxSpeed(), 1.0f);
	const float SpeedRatio = GroundSpeed / MaxGroundSpeed;
	if (!bIsMovingOnGround || GroundSpeed < 3.0f)
	{
		Gait = ERpgLocomotionGait::Idle;
	}
	else if (SpeedRatio < 0.4f)
	{
		Gait = ERpgLocomotionGait::Walk;
	}
	else if (SpeedRatio < 0.85f)
	{
		Gait = ERpgLocomotionGait::Run;
	}
	else
	{
		Gait = ERpgLocomotionGait::Sprint;
	}

	ProceduralLocomotionAlpha =
		bIsMovingOnGround && !bIsCrouching && !bIsAnyMontagePlaying ? 1.0f : 0.0f;

	const FRotator AimDelta = (Character->GetBaseAimRotation() - Character->GetActorRotation()).GetNormalized();
	AimYaw = AimDelta.Yaw;
	AimPitch = AimDelta.Pitch;
	LocomotionAngle = bHasVelocity
		? FMath::RadiansToDegrees(FMath::Atan2(LocalVelocity.Y, LocalVelocity.X))
		: 0.0f;

	if (RpgAnimInstance->ShouldGeneratePoseSearchTrajectory())
	{
		FTransformTrajectory GeneratedTrajectory;
		UPoseSearchTrajectoryLibrary::PoseSearchGenerateTransformTrajectory(
			RpgAnimInstance,
			TrajectoryGenerationData,
			DeltaSeconds,
			TransformTrajectory,
			DesiredControllerYawLastUpdate,
			GeneratedTrajectory,
			0.04f,
			10,
			0.2f,
			8);
		TransformTrajectory = MoveTemp(GeneratedTrajectory);
	}
	else
	{
		TransformTrajectory.Samples.Reset();
		DesiredControllerYawLastUpdate = 0.0f;
	}
}

void URpgAnimInstance::InitializeWithAbilitySystem(UAbilitySystemComponent* ASC)
{
	check(ASC);

	GameplayTagPropertyMap.Initialize(this, ASC);
}

#if WITH_EDITOR
EDataValidationResult URpgAnimInstance::IsDataValid(FDataValidationContext& Context) const
{
	Super::IsDataValid(Context);

	GameplayTagPropertyMap.IsDataValid(this, Context);
	if (bGeneratePoseSearchTrajectory)
	{
		if (GroundMotionMatchingDatabases.IsEmpty())
		{
			Context.AddError(FText::FromString(
				TEXT("Motion Matching is enabled, but no grounded Pose Search database is configured.")));
		}
		if (AirborneMotionMatchingDatabases.IsEmpty())
		{
			Context.AddError(FText::FromString(
				TEXT("Motion Matching is enabled, but no airborne Pose Search database is configured.")));
		}

		const auto ValidateDatabases = [&Context](
			const TArray<TObjectPtr<UPoseSearchDatabase>>& Databases,
			const TCHAR* GroupName)
		{
			for (int32 Index = 0; Index < Databases.Num(); ++Index)
			{
				if (!Databases[Index])
				{
					Context.AddError(FText::FromString(FString::Printf(
						TEXT("%s Pose Search database entry %d is null."),
						GroupName,
						Index)));
				}
			}
		};
		ValidateDatabases(GroundMotionMatchingDatabases, TEXT("Grounded"));
		ValidateDatabases(AirborneMotionMatchingDatabases, TEXT("Airborne"));
	}

	return ((Context.GetNumErrors() > 0) ? EDataValidationResult::Invalid : EDataValidationResult::Valid);
}
#endif // WITH_EDITOR

void URpgAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	if (AActor* OwningActor = GetOwningActor())
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwningActor))
		{
			InitializeWithAbilitySystem(ASC);
		}
	}
}

void URpgAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);

	const FRpgAnimInstanceProxy& Proxy = GetProxyOnAnyThread<FRpgAnimInstanceProxy>();
	WorldVelocity = Proxy.WorldVelocity;
	LocalVelocity = Proxy.LocalVelocity;
	WorldAcceleration = Proxy.WorldAcceleration;
	LocalAcceleration = Proxy.LocalAcceleration;
	LocomotionGroundSpeed = Proxy.GroundSpeed;
	VerticalVelocity = Proxy.VerticalVelocity;
	GroundDistance = Proxy.GroundDistance;
	AimYaw = Proxy.AimYaw;
	AimPitch = Proxy.AimPitch;
	LocomotionAngle = Proxy.LocomotionAngle;
	bHasVelocity = Proxy.bHasVelocity;
	bHasAcceleration = Proxy.bHasAcceleration;
	bLocomotionIsFalling = Proxy.bIsFalling;
	bIsMovingOnGround = Proxy.bIsMovingOnGround;
	bIsCrouching = Proxy.bIsCrouching;
	LocomotionGait = Proxy.Gait;
	LocomotionStance = Proxy.Stance;
	LocomotionMovementState = Proxy.MovementState;
	LocomotionTrajectory = Proxy.TransformTrajectory;
	ProceduralLocomotionAlpha = Proxy.ProceduralLocomotionAlpha;
	bIsAnyMontagePlaying = Proxy.bIsAnyMontagePlaying;
}

void URpgAnimInstance::UpdateGaspMotionMatching(
	const FAnimUpdateContext& Context,
	const FAnimNodeReference& Node)
{
	(void)Context;

	FAnimNode_MotionMatching* MotionMatchingNode = Node.GetAnimNodePtr<FAnimNode_MotionMatching>();
	if (!MotionMatchingNode)
	{
		return;
	}

	const TArray<TObjectPtr<UPoseSearchDatabase>>& SelectedDatabases =
		bLocomotionIsFalling ? AirborneMotionMatchingDatabases : GroundMotionMatchingDatabases;
	TArray<UPoseSearchDatabase*, TInlineAllocator<5>> DatabasesToSearch;
	DatabasesToSearch.Reserve(SelectedDatabases.Num());
	for (UPoseSearchDatabase* Database : SelectedDatabases)
	{
		if (Database)
		{
			DatabasesToSearch.Add(Database);
		}
	}

	MotionMatchingNode->SetDatabasesToSearch(
		MakeArrayView(DatabasesToSearch),
		EPoseSearchInterruptMode::InterruptOnDatabaseChange);
}

FAnimInstanceProxy* URpgAnimInstance::CreateAnimInstanceProxy()
{
	return new FRpgAnimInstanceProxy(this);
}

void URpgAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	delete static_cast<FRpgAnimInstanceProxy*>(InProxy);
}

