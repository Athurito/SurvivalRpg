#include "RpgRuntimeRetargetComponent.h"

#include "RpgRuntimeRetargetProfile.h"
#include "Animation/AnimClassInterface.h"
#include "Animation/AnimInstance.h"
#include "AnimNodes/AnimNode_RetargetPoseFromMesh.h"
#include "Components/GameFrameworkComponentManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "SurvivalRpg/Core/Character/RpgPawnData.h"
#include "SurvivalRpg/Core/Character/RpgPawnExtensionComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/SurvivalRpg.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgRuntimeRetargetComponent)

URpgRuntimeRetargetComponent::URpgRuntimeRetargetComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void URpgRuntimeRetargetComponent::OnRegister()
{
	Super::OnRegister();
	if (HasBegunPlay()) InitializeFromPawnData();
}

void URpgRuntimeRetargetComponent::BeginPlay()
{
	Super::BeginPlay();
	InitializeFromPawnData();
}

void URpgRuntimeRetargetComponent::InitializeFromPawnData()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character || !GetWorld() || !GetWorld()->IsGameWorld()
		|| Character->FindComponentByClass<URpgRuntimeRetargetComponent>() != this) return;
	if (!PawnDataDelegate.IsValid())
	{
		if (UGameInstance* GameInstance = GetWorld()->GetGameInstance())
		{
			if (UGameFrameworkComponentManager* Manager = GameInstance->GetSubsystem<UGameFrameworkComponentManager>())
			{
				ComponentManager = Manager;
				PawnDataDelegate = Manager->RegisterAndCallForActorInitState(Character,
					URpgPawnExtensionComponent::Name_ActorFeatureName, RpgGameplayTags::InitState_DataAvailable,
					FActorInitStateChangedDelegate::CreateUObject(this, &ThisClass::HandlePawnDataAvailable), false);
			}
		}
	}
	RefreshFromPawnData();
}

void URpgRuntimeRetargetComponent::HandlePawnDataAvailable(const FActorInitStateChangedParams& Params)
{
	RefreshFromPawnData();
}

void URpgRuntimeRetargetComponent::RefreshFromPawnData()
{
	const URpgPawnExtensionComponent* PawnExtension = URpgPawnExtensionComponent::FindPawnExtensionComponent(GetOwner());
	const URpgPawnData* PawnData = PawnExtension ? PawnExtension->GetPawnData<URpgPawnData>() : nullptr;
	if (!PawnData) return;
	const URpgRuntimeRetargetProfile* Profile = PawnData->RuntimeRetargetProfile;
	if (bProfileApplied && ActiveProfile == Profile && (!Profile || !Profile->TargetMesh || IsValid(RetargetMesh))) return;
	ApplyProfile(Profile);
}

UIKRetargeter* URpgRuntimeRetargetComponent::GetRetargeter() const
{
	return ActiveProfile ? ActiveProfile->Retargeter.Get() : nullptr;
}

