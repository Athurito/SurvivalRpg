// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgPawnExtensionComponent.h"

#include "Components/GameFrameworkComponentManager.h"
#include "GameFramework/Controller.h"
#include "Net/UnrealNetwork.h"
#include "SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.h"
#include "SurvivalRpg/Core/Character/RpgPawnData.h"
#include "SurvivalRpg/Core/Player/RpgBasePlayerState.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"

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



void URpgPawnExtensionComponent::SetPawnData(const URpgPawnData* InPawnData)
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
		const bool bPreInitializedWithOwnerAsAvatar = (ExistingAvatar == InOwner);

		if (!bPreInitializedWithOwnerAsAvatar)
		{
			// Eher echter Alt-Avatar (z. B. Respawn alter Pawn)
			if (URpgPawnExtensionComponent* PawnExt = FindPawnExtensionComponent(ExistingAvatar))
			{
				PawnExt->UninitializeAbilitySystem();
			}
			else
			{
				// Optional nur Logging statt ensure-crash
				UE_LOG(LogTemp, Warning, TEXT("ASC had unexpected existing avatar: %s"), *GetNameSafe(ExistingAvatar));
			}
		}
		else
		{
			// Plugin hat ASC mit PlayerState als Avatar vorinitialisiert -> okay, wir binden jetzt korrekt auf Pawn um.
			UE_LOG(LogTemp, Verbose, TEXT("ASC pre-initialized with Owner as Avatar; rebinding to Pawn."));
		}
	}
	
	AbilitySystemComponent = InAsc;
	AbilitySystemComponent->InitAbilityActorInfo(InOwner, Pawn);

	if (ensure(PawnData))
	{
		InAsc->SetTagRelationshipMapping(PawnData->TagRelationshipMapping);
	}

	OnAbilitySystemInitialized.Broadcast();
}

void URpgPawnExtensionComponent::UninitializeAbilitySystem()
{
	if (!AbilitySystemComponent) return;
	
	if (AbilitySystemComponent->GetAvatarActor() == GetOwner())
	{
		FGameplayTagContainer AbilityTypesToIgnore;
		AbilityTypesToIgnore.AddTag(RpgGameplayTags::Ability_Behavior_SurvivesDeath);

		AbilitySystemComponent->CancelAbilities(nullptr, &AbilityTypesToIgnore);
		AbilitySystemComponent->ClearAbilityInput();
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
		if (!GetPlayerState<ARpgBasePlayerState>()) return false;
		
		const bool bHasAuthority = Pawn->HasAuthority();
		const bool bIsLocallyControlled = Pawn->IsLocallyControlled();
		
		if (bHasAuthority || bIsLocallyControlled)
		{
			if (!GetController<AController>()) 
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
		if (ARpgBasePlayerState* PlayerState = GetPlayerState<ARpgBasePlayerState>())
		{
			if (URpgAbilitySystemComponent* AbilitySystem = PlayerState->GetRpgAbilitySystemComponent())
			{
				InitializeAbilitySystemComponent(AbilitySystem, PlayerState);
			}
		}
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
