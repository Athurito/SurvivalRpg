#include "RpgCombatDefenseProfile.h"

#include "TimerManager.h"
#include "GameFramework/Controller.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/AbilitySystem/Attributes/RpgDefenseSet.h"
#include "SurvivalRpg/Core/Character/RpgPawnExtensionComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"

URpgCombatDefenseProfileComponent::URpgCombatDefenseProfileComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

URpgCombatDefenseProfileComponent* URpgCombatDefenseProfileComponent::FindCombatDefenseProfileComponent(const AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<URpgCombatDefenseProfileComponent>() : nullptr;
}

void URpgCombatDefenseProfileComponent::OnRegister()
{
	Super::OnRegister();
	BindToPawnExtension();
}

void URpgCombatDefenseProfileComponent::BeginPlay()
{
	Super::BeginPlay();
	BindToPawnExtension();
}

void URpgCombatDefenseProfileComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ApplyRetryTimerHandle);
	}

	ClearAppliedProfile();
	UnbindFromPawnExtension();

	Super::EndPlay(EndPlayReason);
}

void URpgCombatDefenseProfileComponent::ApplyDefenseProfile()
{
	HandleAbilitySystemInitialized();
}

void URpgCombatDefenseProfileComponent::BindToPawnExtension()
{
	if (BoundPawnExtension)
	{
		return;
	}

	URpgPawnExtensionComponent* PawnExtension = URpgPawnExtensionComponent::FindPawnExtensionComponent(GetOwner());
	if (!PawnExtension)
	{
		return;
	}

	BoundPawnExtension = PawnExtension;
	PawnExtension->OnAbilitySystemInitialized_RegisterAndCall(
		FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::HandleAbilitySystemInitialized));
	PawnExtension->OnAbilitySystemUninitialized_Register(
		FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::HandleAbilitySystemUninitialized));
}

void URpgCombatDefenseProfileComponent::UnbindFromPawnExtension()
{
	if (!BoundPawnExtension)
	{
		return;
	}

	BoundPawnExtension->OnAbilitySystemInitialized.RemoveAll(this);
	BoundPawnExtension->OnAbilitySystemUninitialized.RemoveAll(this);
	BoundPawnExtension = nullptr;
}

void URpgCombatDefenseProfileComponent::HandleAbilitySystemInitialized()
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || !BoundPawnExtension)
	{
		return;
	}

	URpgAbilitySystemComponent* ASC = BoundPawnExtension->GetRpgAbilitySystemComponent();
	if (!ASC)
	{
		ScheduleApplyRetry();
		return;
	}

	ApplyProfileToAbilitySystem(ASC);
}

void URpgCombatDefenseProfileComponent::HandleAbilitySystemUninitialized()
{
	ClearAppliedProfile();
}

void URpgCombatDefenseProfileComponent::ApplyProfileToAbilitySystem(URpgAbilitySystemComponent* ASC)
{
	if (!ASC)
	{
		return;
	}

	if (!ASC->GetSet<URpgDefenseSet>())
	{
		ScheduleApplyRetry();
		return;
	}

	ClearAppliedProfile();

	const FRpgCombatDefenseProfileData& ProfileData = GetResolvedProfileData();
	ASC->SetNumericAttributeBase(URpgDefenseSet::GetMaxStaggerAttribute(), FMath::Max(1.0f, ProfileData.MaxStagger));
	ASC->SetNumericAttributeBase(URpgDefenseSet::GetIncomingStaggerDamageMultiplierAttribute(), FMath::Max(0.0f, ProfileData.IncomingStaggerDamageMultiplier));
	ASC->SetNumericAttributeBase(URpgDefenseSet::GetStaggerDurationAttribute(), FMath::Max(0.0f, ProfileData.StaggerDuration));
	ASC->SetNumericAttributeBase(URpgDefenseSet::GetStaggerImmunityDurationAttribute(), FMath::Max(0.0f, ProfileData.StaggerImmunityDuration));
	ASC->SetNumericAttributeBase(URpgDefenseSet::GetStaggeredDamageTakenMultiplierAttribute(), FMath::Max(0.0f, ProfileData.StaggeredDamageTakenMultiplier));

	ASC->SetLooseGameplayTagCount(
		RpgGameplayTags::Trait_Staggerable,
		ProfileData.bCanBeStaggered ? 1 : 0,
		EGameplayTagReplicationState::TagAndCountToAll);

	if (!ProfileData.bCanBeStaggered)
	{
		ASC->SetNumericAttributeBase(URpgDefenseSet::GetStaggerAttribute(), 0.0f);
		ASC->RemoveTimedLooseGameplayTag(RpgGameplayTags::State_StaggerImmune, EGameplayTagReplicationState::TagAndCountToAll);
	}

	AppliedAbilitySystemComponent = ASC;
}

