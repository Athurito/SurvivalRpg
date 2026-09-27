// Copyright Epic Games, Inc. All Rights Reserved.

#include "RpgAnimInstance.h"
#include "AbilitySystemGlobals.h"
#include "Animation/AnimClassInterface.h"
#include "Animation/AnimationAsset.h"
#include "ChooserFunctionLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#include "UObject/Script.h"
#include "UObject/StrongObjectPtr.h"
#endif

#include "SurvivalRpg/Core/Character/RpgCharacter.h"
#include "SurvivalRpg/Core/Character/RpgCharacterMovementComponent.h"
#include "SurvivalRpg/Core/Character/RpgCharacterMoverComponent.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgAnimInstance)


URpgAnimInstance::URpgAnimInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UAnimationAsset* URpgAnimInstance::EvaluateAnimationChooser(UChooserTable* Chooser)
{
	check(IsInGameThread());
	// The stock Blueprint node binds a constant table. Expose the engine's context-object
	// evaluator so inherited data-only animation sets can supply their own table.
	return Chooser ? Cast<UAnimationAsset>(UChooserFunctionLibrary::EvaluateChooser(this, Chooser, UAnimationAsset::StaticClass())) : nullptr;
}

bool URpgAnimInstance::IsAnimationCompatible(const UAnimationAsset* Animation) const
{
	const IAnimClassInterface* AnimClass = IAnimClassInterface::GetFromClass(GetClass());
	const USkeleton* TargetSkeleton = AnimClass ? AnimClass->GetTargetSkeleton() : nullptr;
	return Animation && TargetSkeleton && Animation->GetSkeleton() == TargetSkeleton;
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
	const bool bIsRemoteAutonomousPoseTick =
		World && World->GetNetMode() == NM_ListenServer &&
		Character && Character->GetLocalRole() == ROLE_Authority &&
		Character->GetRemoteRole() == ROLE_AutonomousProxy &&
		MeshComponent && MeshComponent->bOnlyAllowAutonomousTickPose &&
		MeshComponent->bIsAutonomousTickPose;

	// Several client moves can tick the pose within one server frame. A deferred graph
	// update retains only the last move's delta; consume each move before it is overwritten.
	return !bIsRemoteAutonomousPoseTick;
}

void URpgAnimInstance::InitializeWithAbilitySystem(UAbilitySystemComponent* ASC)
{
	check(ASC);

	GameplayTagPropertyMap.Initialize(this, ASC);
}

#if WITH_EDITOR
EDataValidationResult URpgAnimInstance::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);
	Result = CombineDataValidationResults(Result, GameplayTagPropertyMap.IsDataValid(this, Context));
	if (GetClass()->IsFunctionImplementedInScript(GET_FUNCTION_NAME_CHECKED(URpgAnimInstance, ValidateAnimationSet))
		&& !GetClass()->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
	{
		// UAnimInstance has Within=SkeletalMeshComponent. The unregistered outer provides that ownership
		// without a world, animation initialization, mesh physics, or modification of the authored defaults.
		TStrongObjectPtr<USkeletalMeshComponent> ValidationMesh(NewObject<USkeletalMeshComponent>(GetTransientPackage(), NAME_None, RF_Transient));
		TStrongObjectPtr<URpgAnimInstance> ValidationInstance(NewObject<URpgAnimInstance>(ValidationMesh.Get(), GetClass(), NAME_None, RF_Transient, const_cast<URpgAnimInstance*>(this)));
		TArray<FText> Errors;
		{
			FEditorScriptExecutionGuard ScriptGuard;
			ValidationInstance->ValidateAnimationSet(Errors);
		}
		for (const FText& Error : Errors)
		{
			Context.AddError(FText::Format(NSLOCTEXT("RpgAnimInstance", "AnimationSetValidationError", "Animation set '{0}': {1}"),
				FText::FromString(GetPathName()), Error));
		}
	}
	return Context.GetNumErrors() > 0 ? EDataValidationResult::Invalid : Result;
}
#endif // WITH_EDITOR

void URpgAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	CaptureBlockAnimationState();

	if (AActor* OwningActor = GetOwningActor())
	{
		if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwningActor))
		{
			InitializeWithAbilitySystem(ASC);
		}
	}
}

void URpgAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	CaptureBlockAnimationState();

	const ARpgCharacter* Character = Cast<ARpgCharacter>(GetOwningActor());
	if (!Character)
	{
		return;
	}

	URpgCharacterMovementComponent* CharMoveComp = CastChecked<URpgCharacterMovementComponent>(Character->GetCharacterMovement());
	const FRpgCharacterGroundInfo& GroundInfo = CharMoveComp->GetGroundInfo();
	GroundDistance = GroundInfo.GroundDistance;
}

void URpgAnimInstance::CaptureBlockAnimationState()
{

	// Linked animation instances share the gameplay mesh, but capture their own immutable
	// worker inputs here. Never query equipment, GAS or movement from a thread-safe graph.
	bBlockLocomotionActive = false;
	BlockLocalVelocity = FVector::ZeroVector;
	BlockFacingYaw = 0.f;
	bBlockHasMovementInput = false;
	bBlockGrounded = false;
	bBlockInterrupted = true;
	BlockMovementSpeedLimit = 0.f;
	if (const APawn* Pawn = TryGetPawnOwner())
	{
		const USkeletalMeshComponent* GameplayMesh = GetSkelMeshComponent();
		// RotateRootBone offsets are evaluated inside this displayed component. Compensating the fixed-step
		// actor yaw instead makes that offset fight Mover's smoothed mesh yaw between simulation frames.
		BlockFacingYaw = GameplayMesh ? GameplayMesh->GetComponentRotation().Yaw : Pawn->GetActorRotation().Yaw;
		// Directional clip selection retains the pawn's movement frame, independent of mesh basis and smoothing.
		const FRotator Heading(0.f, Pawn->GetActorRotation().Yaw, 0.f);
		const URpgAbilitySystemComponent* ASC = Cast<URpgAbilitySystemComponent>(
			UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(const_cast<APawn*>(Pawn)));
		if (const URpgCharacterMoverComponent* Mover = Pawn->FindComponentByClass<URpgCharacterMoverComponent>())
		{
			bBlockLocomotionActive = Mover->IsBlockMovementActive();
			BlockMovementSpeedLimit = Mover->GetBlockMovementSpeedLimit();
			BlockLocalVelocity = Heading.UnrotateVector(Mover->GetVelocity());
			bBlockHasMovementInput = !Mover->GetMovementIntent().IsNearlyZero();
			bBlockGrounded = Mover->IsOnGround();
		}
		else if (const ARpgCharacter* BlockCharacter = Cast<ARpgCharacter>(Pawn))
		{
			if (const URpgCharacterMovementComponent* Movement = Cast<URpgCharacterMovementComponent>(BlockCharacter->GetCharacterMovement()))
			{
				bBlockLocomotionActive = Movement->GetBlockMovementForMove(BlockMovementSpeedLimit);
				BlockLocalVelocity = Heading.UnrotateVector(Movement->Velocity);
				bBlockHasMovementInput = !Movement->GetCurrentAcceleration().IsNearlyZero();
				bBlockGrounded = Movement->IsMovingOnGround();
			}
		}
		BlockLocalVelocity.Z = 0.f;
		bBlockInterrupted = !bBlockGrounded || !ASC ||
			ASC->HasMatchingGameplayTag(RpgGameplayTags::Status_Death) ||
			ASC->HasMatchingGameplayTag(RpgGameplayTags::State_Dead) ||
			ASC->HasMatchingGameplayTag(RpgGameplayTags::Status_Ragdoll);
		bBlockLocomotionActive &= !bBlockInterrupted;
	}

}

