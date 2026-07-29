#include "RpgCorpseLifecycleComponent.h"

#include "RpgCorpseProfile.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "SurvivalRpg/Physics/RpgCollisionChannels.h"
#include "TimerManager.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgCorpseLifecycleComponent)

namespace RpgCorpseDefaults
{
	static const FName RagdollBoneName(TEXT("pelvis"));
	static const FName AnchorBoneName(TEXT("pelvis"));
	static const FName RagdollCollisionProfileName(TEXT("Ragdoll"));
	static constexpr float VelocityMultiplier = 1.0f;
	static constexpr float MaximumRagdollSpeed = 1200.0f;
	static constexpr float SettleDelaySeconds = 1.0f;
	static constexpr float InteractionRadius = 350.0f;
	static constexpr float EmptyDespawnDelaySeconds = 2.0f;
	static constexpr float MaximumLifetimeSeconds = 120.0f;
	static const FGameplayTagContainer NoExternalRequirements;
}

URpgCorpseLifecycleComponent::URpgCorpseLifecycleComponent(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicatedByDefault(true);

	// Constructor-safe initialization: SetSphereRadius may create a transient BodySetup while
	// this component is still an unnamed CDO/default subobject during module bootstrap.
	InitSphereRadius(RpgCorpseDefaults::InteractionRadius);
	SetCollisionObjectType(ECC_WorldDynamic);
	SetCollisionResponseToAllChannels(ECR_Ignore);
	SetCollisionResponseToChannel(Rpg_TraceChannel_Interaction, ECR_Block);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	SetHiddenInGame(true);
}

void URpgCorpseLifecycleComponent::BeginPlay()
{
	Super::BeginPlay();

	CachedSkeletalMesh = ResolveSkeletalMesh();
	AttachAnchorToConfiguredBone();
	SetSphereRadius(GetCorpseInteractionRadius(), false);
	bLastBroadcastAvailability = IsCorpseAvailable();
	ApplyReplicatedPresentation();
}

void URpgCorpseLifecycleComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		TimerManager.ClearTimer(SettleTimerHandle);
		TimerManager.ClearTimer(MaximumLifetimeTimerHandle);
		TimerManager.ClearTimer(CompletedDespawnTimerHandle);
		TimerManager.ClearTimer(ExpirationDestroyTimerHandle);
		TimerManager.ClearTimer(PresentationSettleTimerHandle);
	}

	CorpseAvailabilityChangedNative.Clear();
	Super::EndPlay(EndPlayReason);
}

void URpgCorpseLifecycleComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, LifecycleState);
	DOREPLIFETIME(ThisClass, RagdollState);
	DOREPLIFETIME(ThisClass, ExpirationServerTimeSeconds);
}

void URpgCorpseLifecycleComponent::NotifyDeathStarted(
	const FVector& AuthoritativeVelocity)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority()
		|| LifecycleState != ERpgCorpseLifecycleState::Inactive)
	{
		return;
	}

	CapturedAuthoritativeVelocity = AuthoritativeVelocity.ContainsNaN()
		? FVector::ZeroVector
		: AuthoritativeVelocity;
	SetLifecycleState(ERpgCorpseLifecycleState::Dying);
}

void URpgCorpseLifecycleComponent::NotifyDeathFinished()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority()
		|| LifecycleState >= ERpgCorpseLifecycleState::Settling)
	{
		return;
	}

	if (LifecycleState == ERpgCorpseLifecycleState::Inactive)
	{
		NotifyDeathStarted(OwnerActor->GetVelocity());
	}

	RagdollState.Revision = RagdollState.Revision == MAX_int32
		? 1
		: FMath::Max(1, RagdollState.Revision + 1);
	RagdollState.LinearVelocity = CalculateRagdollStartVelocity(
		CapturedAuthoritativeVelocity,
		ResolveVelocityMultiplier(),
		ResolveMaximumRagdollSpeed());

	ExpirationServerTimeSeconds =
		GetSynchronizedServerTimeSeconds() + ResolveMaximumLifetimeSeconds();
	SetLifecycleState(ERpgCorpseLifecycleState::Settling);
	ApplyReplicatedPresentation();

	if (UWorld* World = GetWorld())
	{
		const float SettleDelay = ResolveSettleDelaySeconds();
		if (SettleDelay <= KINDA_SMALL_NUMBER)
		{
			HandleSettleElapsed();
		}
		else
		{
			World->GetTimerManager().SetTimer(
				SettleTimerHandle,
				this,
				&ThisClass::HandleSettleElapsed,
				SettleDelay,
				false);
		}

		World->GetTimerManager().SetTimer(
			MaximumLifetimeTimerHandle,
			this,
			&ThisClass::HandleHardLifetimeElapsed,
			ResolveMaximumLifetimeSeconds(),
			false);
	}

	OwnerActor->ForceNetUpdate();
}

