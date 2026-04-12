// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgAbilitySystemComponent.h"

#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "RpgGlobalAbilitySystem.h"
#include "SurvivalRpg/Animation/RpgAnimInstance.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/GameplayTags/GameplayTags.h"

UE_DEFINE_GAMEPLAY_TAG(TAG_Gameplay_AbilityInputBlocked, "Gameplay.AbilityInputBlocked");

namespace
{
	FPredictionKey GetAbilitySpecActivationPredictionKey(FGameplayAbilitySpec& Spec)
	{
		FPredictionKey ActivationPredictionKey;

PRAGMA_DISABLE_DEPRECATION_WARNINGS
		TArray<UGameplayAbility*> Instances = Spec.GetAbilityInstances();
		if (!Instances.IsEmpty() && Instances.Last())
		{
			ActivationPredictionKey = Instances.Last()->GetCurrentActivationInfoRef().GetActivationPredictionKey();
		}
		else
		{
			ActivationPredictionKey = Spec.ActivationInfo.GetActivationPredictionKey();
		}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

		return ActivationPredictionKey;
	}
}

URpgAbilitySystemComponent::URpgAbilitySystemComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	InputPressedSpecHandles.Reset();
	InputReleasedSpecHandles.Reset();
	InputHeldSpecHandles.Reset();

	FMemory::Memset(ActivationGroupCounts, 0, sizeof(ActivationGroupCounts));
}

void URpgAbilitySystemComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (URpgGlobalAbilitySystem* GlobalAbilitySystem = UWorld::GetSubsystem<URpgGlobalAbilitySystem>(GetWorld()))
	{
		GlobalAbilitySystem->UnregisterASC(this);
	}
	Super::EndPlay(EndPlayReason);
}

void URpgAbilitySystemComponent::InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor)
{
	FGameplayAbilityActorInfo* ActorInfo = AbilityActorInfo.Get();
	check(ActorInfo);
	check(InOwnerActor);

	const bool bHasNewPawnAvatar = Cast<APawn>(InAvatarActor) && (InAvatarActor != ActorInfo->AvatarActor);
	
	Super::InitAbilityActorInfo(InOwnerActor, InAvatarActor);
	
	if (bHasNewPawnAvatar)
	{
		// Notify all abilities that a new pawn avatar has been set
		for (const FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities.Items)
		{
			PRAGMA_DISABLE_DEPRECATION_WARNINGS
						ensureMsgf(AbilitySpec.Ability && AbilitySpec.Ability->GetInstancingPolicy() != EGameplayAbilityInstancingPolicy::NonInstanced, TEXT("InitAbilityActorInfo: All Abilities should be Instanced (NonInstanced is being deprecated due to usability issues)."));
			PRAGMA_ENABLE_DEPRECATION_WARNINGS
	
			TArray<UGameplayAbility*> Instances = AbilitySpec.GetAbilityInstances();
			for (UGameplayAbility* AbilityInstance : Instances)
			{
				URpgGameplayAbility* RpgAbilityInstance = Cast<URpgGameplayAbility>(AbilityInstance);
				if (RpgAbilityInstance)
				{
					// Ability instances may be missing for replays
					RpgAbilityInstance->OnPawnAvatarSet();
				}
			}
		}

		// Register with the global system once we actually have a pawn avatar. We wait until this time since some globally-applied effects may require an avatar.
		if (URpgGlobalAbilitySystem* GlobalAbilitySystem = UWorld::GetSubsystem<URpgGlobalAbilitySystem>(GetWorld()))
		{
			GlobalAbilitySystem->RegisterASC(this);
		}

		if (URpgAnimInstance* RpgAnimInst = Cast<URpgAnimInstance>(ActorInfo->GetAnimInstance()))
		{
			RpgAnimInst->InitializeWithAbilitySystem(this);
		}

		TryActivateAbilitiesOnSpawn();
	}
}

void URpgAbilitySystemComponent::TryActivateAbilitiesOnSpawn()
{
	ABILITYLIST_SCOPE_LOCK();
	for (const FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities.Items)
	{
		if (const URpgGameplayAbility* RpgAbilityCDO = Cast<URpgGameplayAbility>(AbilitySpec.Ability))
		{
			RpgAbilityCDO->TryActivateAbilityOnSpawn(AbilityActorInfo.Get(), AbilitySpec);
		}
	}
}

