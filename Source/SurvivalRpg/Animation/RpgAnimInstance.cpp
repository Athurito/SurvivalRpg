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

	const ERpgLocomotionGait PreviousGait = Gait;
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
	constexpr float MovingTrajectorySpeedThreshold = 5.0f;
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

	constexpr float IdleSpeedThreshold = 3.0f;
	const float MaxAcceleration = FMath::Max(MovementComponent->GetMaxAcceleration(), 1.0f);
	const float InputMagnitude = WorldAcceleration.Size2D() / MaxAcceleration;
	const bool bHasGroundedMoveIntent = bIsMovingOnGround && InputMagnitude > 0.1f;
	if (!bIsMovingOnGround || (GroundSpeed < IdleSpeedThreshold && !bHasGroundedMoveIntent))
	{
		Gait = ERpgLocomotionGait::Idle;
	}
	else if (bHasGroundedMoveIntent)
	{
		Gait = InputMagnitude < 0.65f
			? ERpgLocomotionGait::Walk
			: ERpgLocomotionGait::Run;
	}
	else
	{
		// Keep the last moving database through deceleration so a stop pose is not interrupted
		// just because input acceleration reached zero before capsule velocity did.
		Gait = PreviousGait == ERpgLocomotionGait::Walk
			? ERpgLocomotionGait::Walk
			: ERpgLocomotionGait::Run;
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
		// Match the controller-facing GASP trajectory profiles. During movement, current mesh facing is
		// already authoritative; extrapolating a mouse or replicated yaw delta selects false turn poses.
		TrajectoryGenerationData.RotateTowardsMovementSpeed = 0.0f;
		TrajectoryGenerationData.BendVelocityTowardsAcceleration = 0.0f;
		const bool bCanPredictIdleControllerYaw =
			Character->GetLocalRole() != ROLE_SimulatedProxy &&
			GroundSpeed < MovingTrajectorySpeedThreshold &&
			!bHasAcceleration;
		TrajectoryGenerationData.MaxControllerYawRate = bCanPredictIdleControllerYaw ? 100.0f : 0.0f;

		// Avoid interpreting the initial world yaw as a one-frame controller turn.
		if (TransformTrajectory.Samples.IsEmpty())
		{
			DesiredControllerYawLastUpdate = Character->GetViewRotation().Yaw;
		}

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
		else if (GroundMotionMatchingDatabases.Num() != 4)
		{
			Context.AddError(FText::FromString(
				TEXT("Ground Motion Matching databases must be ordered as exactly Idle, Walk, Run, and Sprint.")));
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

	TArray<UPoseSearchDatabase*, TInlineAllocator<5>> DatabasesToSearch;
	if (bLocomotionIsFalling)
	{
		DatabasesToSearch.Reserve(AirborneMotionMatchingDatabases.Num());
		for (UPoseSearchDatabase* Database : AirborneMotionMatchingDatabases)
		{
			if (Database)
			{
				DatabasesToSearch.Add(Database);
			}
		}
	}
	else
	{
		const int32 GaitDatabaseIndex = static_cast<int32>(LocomotionGait);
		if (GroundMotionMatchingDatabases.IsValidIndex(GaitDatabaseIndex))
		{
			if (UPoseSearchDatabase* Database = GroundMotionMatchingDatabases[GaitDatabaseIndex])
			{
				DatabasesToSearch.Add(Database);
			}
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