void URpgCorpseLifecycleComponent::SetInventoryRequirementComplete(
	const bool bIsComplete)
{
	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority()
		|| LifecycleState >= ERpgCorpseLifecycleState::Completed)
	{
		return;
	}

	if (bInventoryRequirementComplete != bIsComplete)
	{
		bInventoryRequirementComplete = bIsComplete;
		EvaluateCompletionRequirements();
	}
}

bool URpgCorpseLifecycleComponent::CompleteExternalRequirement(
	const FGameplayTag RequirementTag)
{
	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() || !RequirementTag.IsValid()
		|| LifecycleState == ERpgCorpseLifecycleState::Inactive
		|| LifecycleState >= ERpgCorpseLifecycleState::Completed)
	{
		return false;
	}

	const FGameplayTagContainer& RequiredTags =
		ResolveRequiredExternalCompletionTags();
	if (!RequiredTags.HasTagExact(RequirementTag)
		|| CompletedExternalRequirements.HasTagExact(RequirementTag))
	{
		return false;
	}

	CompletedExternalRequirements.AddTag(RequirementTag);
	EvaluateCompletionRequirements();
	return true;
}

float URpgCorpseLifecycleComponent::GetCorpseInteractionRadius() const
{
	return CorpseProfile
		? FMath::Max(1.0f, CorpseProfile->InteractionRadius)
		: RpgCorpseDefaults::InteractionRadius;
}

float URpgCorpseLifecycleComponent::GetRemainingLifetimeSeconds() const
{
	if (ExpirationServerTimeSeconds <= 0.0f
		|| LifecycleState == ERpgCorpseLifecycleState::Inactive)
	{
		return 0.0f;
	}

	return FMath::Max(
		0.0f,
		ExpirationServerTimeSeconds - GetSynchronizedServerTimeSeconds());
}

FVector URpgCorpseLifecycleComponent::CalculateRagdollStartVelocity(
	const FVector& AuthoritativeVelocity,
	const float VelocityMultiplier,
	const float MaximumSpeed)
{
	if (AuthoritativeVelocity.ContainsNaN()
		|| !FMath::IsFinite(VelocityMultiplier)
		|| !FMath::IsFinite(MaximumSpeed))
	{
		return FVector::ZeroVector;
	}

	const float SafeMultiplier = FMath::Max(0.0f, VelocityMultiplier);
	const float SafeMaximumSpeed = FMath::Max(0.0f, MaximumSpeed);
	return (AuthoritativeVelocity * SafeMultiplier).GetClampedToMaxSize(SafeMaximumSpeed);
}

void URpgCorpseLifecycleComponent::OnRep_LifecycleState(
	const ERpgCorpseLifecycleState PreviousState)
{
	HandleLifecycleStateChanged(PreviousState);
}

void URpgCorpseLifecycleComponent::OnRep_RagdollState()
{
	ApplyReplicatedPresentation();
}

void URpgCorpseLifecycleComponent::AttachAnchorToConfiguredBone()
{
	USkeletalMeshComponent* Mesh = ResolveSkeletalMesh();
	if (!Mesh)
	{
		return;
	}

	const FName DesiredBone = ResolveAnchorBoneName();
	const FName AttachBone = Mesh->DoesSocketExist(DesiredBone)
		? DesiredBone
		: NAME_None;
	AttachToComponent(
		Mesh,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		AttachBone);
}