void URpgAbilitySystemComponent::AbilityInputTagPressed(const FGameplayTag& InputTag)
{
	if (InputTag.IsValid())
	{
		for (const FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities.Items)
		{
			if (AbilitySpec.Ability && (AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag)))
			{
				InputPressedSpecHandles.AddUnique(AbilitySpec.Handle);
				InputHeldSpecHandles.AddUnique(AbilitySpec.Handle);
			}
		}
	}
}

void URpgAbilitySystemComponent::AbilityInputTagReleased(const FGameplayTag& InputTag)
{
	if (InputTag.IsValid())
	{
		for (const FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities.Items)
		{
			if (AbilitySpec.Ability && (AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag)))
			{
				InputReleasedSpecHandles.AddUnique(AbilitySpec.Handle);
				InputHeldSpecHandles.Remove(AbilitySpec.Handle);
			}
		}
	}
}

void URpgAbilitySystemComponent::ProcessAbilityInput(float DeltaTime, bool bGamePaused)
{
	if (HasMatchingGameplayTag(TAG_Gameplay_AbilityInputBlocked))
	{
		ClearAbilityInput();
		return;
	}

	static TArray<FGameplayAbilitySpecHandle> AbilitiesToActivate;
	AbilitiesToActivate.Reset();

	//@TODO: See if we can use FScopedServerAbilityRPCBatcher ScopedRPCBatcher in some of these loops

	//
	// Process all abilities that activate when the input is held.
	//
	for (const FGameplayAbilitySpecHandle& SpecHandle : InputHeldSpecHandles)
	{
		if (const FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromHandle(SpecHandle))
		{
			if (AbilitySpec->Ability && !AbilitySpec->IsActive())
			{
				const URpgGameplayAbility* RpgAbilityCDO = Cast<URpgGameplayAbility>(AbilitySpec->Ability);
				if (RpgAbilityCDO && RpgAbilityCDO->GetActivationPolicy() == ERpgAbilityActivationPolicy::WhileInputActive)
				{
					AbilitiesToActivate.AddUnique(AbilitySpec->Handle);
				}
			}
		}
	}

	//
	// Process all abilities that had their input pressed this frame.
	//
	for (const FGameplayAbilitySpecHandle& SpecHandle : InputPressedSpecHandles)
	{
		if (FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromHandle(SpecHandle))
		{
			if (AbilitySpec->Ability)
			{
				AbilitySpec->InputPressed = true;

				if (AbilitySpec->IsActive())
				{
					// Ability is active so pass along the input event.
					AbilitySpecInputPressed(*AbilitySpec);
				}
				else
				{
					const URpgGameplayAbility* RpgAbilityCDO = Cast<URpgGameplayAbility>(AbilitySpec->Ability);

					if (RpgAbilityCDO && RpgAbilityCDO->GetActivationPolicy() == ERpgAbilityActivationPolicy::OnInputTriggered)
					{
						AbilitiesToActivate.AddUnique(AbilitySpec->Handle);
					}
				}
			}
		}
	}

	//
	// Try to activate all the abilities that are from presses and holds.
	// We do it all at once so that held inputs don't activate the ability
	// and then also send a input event to the ability because of the press.
	//
	for (const FGameplayAbilitySpecHandle& AbilitySpecHandle : AbilitiesToActivate)
	{
		TryActivateAbility(AbilitySpecHandle);
	}

	//
	// Process all abilities that had their input released this frame.
	//
	for (const FGameplayAbilitySpecHandle& SpecHandle : InputReleasedSpecHandles)
	{
		if (FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromHandle(SpecHandle))
		{
			if (AbilitySpec->Ability)
			{
				AbilitySpec->InputPressed = false;

				if (AbilitySpec->IsActive())
				{
					// Ability is active so pass along the input event.
					AbilitySpecInputReleased(*AbilitySpec);
				}
			}
		}
	}

	//
	// Clear the cached ability handles.
	//
	InputPressedSpecHandles.Reset();
	InputReleasedSpecHandles.Reset();
}

void URpgAbilitySystemComponent::ClearAbilityInput()
{
	InputPressedSpecHandles.Reset();
	InputReleasedSpecHandles.Reset();
	InputHeldSpecHandles.Reset();
}