void URpgCombatDefenseProfileComponent::ClearAppliedProfile()
{
	if (!AppliedAbilitySystemComponent)
	{
		return;
	}

	AppliedAbilitySystemComponent->SetLooseGameplayTagCount(
		RpgGameplayTags::Trait_Staggerable,
		0,
		EGameplayTagReplicationState::TagAndCountToAll);
	AppliedAbilitySystemComponent->RemoveTimedLooseGameplayTag(
		RpgGameplayTags::State_StaggerImmune,
		EGameplayTagReplicationState::TagAndCountToAll);

	AppliedAbilitySystemComponent = nullptr;
}

void URpgCombatDefenseProfileComponent::ScheduleApplyRetry()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			ApplyRetryTimerHandle,
			this,
			&ThisClass::HandleAbilitySystemInitialized,
			0.1f,
			false);
	}
}

const FRpgCombatDefenseProfileData& URpgCombatDefenseProfileComponent::GetResolvedProfileData() const
{
	return DefenseProfile ? DefenseProfile->GetProfileData() : FallbackProfileData;
}

URpgPlayerCombatDefenseProfileComponent::URpgPlayerCombatDefenseProfileComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void URpgPlayerCombatDefenseProfileComponent::OnRegister()
{
	Super::OnRegister();
	BindToPlayerController();
}

void URpgPlayerCombatDefenseProfileComponent::BeginPlay()
{
	Super::BeginPlay();
	BindToPlayerController();
}

void URpgPlayerCombatDefenseProfileComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ApplyRetryTimerHandle);
	}

	ClearAppliedProfile();
	UnbindFromPawnExtension();
	UnbindFromPlayerController();

	Super::EndPlay(EndPlayReason);
}

void URpgPlayerCombatDefenseProfileComponent::ApplyDefenseProfile()
{
	HandleAbilitySystemInitialized();
}

void URpgPlayerCombatDefenseProfileComponent::BindToPlayerController()
{
	if (BoundPlayerController)
	{
		return;
	}

	AController* Controller = Cast<AController>(GetOwner());
	if (!Controller)
	{
		return;
	}

	BoundPlayerController = Controller;
	Controller->OnPossessedPawnChanged.AddDynamic(this, &ThisClass::HandlePossessedPawnChanged);

	if (Controller->HasAuthority())
	{
		BindToPawn(Controller->GetPawn());
	}
}

void URpgPlayerCombatDefenseProfileComponent::UnbindFromPlayerController()
{
	if (!BoundPlayerController)
	{
		return;
	}

	BoundPlayerController->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::HandlePossessedPawnChanged);
	BoundPlayerController = nullptr;
}

void URpgPlayerCombatDefenseProfileComponent::BindToPawn(APawn* Pawn)
{
	if (!Pawn)
	{
		return;
	}

	URpgPawnExtensionComponent* PawnExtension = URpgPawnExtensionComponent::FindPawnExtensionComponent(Pawn);
	if (!PawnExtension)
	{
		ScheduleApplyRetry();
		return;
	}

	if (BoundPawnExtension == PawnExtension)
	{
		return;
	}

	UnbindFromPawnExtension();
	BoundPawnExtension = PawnExtension;
	PawnExtension->OnAbilitySystemInitialized_RegisterAndCall(
		FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::HandleAbilitySystemInitialized));
	PawnExtension->OnAbilitySystemUninitialized_Register(
		FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::HandleAbilitySystemUninitialized));
}

void URpgPlayerCombatDefenseProfileComponent::UnbindFromPawnExtension()
{
	if (!BoundPawnExtension)
	{
		return;
	}

	BoundPawnExtension->OnAbilitySystemInitialized.RemoveAll(this);
	BoundPawnExtension->OnAbilitySystemUninitialized.RemoveAll(this);
	BoundPawnExtension = nullptr;
}