USkeletalMeshComponent* URpgCorpseLifecycleComponent::ResolveSkeletalMesh() const
{
	if (CachedSkeletalMesh)
	{
		return CachedSkeletalMesh;
	}

	if (const ACharacter* CharacterOwner = Cast<ACharacter>(GetOwner()))
	{
		return CharacterOwner->GetMesh();
	}

	return GetOwner()
		? GetOwner()->FindComponentByClass<USkeletalMeshComponent>()
		: nullptr;
}

void URpgCorpseLifecycleComponent::ApplyReplicatedPresentation()
{
	ApplyRagdollIfNeeded();

	if (LifecycleState >= ERpgCorpseLifecycleState::Available)
	{
		SleepRagdollWhenReady();
	}

	SetCollisionEnabled(IsCorpseAvailable()
		? ECollisionEnabled::QueryOnly
		: ECollisionEnabled::NoCollision);
}

bool URpgCorpseLifecycleComponent::ApplyRagdollIfNeeded()
{
	if (RagdollState.Revision == 0
		|| RagdollState.Revision == LastAppliedRagdollRevision)
	{
		return false;
	}

	USkeletalMeshComponent* Mesh = ResolveSkeletalMesh();
	if (!Mesh)
	{
		return false;
	}

	CachedSkeletalMesh = Mesh;
	Mesh->SetCollisionProfileName(ResolveRagdollCollisionProfileName());
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetAllBodiesBelowSimulatePhysics(ResolveRagdollBoneName(), true, true);
	Mesh->SetAllBodiesBelowPhysicsBlendWeight(ResolveRagdollBoneName(), 1.0f, false, true);
	Mesh->SetAllPhysicsLinearVelocity(FVector(RagdollState.LinearVelocity), false);
	LastAppliedRagdollRevision = RagdollState.Revision;
	LocalRagdollAppliedWorldTimeSeconds = GetWorld()
		? GetWorld()->GetTimeSeconds()
		: 0.0f;
	return true;
}

void URpgCorpseLifecycleComponent::SleepRagdollWhenReady()
{
	USkeletalMeshComponent* Mesh = ResolveSkeletalMesh();
	UWorld* World = GetWorld();
	if (!Mesh || !World || LastAppliedRagdollRevision == 0)
	{
		return;
	}

	// A late joiner receives an already-Available state together with the ragdoll revision. Give
	// its local bodies the same configured settle window before sleeping so the corpse does not
	// freeze upright in the animation reference pose. Exact bone parity remains intentionally local.
	const float ElapsedSinceLocalApply = FMath::Max(
		0.0f,
		World->GetTimeSeconds() - LocalRagdollAppliedWorldTimeSeconds);
	const float RemainingSettleTime = ResolveSettleDelaySeconds() - ElapsedSinceLocalApply;
	if (RemainingSettleTime > KINDA_SMALL_NUMBER)
	{
		World->GetTimerManager().SetTimer(
			PresentationSettleTimerHandle,
			this,
			&ThisClass::SleepRagdollWhenReady,
			RemainingSettleTime,
			false);
		return;
	}

	World->GetTimerManager().ClearTimer(PresentationSettleTimerHandle);
	Mesh->PutAllRigidBodiesToSleep();
}

void URpgCorpseLifecycleComponent::SetLifecycleState(
	const ERpgCorpseLifecycleState NewState)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() || LifecycleState == NewState)
	{
		return;
	}

	const ERpgCorpseLifecycleState PreviousState = LifecycleState;
	LifecycleState = NewState;
	HandleLifecycleStateChanged(PreviousState);
	OwnerActor->ForceNetUpdate();
}

void URpgCorpseLifecycleComponent::HandleLifecycleStateChanged(
	const ERpgCorpseLifecycleState PreviousState)
{
	ApplyReplicatedPresentation();

	const bool bIsAvailable = IsCorpseAvailable();
	const bool bWasAvailable =
		PreviousState == ERpgCorpseLifecycleState::Available;
	if (bIsAvailable != bWasAvailable
		|| bIsAvailable != bLastBroadcastAvailability)
	{
		bLastBroadcastAvailability = bIsAvailable;
		CorpseAvailabilityChangedNative.Broadcast(this, bIsAvailable);
		OnCorpseAvailabilityChanged.Broadcast(this, bIsAvailable);
	}
}