void URpgAbilitySystemComponent::AbilitySpecInputPressed(FGameplayAbilitySpec& Spec)
{
	Super::AbilitySpecInputPressed(Spec);

	if (Spec.IsActive())
	{
		InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputPressed, Spec.Handle, GetAbilitySpecActivationPredictionKey(Spec));
	}
}

void URpgAbilitySystemComponent::AbilitySpecInputReleased(FGameplayAbilitySpec& Spec)
{
	Super::AbilitySpecInputReleased(Spec);

	if (Spec.IsActive())
	{
		InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased, Spec.Handle, GetAbilitySpecActivationPredictionKey(Spec));
	}
}


bool URpgAbilitySystemComponent::GrantAbilitySet(const URpgAbilitySet* AbilitySet, UObject* SourceObject)
{
	if (!IsValid(AbilitySet))
	{
		return false;
	}

	// Wenn Client: an Server delegieren
	if (!HasGrantAuthority())
	{
		Server_GrantAbilitySet(AbilitySet, SourceObject);
		return true; // Anfrage raus, Server repliziert Ergebnis
	}

	if (OwnerPlayerState)
	{
		OwnerPlayerState->SendAbilitiesChangedEvent();
	}

	return GrantAbilitySet_Internal(AbilitySet, SourceObject);
}

bool URpgAbilitySystemComponent::RemoveAbilitySet(const URpgAbilitySet* AbilitySet)
{
	if (!IsValid(AbilitySet))
	{
		return false;
	}

	if (!HasGrantAuthority())
	{
		Server_RemoveAbilitySet(AbilitySet);
		return true;
	}

	if (OwnerPlayerState)
	{
		OwnerPlayerState->SendAbilitiesChangedEvent();
	}

	return RemoveAbilitySet_Internal(AbilitySet);
}

bool URpgAbilitySystemComponent::HasAbilitySet(const URpgAbilitySet* AbilitySet) const
{
	return IsValid(AbilitySet) && GrantedAbilitySets.Contains(AbilitySet);
}

bool URpgAbilitySystemComponent::HasGrantAuthority() const
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bForceGrantAuthorityForTests)
	{
		return true;
	}
#endif

	return IsOwnerActorAuthoritative();
}

void URpgAbilitySystemComponent::Server_GrantAbilitySet_Implementation(const URpgAbilitySet* AbilitySet, UObject* SourceObject)
{
	GrantAbilitySet_Internal(AbilitySet, SourceObject);
}

void URpgAbilitySystemComponent::Server_RemoveAbilitySet_Implementation(const URpgAbilitySet* AbilitySet)
{
	RemoveAbilitySet_Internal(AbilitySet);
}

bool URpgAbilitySystemComponent::GrantAbilitySet_Internal(const URpgAbilitySet* AbilitySet, UObject* SourceObject)
{
	if (!IsValid(AbilitySet))
	{
		return false;
	}

	// Optional: doppelt grant verhindern
	if (GrantedAbilitySets.Contains(AbilitySet))
	{
		return false;
	}

	FRpgAbilitySet_GrantedHandles Handles;
	AbilitySet->GiveToAbilitySystem(this, &Handles, SourceObject);

	GrantedAbilitySets.Add(AbilitySet, Handles);
	return true;
}

bool URpgAbilitySystemComponent::RemoveAbilitySet_Internal(const URpgAbilitySet* AbilitySet)
{
	if (!IsValid(AbilitySet))
	{
		return false;
	}

	FRpgAbilitySet_GrantedHandles* Handles = GrantedAbilitySets.Find(AbilitySet);
	if (!Handles)
	{
		return false;
	}

	Handles->TakeFromAbilitySystem(this);
	GrantedAbilitySets.Remove(AbilitySet);
	return true;
}

void URpgAbilitySystemComponent::ApplyDefaultAbilitySetupIfNeeded(UObject* SourceObject)
{
	if (bDefaultSetupApplied)
	{
		return;
	}

	if (!HasGrantAuthority())
	{
		return; // Abilities/Effects nur serverseitig geben
	}

	// Safety: ActorInfo muss gültig sein
	if (!AbilityActorInfo.IsValid() || AbilityActorInfo->AvatarActor.Get() == nullptr)
	{
		return;
	}

	if (DefaultAbilitySetup)
	{
		DefaultAbilitySetup->GiveToAbilitySystem(this, &DefaultGrantedHandles, SourceObject);
	}

	bDefaultSetupApplied = true;
}