void URpgPlayerCombatDefenseProfileComponent::HandleAbilitySystemInitialized()
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	if (!BoundPawnExtension)
	{
		if (AController* Controller = Cast<AController>(Owner))
		{
			BindToPawn(Controller->GetPawn());
		}
	}

	URpgAbilitySystemComponent* ASC = BoundPawnExtension ? BoundPawnExtension->GetRpgAbilitySystemComponent() : nullptr;
	if (!ASC)
	{
		ScheduleApplyRetry();
		return;
	}

	ApplyProfileToAbilitySystem(ASC);
}

void URpgPlayerCombatDefenseProfileComponent::HandleAbilitySystemUninitialized()
{
	ClearAppliedProfile();
}

void URpgPlayerCombatDefenseProfileComponent::HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	if (AActor* Owner = GetOwner(); !Owner || !Owner->HasAuthority())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ApplyRetryTimerHandle);
	}

	ClearAppliedProfile();
	UnbindFromPawnExtension();
	BindToPawn(NewPawn);
}

void URpgPlayerCombatDefenseProfileComponent::ApplyProfileToAbilitySystem(URpgAbilitySystemComponent* ASC)
{
	if (!ASC)
	{
		return;
	}

	if (!ASC->GetSet<URpgDefenseSet>())
	{
		ScheduleApplyRetry();
		return;
	}

	ClearAppliedProfile();

	const FRpgCombatDefenseProfileData& ProfileData = GetResolvedProfileData();
	ASC->SetNumericAttributeBase(URpgDefenseSet::GetMaxStaggerAttribute(), FMath::Max(1.0f, ProfileData.MaxStagger));
	ASC->SetNumericAttributeBase(URpgDefenseSet::GetIncomingStaggerDamageMultiplierAttribute(), FMath::Max(0.0f, ProfileData.IncomingStaggerDamageMultiplier));
	ASC->SetNumericAttributeBase(URpgDefenseSet::GetStaggerDurationAttribute(), FMath::Max(0.0f, ProfileData.StaggerDuration));
	ASC->SetNumericAttributeBase(URpgDefenseSet::GetStaggerImmunityDurationAttribute(), FMath::Max(0.0f, ProfileData.StaggerImmunityDuration));
	ASC->SetNumericAttributeBase(URpgDefenseSet::GetStaggeredDamageTakenMultiplierAttribute(), FMath::Max(0.0f, ProfileData.StaggeredDamageTakenMultiplier));

	ASC->SetLooseGameplayTagCount(
		RpgGameplayTags::Trait_Staggerable,
		ProfileData.bCanBeStaggered ? 1 : 0,
		EGameplayTagReplicationState::TagAndCountToAll);

	if (!ProfileData.bCanBeStaggered)
	{
		ASC->SetNumericAttributeBase(URpgDefenseSet::GetStaggerAttribute(), 0.0f);
		ASC->RemoveTimedLooseGameplayTag(RpgGameplayTags::State_StaggerImmune, EGameplayTagReplicationState::TagAndCountToAll);
	}

	AppliedAbilitySystemComponent = ASC;
}

void URpgPlayerCombatDefenseProfileComponent::ClearAppliedProfile()
{
	if (!AppliedAbilitySystemComponent)
	{
		return;
	}

	AppliedAbilitySystemComponent->SetNumericAttributeBase(URpgDefenseSet::GetStaggerAttribute(), 0.0f);
	AppliedAbilitySystemComponent->SetLooseGameplayTagCount(
		RpgGameplayTags::Trait_Staggerable,
		0,
		EGameplayTagReplicationState::TagAndCountToAll);
	AppliedAbilitySystemComponent->RemoveTimedLooseGameplayTag(
		RpgGameplayTags::State_StaggerImmune,
		EGameplayTagReplicationState::TagAndCountToAll);

	AppliedAbilitySystemComponent = nullptr;
}

void URpgPlayerCombatDefenseProfileComponent::ScheduleApplyRetry()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			ApplyRetryTimerHandle,
			this,
			&ThisClass::HandleAbilitySystemInitialized,
			0.1f,
			false);
	}
}

const FRpgCombatDefenseProfileData& URpgPlayerCombatDefenseProfileComponent::GetResolvedProfileData() const
{
	return DefenseProfile ? DefenseProfile->GetProfileData() : FallbackProfileData;
}