void URpgCorpseLifecycleComponent::HandleSettleElapsed()
{
	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority()
		|| LifecycleState != ERpgCorpseLifecycleState::Settling)
	{
		return;
	}

	if (HasCompletionRequirements() && AreCompletionRequirementsSatisfied())
	{
		BeginCompletedState();
		return;
	}

	SetLifecycleState(ERpgCorpseLifecycleState::Available);
}

void URpgCorpseLifecycleComponent::HandleHardLifetimeElapsed()
{
	BeginExpiration();
}

void URpgCorpseLifecycleComponent::EvaluateCompletionRequirements()
{
	if (LifecycleState == ERpgCorpseLifecycleState::Available
		&& HasCompletionRequirements()
		&& AreCompletionRequirementsSatisfied())
	{
		BeginCompletedState();
	}
}

bool URpgCorpseLifecycleComponent::HasCompletionRequirements() const
{
	return RequiresInventoryEmpty()
		|| !ResolveRequiredExternalCompletionTags().IsEmpty();
}

bool URpgCorpseLifecycleComponent::AreCompletionRequirementsSatisfied() const
{
	if (RequiresInventoryEmpty() && !bInventoryRequirementComplete)
	{
		return false;
	}

	return CompletedExternalRequirements.HasAllExact(
		ResolveRequiredExternalCompletionTags());
}

void URpgCorpseLifecycleComponent::BeginCompletedState()
{
	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority()
		|| LifecycleState >= ERpgCorpseLifecycleState::Completed)
	{
		return;
	}

	SetLifecycleState(ERpgCorpseLifecycleState::Completed);
	if (UWorld* World = GetWorld())
	{
		const float Delay = ResolveEmptyDespawnDelaySeconds();
		if (Delay <= KINDA_SMALL_NUMBER)
		{
			BeginExpiration();
		}
		else
		{
			World->GetTimerManager().SetTimer(
				CompletedDespawnTimerHandle,
				this,
				&ThisClass::BeginExpiration,
				Delay,
				false);
		}
	}
}

void URpgCorpseLifecycleComponent::BeginExpiration()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority()
		|| LifecycleState == ERpgCorpseLifecycleState::Expiring)
	{
		return;
	}

	SetLifecycleState(ERpgCorpseLifecycleState::Expiring);
	if (UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		TimerManager.ClearTimer(SettleTimerHandle);
		TimerManager.ClearTimer(MaximumLifetimeTimerHandle);
		TimerManager.ClearTimer(CompletedDespawnTimerHandle);
		TimerManager.ClearTimer(PresentationSettleTimerHandle);
		TimerManager.SetTimer(
			ExpirationDestroyTimerHandle,
			this,
			&ThisClass::DestroyOwnerAuthority,
			0.05f,
			false);
	}
}

void URpgCorpseLifecycleComponent::DestroyOwnerAuthority()
{
	if (AActor* OwnerActor = GetOwner(); OwnerActor && OwnerActor->HasAuthority())
	{
		OwnerActor->Destroy();
	}
}

float URpgCorpseLifecycleComponent::GetSynchronizedServerTimeSeconds() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0f;
	}

	if (const AGameStateBase* GameState = World->GetGameState())
	{
		return GameState->GetServerWorldTimeSeconds();
	}

	return World->GetTimeSeconds();
}

FName URpgCorpseLifecycleComponent::ResolveRagdollBoneName() const
{
	return CorpseProfile && !CorpseProfile->RagdollBoneName.IsNone()
		? CorpseProfile->RagdollBoneName
		: RpgCorpseDefaults::RagdollBoneName;
}

