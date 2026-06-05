// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgAbilitySystemComponent.h"

#include "GameplayEffect.h"
#include "RpgAbilityTagRelationshipMapping.h"
#include "RpgGlobalAbilitySystem.h"
#include "SurvivalRpg/SurvivalRpg.h"
#include "SurvivalRpg/Animation/RpgAnimInstance.h"
#include "SurvivalRpg/Core/Player/RpgBasePlayerState.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/System/RpgAssetManager.h"
#include "SurvivalRpg/System/RpgGameData.h"

UE_DEFINE_GAMEPLAY_TAG(TAG_Gameplay_AbilityInputBlocked, "Gameplay.AbilityInputBlocked");

namespace
{
bool AbilitySpecHasActivationTag(const FGameplayAbilitySpec& Spec, const FGameplayTag& ActivationTag)
{
	if (!Spec.Ability || !ActivationTag.IsValid())
	{
		return false;
	}

	if (Spec.GetDynamicSpecSourceTags().HasTagExact(ActivationTag))
	{
		return true;
	}

	return Spec.Ability->GetAssetTags().HasTagExact(ActivationTag);
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
	if (UWorld* World = GetWorld())
	{
		for (TPair<FGameplayTag, FTimerHandle>& Entry : TimedLooseTagTimerHandles)
		{
			World->GetTimerManager().ClearTimer(Entry.Value);
		}
	}
	TimedLooseTagTimerHandles.Reset();

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
	OwnerPlayerState = Cast<ARpgBasePlayerState>(InOwnerActor);
	
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

void URpgAbilitySystemComponent::CancelAbilitiesByFunc(TShouldCancelAbilityFunc ShouldCancelFunc, bool bReplicateCancelAbility)
{
	ABILITYLIST_SCOPE_LOCK();
	for (const FGameplayAbilitySpec& AbilitySpec : ActivatableAbilities.Items)
	{
		if (!AbilitySpec.IsActive())
		{
			continue;
		}

		URpgGameplayAbility* RpgAbilityCDO = Cast<URpgGameplayAbility>(AbilitySpec.Ability);
		if (!RpgAbilityCDO)
		{
			UE_LOG(LogRpgAbilitySystem, Error, TEXT("CancelAbilitiesByFunc: Non-RpgGameplayAbility %s was Granted to ASC. Skipping."), *AbilitySpec.Ability.GetName());
			continue;
		}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
				ensureMsgf(AbilitySpec.Ability->GetInstancingPolicy() != EGameplayAbilityInstancingPolicy::NonInstanced, TEXT("CancelAbilitiesByFunc: All Abilities should be Instanced (NonInstanced is being deprecated due to usability issues)."));
PRAGMA_ENABLE_DEPRECATION_WARNINGS
			
				// Cancel all the spawned instances.
		TArray<UGameplayAbility*> Instances = AbilitySpec.GetAbilityInstances();
		for (UGameplayAbility* AbilityInstance : Instances)
		{
			URpgGameplayAbility* RpgAbilityInstance = CastChecked<URpgGameplayAbility>(AbilityInstance);

			if (ShouldCancelFunc(RpgAbilityInstance, AbilitySpec.Handle))
			{
				if (RpgAbilityInstance->CanBeCanceled())
				{
					RpgAbilityInstance->CancelAbility(AbilitySpec.Handle, AbilityActorInfo.Get(), RpgAbilityInstance->GetCurrentActivationInfo(), bReplicateCancelAbility);
				}
				else
				{
					UE_LOG(LogRpgAbilitySystem, Error, TEXT("CancelAbilitiesByFunc: Can't cancel ability [%s] because CanBeCanceled is false."), *RpgAbilityInstance->GetName());
				}
			}
		}
	}
}

void URpgAbilitySystemComponent::CancelInputActivatedAbilities(bool bReplicateCancelAbility)
{
	auto ShouldCancelFunc = [this](const URpgGameplayAbility* RpgAbility, FGameplayAbilitySpecHandle Handle)
	{
		const ERpgAbilityActivationPolicy ActivationPolicy = RpgAbility->GetActivationPolicy();
		return ((ActivationPolicy == ERpgAbilityActivationPolicy::OnInputTriggered) || (ActivationPolicy == ERpgAbilityActivationPolicy::WhileInputActive));
	};

	CancelAbilitiesByFunc(ShouldCancelFunc, bReplicateCancelAbility);
}

void URpgAbilitySystemComponent::AbilitySpecInputPressed(FGameplayAbilitySpec& Spec)
{
	Super::AbilitySpecInputPressed(Spec);

	if (Spec.IsActive())
	{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		const UGameplayAbility* Instance = Spec.GetPrimaryInstance();
		FPredictionKey OriginalPredictionKey = Instance ? Instance->GetCurrentActivationInfo().GetActivationPredictionKey() : Spec.ActivationInfo.GetActivationPredictionKey();
PRAGMA_ENABLE_DEPRECATION_WARNINGS
		
		InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputPressed, Spec.Handle, OriginalPredictionKey);
	}
}

void URpgAbilitySystemComponent::AbilitySpecInputReleased(FGameplayAbilitySpec& Spec)
{
	Super::AbilitySpecInputReleased(Spec);

	if (Spec.IsActive())
	{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		const UGameplayAbility* Instance = Spec.GetPrimaryInstance();
		FPredictionKey OriginalPredictionKey = Instance ? Instance->GetCurrentActivationInfo().GetActivationPredictionKey() : Spec.ActivationInfo.GetActivationPredictionKey();
PRAGMA_ENABLE_DEPRECATION_WARNINGS
		InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased, Spec.Handle, OriginalPredictionKey);
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

void URpgAbilitySystemComponent::NotifyAbilityActivated(const FGameplayAbilitySpecHandle Handle,
                                                        UGameplayAbility* Ability)
{
	Super::NotifyAbilityActivated(Handle, Ability);
	if (URpgGameplayAbility* RpgAbility = Cast<URpgGameplayAbility>(Ability))
	{
		AddAbilityToActivationGroup(RpgAbility->GetActivationGroup(), RpgAbility);
	}
}

void URpgAbilitySystemComponent::NotifyAbilityFailed(const FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability,
	const FGameplayTagContainer& FailureReason)
{
	Super::NotifyAbilityFailed(Handle, Ability, FailureReason);
	
	if (APawn* Avatar = Cast<APawn>(GetAvatarActor()))
	{
		if (!Avatar->IsLocallyControlled() && Ability->IsSupportedForNetworking())
		{
			ClientNotifyAbilityFailed(Ability, FailureReason);
			return;
		}
	}

	HandleAbilityFailed(Ability, FailureReason);
}

void URpgAbilitySystemComponent::NotifyAbilityEnded(FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability,
	bool bWasCancelled)
{
	Super::NotifyAbilityEnded(Handle, Ability, bWasCancelled);
	
	if (URpgGameplayAbility* RpgAbility = Cast<URpgGameplayAbility>(Ability))
	{
		RemoveAbilityFromActivationGroup(RpgAbility->GetActivationGroup(), RpgAbility);
	}
}

void URpgAbilitySystemComponent::ApplyAbilityBlockAndCancelTags(const FGameplayTagContainer& AbilityTags, UGameplayAbility* RequestingAbility, bool bEnableBlockTags, const FGameplayTagContainer& BlockTags, bool bExecuteCancelTags, const FGameplayTagContainer& CancelTags)
{
	FGameplayTagContainer ModifiedBlockTags = BlockTags;
	FGameplayTagContainer ModifiedCancelTags = CancelTags;

	if (TagRelationshipMapping)
	{
		// Use the mapping to expand the ability tags into block and cancel tag
		TagRelationshipMapping->GetAbilityTagsToBlockAndCancel(AbilityTags, &ModifiedBlockTags, &ModifiedCancelTags);
	}

	Super::ApplyAbilityBlockAndCancelTags(AbilityTags, RequestingAbility, bEnableBlockTags, ModifiedBlockTags, bExecuteCancelTags, ModifiedCancelTags);

	//@TODO: Apply any special logic like blocking input or movement
}

void URpgAbilitySystemComponent::HandleChangeAbilityCanBeCanceled(const FGameplayTagContainer& AbilityTags, UGameplayAbility* RequestingAbility, bool bCanBeCanceled)
{
	Super::HandleChangeAbilityCanBeCanceled(AbilityTags, RequestingAbility, bCanBeCanceled);
	//@TODO: Apply any special logic like blocking input or movement

}



void URpgAbilitySystemComponent::GetAdditionalActivationTagRequirements(const FGameplayTagContainer& AbilityTags, FGameplayTagContainer& OutActivationRequired, FGameplayTagContainer& OutActivationBlocked) const
{
	if (TagRelationshipMapping)
	{
		TagRelationshipMapping->GetRequiredAndBlockedActivationTags(AbilityTags, &OutActivationRequired, &OutActivationBlocked);
	}
}

void URpgAbilitySystemComponent::SetTagRelationshipMapping(URpgAbilityTagRelationshipMapping* NewMapping)
{
	TagRelationshipMapping = NewMapping;
}

void URpgAbilitySystemComponent::ClientNotifyAbilityFailed_Implementation(const UGameplayAbility* Ability, const FGameplayTagContainer& FailureReason)
{
	HandleAbilityFailed(Ability, FailureReason);
}

void URpgAbilitySystemComponent::HandleAbilityFailed(const UGameplayAbility* Ability, const FGameplayTagContainer& FailureReason)
{
	if (const URpgGameplayAbility* RpgAbility = Cast<const URpgGameplayAbility>(Ability))
	{
		RpgAbility->OnAbilityFailedToActivate(FailureReason);
	}	
}

bool URpgAbilitySystemComponent::IsActivationGroupBlocked(ERpgAbilityActivationGroup Group) const
{
	bool bBlocked = false;

	switch (Group)
	{
	case ERpgAbilityActivationGroup::Independent:
		// Independent abilities are never blocked.
		bBlocked = false;
		break;

	case ERpgAbilityActivationGroup::Exclusive_Replaceable:
	case ERpgAbilityActivationGroup::Exclusive_Blocking:
		// Exclusive abilities can activate if nothing is blocking.
		bBlocked = (ActivationGroupCounts[static_cast<uint8>(ERpgAbilityActivationGroup::Exclusive_Blocking)] > 0);
		break;

	default:
		checkf(false, TEXT("IsActivationGroupBlocked: Invalid ActivationGroup [%d]\n"), static_cast<uint8>(Group));
		break;
	}

	return bBlocked;
}

void URpgAbilitySystemComponent::AddAbilityToActivationGroup(ERpgAbilityActivationGroup Group, URpgGameplayAbility* RpgAbility)
{
	check(RpgAbility);
	check(ActivationGroupCounts[static_cast<uint8>(Group)] < INT32_MAX);

	ActivationGroupCounts[static_cast<uint8>(Group)]++;

	const bool bReplicateCancelAbility = false;

	switch (Group)
	{
	case ERpgAbilityActivationGroup::Independent:
		// Independent abilities do not cancel any other abilities.
		break;

	case ERpgAbilityActivationGroup::Exclusive_Replaceable:
	case ERpgAbilityActivationGroup::Exclusive_Blocking:
		CancelActivationGroupAbilities(ERpgAbilityActivationGroup::Exclusive_Replaceable, RpgAbility, bReplicateCancelAbility);
		break;

	default:
		checkf(false, TEXT("AddAbilityToActivationGroup: Invalid ActivationGroup [%d]\n"), static_cast<uint8>(Group));
		break;
	}

	const int32 ExclusiveCount = ActivationGroupCounts[static_cast<uint8>(ERpgAbilityActivationGroup::Exclusive_Replaceable)] + ActivationGroupCounts[static_cast<uint8>(ERpgAbilityActivationGroup::Exclusive_Blocking)];
	if (!ensure(ExclusiveCount <= 1))
	{
		UE_LOG(LogRpgAbilitySystem, Error, TEXT("AddAbilityToActivationGroup: Multiple exclusive abilities are running."));
	}
}

void URpgAbilitySystemComponent::RemoveAbilityFromActivationGroup(ERpgAbilityActivationGroup Group, URpgGameplayAbility* RpgAbility)
{
	check(RpgAbility);
	check(ActivationGroupCounts[static_cast<uint8>(Group)] > 0);

	ActivationGroupCounts[static_cast<uint8>(Group)]--;
}

void URpgAbilitySystemComponent::CancelActivationGroupAbilities(ERpgAbilityActivationGroup Group, URpgGameplayAbility* IgnoreRpgAbility, bool bReplicateCancelAbility)
{
	auto ShouldCancelFunc = [this, Group, IgnoreRpgAbility](const URpgGameplayAbility* RpgAbility, FGameplayAbilitySpecHandle Handle)
	{
		return ((RpgAbility->GetActivationGroup() == Group) && (RpgAbility != IgnoreRpgAbility));
	};

	CancelAbilitiesByFunc(ShouldCancelFunc, bReplicateCancelAbility);
}

void URpgAbilitySystemComponent::AddDynamicTagGameplayEffect(const FGameplayTag& Tag)
{
	const TSubclassOf<UGameplayEffect> DynamicTagGE = URpgAssetManager::GetSubclass(URpgGameData::Get().DynamicTagGameplayEffect);
	if (!DynamicTagGE)
	{
		UE_LOG(LogRpgAbilitySystem, Warning, TEXT("AddDynamicTagGameplayEffect: Unable to find DynamicTagGameplayEffect [%s]."), *URpgGameData::Get().DynamicTagGameplayEffect.GetAssetName());
		return;
	}

	const FGameplayEffectSpecHandle SpecHandle = MakeOutgoingSpec(DynamicTagGE, 1.0f, MakeEffectContext());
	FGameplayEffectSpec* Spec = SpecHandle.Data.Get();

	if (!Spec)
	{
		UE_LOG(LogRpgAbilitySystem, Warning, TEXT("AddDynamicTagGameplayEffect: Unable to make outgoing spec for [%s]."), *GetNameSafe(DynamicTagGE));
		return;
	}

	Spec->DynamicGrantedTags.AddTag(Tag);

	ApplyGameplayEffectSpecToSelf(*Spec);
}

void URpgAbilitySystemComponent::RemoveDynamicTagGameplayEffect(const FGameplayTag& Tag)
{
	const TSubclassOf<UGameplayEffect> DynamicTagGE = URpgAssetManager::GetSubclass(URpgGameData::Get().DynamicTagGameplayEffect);
	if (!DynamicTagGE)
	{
		UE_LOG(LogRpgAbilitySystem, Warning, TEXT("RemoveDynamicTagGameplayEffect: Unable to find gameplay effect [%s]."), *URpgGameData::Get().DynamicTagGameplayEffect.GetAssetName());
		return;
	}

	FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(Tag));
	Query.EffectDefinition = DynamicTagGE;

	RemoveActiveEffects(Query);
}

void URpgAbilitySystemComponent::AddTimedLooseGameplayTag(
	const FGameplayTag& Tag,
	float Duration,
	EGameplayTagReplicationState ReplicationState)
{
	if (!Tag.IsValid())
	{
		return;
	}

	RemoveTimedLooseGameplayTag(Tag, ReplicationState);

	if (Duration <= 0.0f)
	{
		return;
	}

	SetLooseGameplayTagCount(Tag, 1, ReplicationState);

	if (UWorld* World = GetWorld())
	{
		FTimerHandle& TimerHandle = TimedLooseTagTimerHandles.FindOrAdd(Tag);
		World->GetTimerManager().SetTimer(
			TimerHandle,
			FTimerDelegate::CreateWeakLambda(this, [this, Tag, ReplicationState]()
			{
				SetLooseGameplayTagCount(Tag, 0, ReplicationState);
				TimedLooseTagTimerHandles.Remove(Tag);
			}),
			Duration,
			false);
	}
}

void URpgAbilitySystemComponent::RemoveTimedLooseGameplayTag(
	const FGameplayTag& Tag,
	EGameplayTagReplicationState ReplicationState)
{
	if (!Tag.IsValid())
	{
		return;
	}

	if (FTimerHandle* TimerHandle = TimedLooseTagTimerHandles.Find(Tag))
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(*TimerHandle);
		}
		TimedLooseTagTimerHandles.Remove(Tag);
	}

	SetLooseGameplayTagCount(Tag, 0, ReplicationState);
}

