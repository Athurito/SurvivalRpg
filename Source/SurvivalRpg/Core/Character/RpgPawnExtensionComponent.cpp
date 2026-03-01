// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgPawnExtensionComponent.h"

#include "Components/GameFrameworkComponentManager.h"
#include "Net/UnrealNetwork.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/GameplayTags/GameplayTags.h"

const FName URpgPawnExtensionComponent::Name_ActorFeatureName = FName("PawnExtension");


URpgPawnExtensionComponent::URpgPawnExtensionComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	AbilitySystemComponent = nullptr;
	PawnData = nullptr;
}

void URpgPawnExtensionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(URpgPawnExtensionComponent, PawnData);
}

void URpgPawnExtensionComponent::BeginPlay()
{
	Super::BeginPlay();
	
	// Listen for changes to all features
	BindOnActorInitStateChanged(NAME_None, FGameplayTag(), false);
	
	// Notifies state manager that we have spawned, then try rest of default initialization
	ensure(TryToChangeInitState(RpgGameplayTags::InitState_Spawned));
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

void URpgPawnExtensionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UninitializeAbilitySystem();
	UnregisterInitStateFeature();
	Super::EndPlay(EndPlayReason);
}



void URpgPawnExtensionComponent::SetPawnData(const UBasePawnData* InPawnData)
{
	check(InPawnData);
	
	APawn* Pawn = GetPawnChecked<APawn>();
	
	if (Pawn->GetLocalRole() != ROLE_Authority)
	{
		return;
	}
	
	if (PawnData)
	{
		//Already Set
		return;
	}
		
	PawnData = InPawnData;
	
	Pawn->ForceNetUpdate();

	CheckDefaultInitialization();
}

void URpgPawnExtensionComponent::OnRep_PawnData()
{
	CheckDefaultInitialization();
}

void URpgPawnExtensionComponent::InitializeAbilitySystemComponent(URpgAbilitySystemComponent* InAsc, AActor* InOwner)
{
	check(InAsc);
	check(InOwner);
	
	if (AbilitySystemComponent == InAsc)
	{
		// The ability system component hasn't changed.
		return;
	}
	
	if (AbilitySystemComponent)
	{
		// Clean up the old ability system component.
		UninitializeAbilitySystem();
	}
		
	APawn* Pawn = GetPawnChecked<APawn>();
	//Death or respawn
	// If the ASC is already initialized on another pawn, then uninitialize it from that pawn before initializing it on this one. This can happen during death/respawn when the same ASC is used again.
	AActor* ExistingAvatar = InAsc->GetAvatarActor();
	if ((ExistingAvatar != nullptr) && (ExistingAvatar != Pawn))
	{
		ensure(!ExistingAvatar->HasAuthority());
		if (URpgPawnExtensionComponent* PawnExt = FindPawnExtensionComponent(ExistingAvatar))
		{
			PawnExt->UninitializeAbilitySystem();
		}
	}
	
	AbilitySystemComponent = InAsc;
	AbilitySystemComponent->InitAbilityActorInfo(InOwner, Pawn);
	OnAbilitySystemInitialized.Broadcast();
}

void URpgPawnExtensionComponent::UninitializeAbilitySystem()
{
	if (!AbilitySystemComponent) return;
	
	if (AbilitySystemComponent->GetAvatarActor() == GetOwner())
	{
		AbilitySystemComponent->CancelAbilities();
		AbilitySystemComponent->RemoveAllGameplayCues();
		if (AbilitySystemComponent->GetOwnerActor() != nullptr)
		{
			AbilitySystemComponent->SetAvatarActor(nullptr);
		}
		else
		{
			AbilitySystemComponent->ClearActorInfo();
		}
		OnAbilitySystemUninitialized.Broadcast();
	}
	AbilitySystemComponent = nullptr;
}

void URpgPawnExtensionComponent::HandleControllerChanged()
{
	if (AbilitySystemComponent && (AbilitySystemComponent->GetAvatarActor() == GetPawnChecked<APawn>()))
	{
		ensure(AbilitySystemComponent->AbilityActorInfo->OwnerActor == AbilitySystemComponent->GetOwnerActor());
		if (AbilitySystemComponent->GetOwnerActor() == nullptr)
		{
			UninitializeAbilitySystem();
		}
		else
		{
			AbilitySystemComponent->RefreshAbilityActorInfo();
		}
	}

	CheckDefaultInitialization();
}

void URpgPawnExtensionComponent::HandlePlayerStateReplicated()
{
	CheckDefaultInitialization();
}

void URpgPawnExtensionComponent::SetupPlayerInputComponent()
{
	CheckDefaultInitialization();
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

bool URpgPawnExtensionComponent::CanChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState, FGameplayTag DesiredState) const
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
		
		const bool bHasAuthority = Pawn->HasAuthority();
		const bool bIsLocallyControlled = Pawn->IsLocallyControlled();
		
		if (bHasAuthority || bIsLocallyControlled)
		{
			if (!GetController<APlayerController>()) 
				return false;
		}
		return true;
	}
	
	// -------------- DataInitialized --------------
	if (CurrentState == RpgGameplayTags::InitState_DataAvailable && DesiredState == RpgGameplayTags::InitState_DataInitialized)
	{
		// Transition to initialize if all features have their data available
		return Manager->HaveAllFeaturesReachedInitState(Pawn, RpgGameplayTags::InitState_DataAvailable);
	}
	
	// -------------- GameplayReady --------------
	if (CurrentState == RpgGameplayTags::InitState_DataInitialized && DesiredState == RpgGameplayTags::InitState_GameplayReady)
	{
		return true;
	}
	return false;
}

void URpgPawnExtensionComponent::HandleChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState, FGameplayTag DesiredState)
{
	if (DesiredState == RpgGameplayTags::InitState_DataInitialized)
	{
		// This is currently all handled by other components listening to this state change
	}
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

void URpgPawnExtensionComponent::OnAbilitySystemInitialized_RegisterAndCall(FSimpleMulticastDelegate::FDelegate Delegate)
{
	if (!OnAbilitySystemInitialized.IsBoundToObject(Delegate.GetUObject()))
	{
		OnAbilitySystemInitialized.Add(Delegate);
	}

	if (AbilitySystemComponent)
	{
		Delegate.Execute();
	}
}

void URpgPawnExtensionComponent::OnAbilitySystemUninitialized_Register(FSimpleMulticastDelegate::FDelegate Delegate)
{
	if (!OnAbilitySystemUninitialized.IsBoundToObject(Delegate.GetUObject()))
	{
		OnAbilitySystemUninitialized.Add(Delegate);
	}
}