FName URpgCorpseLifecycleComponent::ResolveAnchorBoneName() const
{
	return CorpseProfile && !CorpseProfile->AnchorBoneName.IsNone()
		? CorpseProfile->AnchorBoneName
		: RpgCorpseDefaults::AnchorBoneName;
}

FName URpgCorpseLifecycleComponent::ResolveRagdollCollisionProfileName() const
{
	return CorpseProfile && !CorpseProfile->RagdollCollisionProfileName.IsNone()
		? CorpseProfile->RagdollCollisionProfileName
		: RpgCorpseDefaults::RagdollCollisionProfileName;
}

float URpgCorpseLifecycleComponent::ResolveVelocityMultiplier() const
{
	return CorpseProfile
		? FMath::Max(0.0f, CorpseProfile->RagdollVelocityMultiplier)
		: RpgCorpseDefaults::VelocityMultiplier;
}

float URpgCorpseLifecycleComponent::ResolveMaximumRagdollSpeed() const
{
	return CorpseProfile
		? FMath::Max(0.0f, CorpseProfile->MaximumRagdollSpeed)
		: RpgCorpseDefaults::MaximumRagdollSpeed;
}

float URpgCorpseLifecycleComponent::ResolveSettleDelaySeconds() const
{
	return CorpseProfile
		? FMath::Max(0.0f, CorpseProfile->SettleDelaySeconds)
		: RpgCorpseDefaults::SettleDelaySeconds;
}

float URpgCorpseLifecycleComponent::ResolveEmptyDespawnDelaySeconds() const
{
	return CorpseProfile
		? FMath::Max(0.0f, CorpseProfile->EmptyDespawnDelaySeconds)
		: RpgCorpseDefaults::EmptyDespawnDelaySeconds;
}

float URpgCorpseLifecycleComponent::ResolveMaximumLifetimeSeconds() const
{
	return CorpseProfile
		? FMath::Max(0.1f, CorpseProfile->MaximumLifetimeSeconds)
		: RpgCorpseDefaults::MaximumLifetimeSeconds;
}

bool URpgCorpseLifecycleComponent::RequiresInventoryEmpty() const
{
	return CorpseProfile ? CorpseProfile->bRequireInventoryEmpty : true;
}

const FGameplayTagContainer&
URpgCorpseLifecycleComponent::ResolveRequiredExternalCompletionTags() const
{
	return CorpseProfile
		? CorpseProfile->RequiredExternalCompletionTags
		: RpgCorpseDefaults::NoExternalRequirements;
}

#if WITH_EDITOR
EDataValidationResult URpgCorpseLifecycleComponent::IsDataValid(
	FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(
		Super::IsDataValid(Context),
		EDataValidationResult::Valid);
	if (CorpseProfile)
	{
		Result = CombineDataValidationResults(
			Result,
			CorpseProfile->IsDataValid(Context));
	}

	const USkeletalMeshComponent* Mesh = ResolveSkeletalMesh();
	if (Mesh && Mesh->GetSkeletalMeshAsset())
	{
		if (!Mesh->GetPhysicsAsset())
		{
			Context.AddError(NSLOCTEXT(
				"RpgCorpse",
				"MissingPhysicsAsset",
				"A ragdoll corpse skeletal mesh requires a PhysicsAsset."));
			Result = EDataValidationResult::Invalid;
		}

		if (Mesh->GetBoneIndex(ResolveRagdollBoneName()) == INDEX_NONE)
		{
			Context.AddError(FText::Format(
				NSLOCTEXT("RpgCorpse", "UnknownRagdollBone", "Ragdoll bone '{0}' does not exist on the owner skeletal mesh."),
				FText::FromName(ResolveRagdollBoneName())));
			Result = EDataValidationResult::Invalid;
		}

		if (Mesh->GetBoneIndex(ResolveAnchorBoneName()) == INDEX_NONE)
		{
			Context.AddError(FText::Format(
				NSLOCTEXT("RpgCorpse", "UnknownAnchorBone", "Corpse anchor bone '{0}' does not exist on the owner skeletal mesh."),
				FText::FromName(ResolveAnchorBoneName())));
			Result = EDataValidationResult::Invalid;
		}
	}

	return Result;
}
#endif