bool URpgRuntimeRetargetComponent::ApplyProfile(const URpgRuntimeRetargetProfile* Profile)
{
	ClearPresentation();
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!IsRegistered() || !Character || !GetWorld() || !GetWorld()->IsGameWorld()
		|| Character->FindComponentByClass<URpgRuntimeRetargetComponent>() != this) return false;
	USkeletalMeshComponent* GameplayMesh = Character->GetMesh();
	if (!Profile || !Profile->TargetMesh)
	{
		ActiveProfile = Profile;
		bProfileApplied = true;
		return true;
	}
	FText Error;
	if (!GameplayMesh || !GameplayMesh->IsRegistered() || !GameplayMesh->GetSkeletalMeshAsset()
		|| Character->FindComponentByClass<USkeletalMeshComponent>() != GameplayMesh
		|| !Profile->ValidateConfiguration(GameplayMesh->GetSkeletalMeshAsset(), Error))
	{
		UE_LOG(LogRpgCharacter, Warning, TEXT("Runtime retarget profile [%s] rejected for [%s]: %s; keeping the gameplay mesh."),
			*GetPathNameSafe(Profile), *GetNameSafe(Character), Error.IsEmpty() ? TEXT("Gameplay mesh is unavailable") : *Error.ToString());
		return false;
	}
	ActiveProfile = Profile;
	bProfileApplied = true;
	// Dedicated servers retain the complete gameplay pose and sockets without allocating a cosmetic follower.
	if (GetNetMode() == NM_DedicatedServer) return true;
	SourceMesh = GameplayMesh;
	PreviousSourceTickOption = GameplayMesh->VisibilityBasedAnimTickOption;
	bPreviousSourceURO = GameplayMesh->bEnableUpdateRateOptimizations;
	bPreviousSourceVisible = GameplayMesh->GetVisibleFlag();
	GameplayMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	GameplayMesh->bEnableUpdateRateOptimizations = false;
	RetargetMesh = NewObject<USkeletalMeshComponent>(Character, NAME_None, RF_Transient);
	RetargetMesh->SetupAttachment(GameplayMesh);
	RetargetMesh->SetRelativeTransform(Profile->RelativeTransform);
	RetargetMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RetargetMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	RetargetMesh->SetGenerateOverlapEvents(false);
	RetargetMesh->SetCanEverAffectNavigation(false);
	RetargetMesh->SetIsReplicated(false);
	RetargetMesh->bEnablePhysicsOnDedicatedServer = false;
	RetargetMesh->bEnableUpdateRateOptimizations = false;
	RetargetMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	RetargetMesh->PrimaryComponentTick.TickGroup = TG_PostPhysics;
	RetargetMesh->PrimaryComponentTick.EndTickGroup = TG_PostPhysics;
	RetargetMesh->AddTickPrerequisiteComponent(GameplayMesh);
	RetargetMesh->SetVisibility(false, false);
	RetargetMesh->SetHiddenInGame(GameplayMesh->bHiddenInGame, false);
	RetargetMesh->SetSkeletalMesh(Profile->TargetMesh);
	RetargetMesh->SetAnimInstanceClass(Profile->RetargetAnimClass);
	// ActiveProfile and the component getter must be valid before RegisterComponent creates the presentation AnimInstance.
	RetargetMesh->RegisterComponent();
	// GAS discovers the avatar mesh through this engine component lookup. Adding presentation must not change its result.
	if (!RetargetMesh->IsRegistered() || !RetargetMesh->GetAnimInstance()
		|| Character->FindComponentByClass<USkeletalMeshComponent>() != GameplayMesh)
	{
		UE_LOG(LogRpgCharacter, Warning, TEXT("Runtime retarget AnimBP failed to initialize for [%s]; keeping the gameplay mesh."), *GetNameSafe(Character));
		ClearPresentation();
		return false;
	}
	TargetPoseDelegate = RetargetMesh->RegisterOnBoneTransformsFinalizedDelegate(
		FOnBoneTransformsFinalizedMultiCast::FDelegate::CreateUObject(this, &ThisClass::HandleTargetPoseFinalized));
	return true;
}