void URpgAbilitySystemComponent::RemoveDefaultAbilitySetup()
{
	if (!HasGrantAuthority())
	{
		return;
	}

	if (bDefaultSetupApplied)
	{
		DefaultGrantedHandles.TakeFromAbilitySystem(this);
		bDefaultSetupApplied = false;
	}
}

void URpgAbilitySystemComponent::ActivateAbilitiesByInputTag(FGameplayTag InputTag, bool bAllowRemoteActivation)
{
	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			TryActivateAbility(Spec.Handle, bAllowRemoteActivation);
		}
	}
}

void URpgAbilitySystemComponent::ResetForRevive()
{
	CancelAbilities();
	RemoveAllGameplayCues();
	ClearLifecycleEffects();
	ClearLifecycleTags();
}

void URpgAbilitySystemComponent::ResetForRespawn()
{
	ResetForRevive();

	FGameplayTagContainer RespawnClearedEffectTags;
	RespawnClearedEffectTags.AddTag(RpgGameplayTags::Effect_Behavior_ClearOnRespawn);

	RemoveActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(RespawnClearedEffectTags));
}

bool URpgAbilitySystemComponent::TryActivateFirstAbilityByClass(TSubclassOf<UGameplayAbility> AbilityClass, bool bAllowRemoteActivation)
{
	if (!IsValid(AbilityClass))
	{
		return false;
	}

	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (Spec.Ability && Spec.Ability->GetClass()->IsChildOf(AbilityClass))
		{
			return TryActivateAbility(Spec.Handle, bAllowRemoteActivation);
		}
	}

	return false;
}

void URpgAbilitySystemComponent::BeginPlay()
{
	Super::BeginPlay();
	
	OwnerPlayerState = Cast<ARpgPlayerState>(GetOwner());
}

void URpgAbilitySystemComponent::OnRep_ActivateAbilities()
{
	Super::OnRep_ActivateAbilities();
	
	if (!OwnerPlayerState) return;
	
	bool bAbilitiesChanged = false;
	
	if (LastActiveAbilities.Num() != ActivatableAbilities.Items.Num())
	{
		bAbilitiesChanged = true;
	}
	else
	{
		for (int32 i = 0; i < LastActiveAbilities.Num(); ++i)
		{
			if (LastActiveAbilities[i].Ability != ActivatableAbilities.Items[i].Ability)
			{
				bAbilitiesChanged = true;
				break;
			}
		}
	}
	
	if (bAbilitiesChanged)
	{
		OwnerPlayerState->SendAbilitiesChangedEvent();
		LastActiveAbilities = ActivatableAbilities.Items;
	}
}

void URpgAbilitySystemComponent::ClearLifecycleTags()
{
	SetLooseGameplayTagCount(RpgGameplayTags::State_Dead, 0);
	SetLooseGameplayTagCount(RpgGameplayTags::Status_Death, 0);
	SetLooseGameplayTagCount(RpgGameplayTags::Status_Death_Dying, 0);
	SetLooseGameplayTagCount(RpgGameplayTags::Status_Death_Dead, 0);
	SetLooseGameplayTagCount(RpgGameplayTags::Status_Dead_WaitingForRespawn, 0);
	SetLooseGameplayTagCount(RpgGameplayTags::Status_Downed, 0);
	SetLooseGameplayTagCount(RpgGameplayTags::Status_Downed_BleedingOut, 0);
	SetLooseGameplayTagCount(RpgGameplayTags::Status_Downed_Reviving, 0);
}

void URpgAbilitySystemComponent::ClearLifecycleEffects()
{
	FGameplayTagContainer LifecycleTags;
	LifecycleTags.AddTag(RpgGameplayTags::State_Dead);
	LifecycleTags.AddTag(RpgGameplayTags::Status_Death);
	LifecycleTags.AddTag(RpgGameplayTags::Status_Death_Dying);
	LifecycleTags.AddTag(RpgGameplayTags::Status_Death_Dead);
	LifecycleTags.AddTag(RpgGameplayTags::Status_Dead_WaitingForRespawn);
	LifecycleTags.AddTag(RpgGameplayTags::Status_Downed);
	LifecycleTags.AddTag(RpgGameplayTags::Status_Downed_BleedingOut);
	LifecycleTags.AddTag(RpgGameplayTags::Status_Downed_Reviving);

	RemoveActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(LifecycleTags));
}