void URpgAbilitySystemComponent::GetAbilityTargetData(const FGameplayAbilitySpecHandle AbilityHandle, FGameplayAbilityActivationInfo ActivationInfo, FGameplayAbilityTargetDataHandle& OutTargetDataHandle)
{
	TSharedPtr<FAbilityReplicatedDataCache> ReplicatedData = AbilityTargetDataMap.Find(FGameplayAbilitySpecHandleAndPredictionKey(AbilityHandle, ActivationInfo.GetActivationPredictionKey()));
	if (ReplicatedData.IsValid())
	{
		OutTargetDataHandle = ReplicatedData->TargetData;
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

bool URpgAbilitySystemComponent::TryActivateFirstAbilityByTag(FGameplayTag ActivationTag, bool bAllowRemoteActivation)
{
	if (!ActivationTag.IsValid())
	{
		return false;
	}

	for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (AbilitySpecHasActivationTag(Spec, ActivationTag))
		{
			return TryActivateAbility(Spec.Handle, bAllowRemoteActivation);
		}
	}

	return false;
}

bool URpgAbilitySystemComponent::TryActivateFirstAbilityByInputTag(FGameplayTag InputTag, bool bAllowRemoteActivation)
{
	return TryActivateFirstAbilityByTag(InputTag, bAllowRemoteActivation);
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
	
	OwnerPlayerState = Cast<ARpgBasePlayerState>(GetOwner());
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
	SetLooseGameplayTagCount(RpgGameplayTags::State_Blocking, 0);
	SetLooseGameplayTagCount(RpgGameplayTags::State_PerfectBlockWindow, 0);
	SetLooseGameplayTagCount(RpgGameplayTags::State_Staggered, 0);
	SetLooseGameplayTagCount(RpgGameplayTags::State_GuardBroken, 0);
	RemoveTimedLooseGameplayTag(RpgGameplayTags::State_StaggerImmune, EGameplayTagReplicationState::TagAndCountToAll);
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