void URpgRuntimeRetargetComponent::HandleTargetPoseFinalized()
{
	UAnimInstance* Animation = IsValid(RetargetMesh) ? RetargetMesh->GetAnimInstance() : nullptr;
	const IAnimClassInterface* AnimClass = Animation ? IAnimClassInterface::GetFromClass(Animation->GetClass()) : nullptr;
	bool bRetargetReady = false;
	if (SourceMesh.IsValid() && SourceMesh->IsRegistered() && SourceMesh->GetSkeletalMeshAsset()
		&& ActiveProfile && AnimClass && RetargetMesh->GetAttachParent() == SourceMesh.Get()
		&& RetargetMesh->GetSkeletalMeshAsset() == ActiveProfile->TargetMesh)
	{
		for (const FStructProperty* Property : AnimClass->GetAnimNodeProperties())
		{
			if (!Property || Property->Struct != FAnimNode_RetargetPoseFromMesh::StaticStruct()) continue;
			FAnimNode_RetargetPoseFromMesh* Node = Property->ContainerPtrToValuePtr<FAnimNode_RetargetPoseFromMesh>(Animation);
			// Read only after this mesh's evaluation has completed. Never initialize or evaluate an AnimNode here.
			FIKRetargetProcessor* Processor = Node->GetRetargetProcessor();
			if (Node->RetargetFrom != ERetargetSourceMode::SourcePosePin && Node->SourceMeshComponent == SourceMesh
				&& Node->IKRetargeterAsset == ActiveProfile->Retargeter && Node->LODThreshold == INDEX_NONE
				&& Processor && Processor->IsInitialized()
				&& Processor->WasInitializedWithTheseAssets(SourceMesh->GetSkeletalMeshAsset(), ActiveProfile->TargetMesh, ActiveProfile->Retargeter)
				&& Processor->GetSkeleton(ERetargetSourceOrTarget::Source).BoneNames.Num() == SourceMesh->GetComponentSpaceTransforms().Num())
			{
				bRetargetReady = true;
				break;
			}
		}
	}
	if (!bRetargetReady)
	{
		RejectPendingPresentation();
		return;
	}
	// The first successful Evaluate initializes the processor after PreUpdate. Require another completed frame with
	// the same live source/processor/assets so its next PreUpdate has copied the gameplay pose before we hide it.
	if (FirstPoseFrame == MAX_uint64)
	{
		FirstPoseFrame = GFrameCounter;
		return;
	}
	if (GFrameCounter == FirstPoseFrame) return;
	RetargetMesh->UnregisterOnBoneTransformsFinalizedDelegate(TargetPoseDelegate);
	TargetPoseDelegate.Reset();
	SourceMesh->SetVisibility(false, false);
	bSourceVisibilityOverridden = true;
	RetargetMesh->SetVisibility(bPreviousSourceVisible, false);
}

void URpgRuntimeRetargetComponent::RejectPendingPresentation()
{
	UE_LOG(LogRpgCharacter, Warning, TEXT("Runtime retarget node did not initialize from the configured gameplay mesh and retargeter for [%s]; keeping the gameplay mesh."),
		*GetNameSafe(GetOwner()));
	if (IsValid(RetargetMesh))
	{
		RetargetMesh->UnregisterOnBoneTransformsFinalizedDelegate(TargetPoseDelegate);
		RetargetMesh->SetVisibility(false, false);
		RetargetMesh->SetComponentTickEnabled(false);
	}
	TargetPoseDelegate.Reset();
	// Do not destroy the skeletal component from inside its own finalization callback.
	if (UWorld* World = GetWorld())
	{
		DeferredCleanupHandle = World->GetTimerManager().SetTimerForNextTick(this, &ThisClass::ClearPresentation);
	}
}

void URpgRuntimeRetargetComponent::ClearPresentation()
{
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(DeferredCleanupHandle);
	DeferredCleanupHandle.Invalidate();
	if (IsValid(RetargetMesh))
	{
		RetargetMesh->UnregisterOnBoneTransformsFinalizedDelegate(TargetPoseDelegate);
		if (SourceMesh.IsValid()) RetargetMesh->RemoveTickPrerequisiteComponent(SourceMesh.Get());
		RetargetMesh->DestroyComponent();
	}
	RetargetMesh = nullptr;
	TargetPoseDelegate.Reset();
	if (SourceMesh.IsValid())
	{
		SourceMesh->VisibilityBasedAnimTickOption = PreviousSourceTickOption;
		SourceMesh->bEnableUpdateRateOptimizations = bPreviousSourceURO;
		if (bSourceVisibilityOverridden) SourceMesh->SetVisibility(bPreviousSourceVisible, false);
	}
	SourceMesh.Reset();
	ActiveProfile = nullptr;
	bProfileApplied = false;
	bSourceVisibilityOverridden = false;
	FirstPoseFrame = MAX_uint64;
}

void URpgRuntimeRetargetComponent::StopListening()
{
	if (ComponentManager.IsValid()) ComponentManager->UnregisterActorInitStateDelegate(GetOwner(), PawnDataDelegate);
	PawnDataDelegate.Reset();
	ComponentManager.Reset();
}

void URpgRuntimeRetargetComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopListening();
	ClearPresentation();
	Super::EndPlay(EndPlayReason);
}

void URpgRuntimeRetargetComponent::OnUnregister()
{
	StopListening();
	ClearPresentation();
	Super::OnUnregister();
}
