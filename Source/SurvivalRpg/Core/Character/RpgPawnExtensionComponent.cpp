// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgPawnExtensionComponent.h"

#include "Components/GameFrameworkComponentManager.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/GameplayTags/GameplayTags.h"

const FName URpgPawnExtensionComponent::Name_ActorFeatureName = FName("RpgPawnExtensionComponent");

bool URpgPawnExtensionComponent::CanChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState,
	FGameplayTag DesiredState) const
{
	check(Manager);
	APawn* Pawn = GetPawn<APawn>();
	
	// -------------- Spawned --------------
	if (!CurrentState.IsValid() && DesiredState == RpgGameplayTags::InitState_Spawned)
	{
		if (Pawn) return true;
	}

	// -------------- DataAvailable --------------
	if (CurrentState == RpgGameplayTags::InitState_Spawned && DesiredState == RpgGameplayTags::InitState_DataAvailable)
	{
		if (!PawnData) return false;
		if (Pawn->IsLocallyControlled())
		{
			if (!GetController<APlayerController>()) 
				return false;
		}
		return true;
	}
	
	// -------------- DataInitialized --------------
	if (CurrentState == RpgGameplayTags::InitState_DataAvailable && DesiredState == RpgGameplayTags::InitState_DataInitialized)
	{
		return Manager->HaveAllFeaturesReachedInitState(Pawn, RpgGameplayTags::InitState_DataAvailable);
	}
	
	// -------------- GameplayReady --------------
	if (CurrentState == RpgGameplayTags::InitState_DataInitialized && DesiredState == RpgGameplayTags::InitState_GameplayReady)
	{
		return true;
	}
	return false;
}

void URpgPawnExtensionComponent::HandleChangeInitState(UGameFrameworkComponentManager* Manager,
	FGameplayTag CurrentState, FGameplayTag DesiredState)
{
	// Nothing To Do
}

void URpgPawnExtensionComponent::OnActorInitStateChanged(const FActorInitStateChangedParams& Params)
{
	if (Params.FeatureName != Name_ActorFeatureName)
	{
		if (Params.FeatureState == RpgGameplayTags::InitState_DataAvailable)
		{
			CheckDefaultInitialization();
		}
	}
}

void URpgPawnExtensionComponent::CheckDefaultInitialization()
{
	CheckDefaultInitializationForImplementers();
	
	const TArray<FGameplayTag> StateChain = {
		RpgGameplayTags::InitState_Spawned,
		RpgGameplayTags::InitState_DataAvailable,
		RpgGameplayTags::InitState_DataInitialized,
		RpgGameplayTags::InitState_GameplayReady
	};
	
	ContinueInitStateChain(StateChain);
}

void URpgPawnExtensionComponent::InitializeAbilitySystemComponent(URpgAbilitySystemComponent* InAsc, AActor* InOwner)
{
	check(InAsc);
	check(InOwner);
	
	if (AbilitySystemComponent == InAsc) return;
	
	if (AbilitySystemComponent)
		return UninitializeAbilitySystemComponent();
		
	APawn* Pawn = GetPawnChecked<APawn>();
	
	//Death or respawn
	// If the ASC is already initialized on another pawn, then uninitialize it from that pawn before initializing it on this one. This can happen during death/respawn when the same ASC is used again.
	AActor* ExistingAvatar = InAsc->GetAvatarActor();
	if (ExistingAvatar && ExistingAvatar != Pawn)
	{
		if (URpgPawnExtensionComponent* PawnExt = FindPawnExtensionComponent(ExistingAvatar))
		{
			PawnExt->UninitializeAbilitySystemComponent();
		}
	}
		
	
	
	AbilitySystemComponent = InAsc;
	AbilitySystemComponent->InitAbilityActorInfo(InOwner, Pawn);
	
}

void URpgPawnExtensionComponent::UninitializeAbilitySystemComponent()
{
	if (!AbilitySystemComponent) return;
	
	if (AbilitySystemComponent->GetAvatarActor() == GetOwner())
	{
		AbilitySystemComponent->CancelAbilities();
		if (AbilitySystemComponent->GetAvatarActor())
		{
			AbilitySystemComponent->SetAvatarActor(nullptr);
		}
		else
		{
			AbilitySystemComponent->ClearActorInfo();
		}
	}
	AbilitySystemComponent = nullptr;
}

URpgPawnExtensionComponent::URpgPawnExtensionComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.bCanEverTick = false;
	AbilitySystemComponent = nullptr;
}

void URpgPawnExtensionComponent::SetPawnData(const UBasePawnData* InPawnData)
{
	check(InPawnData);
	if (PawnData) return;
	PawnData = InPawnData;
}

void URpgPawnExtensionComponent::SetupPlayerInputComponent()
{
	CheckDefaultInitialization();
}

void URpgPawnExtensionComponent::BeginPlay()
{
	Super::BeginPlay();
	BindOnActorInitStateChanged(NAME_None, FGameplayTag(), false);
	TryToChangeInitState(RpgGameplayTags::InitState_Spawned);
	CheckDefaultInitialization();
}

void URpgPawnExtensionComponent::OnRegister()
{
	Super::OnRegister();
	if (GetPawn<APawn>())
	{
		RegisterInitStateFeature();
	}
}
