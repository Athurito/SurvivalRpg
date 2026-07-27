// Copyright Epic Games, Inc. All Rights Reserved.

#include "RpgGameplayAbility_Interact.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Interaction/InteractionQuery.h"
#include "SurvivalRpg/Interaction/InteractionStatics.h"
#include "SurvivalRpg/Interaction/Tasks/AbilityTask_GrantNearbyInteraction.h"
#include "SurvivalRpg/Interaction/Tasks/AbilityTask_WaitForInteractableTargets_FocusSweep.h"
#include "SurvivalRpg/Interaction/Tasks/AbilityTask_WaitForInteractableTargets_Nearby.h"
#include "SurvivalRpg/Physics/RpgCollisionChannels.h"
#include "SurvivalRpg/UI/IndicatorSystem/IndicatorDescriptor.h"
#include "SurvivalRpg/UI/IndicatorSystem/RpgIndicatorManagerComponent.h"
#include "SurvivalRpg/UI/Interaction/RpgInteractionPromptData.h"
#include "SurvivalRpg/UI/Interaction/RpgInteractionPromptWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgGameplayAbility_Interact)

URpgGameplayAbility_Interact::URpgGameplayAbility_Interact(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationPolicy = ERpgAbilityActivationPolicy::OnSpawn;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	DefaultInteractionWidgetClass = URpgInteractionPromptWidget::StaticClass();
	DefaultNearbyWidgetClass = URpgInteractionPromptWidget::StaticClass();
}

void URpgGameplayAbility_Interact::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// Native orchestration is intentional: legacy GA_Interaction Blueprint graphs must not start a
	// second trace/input task chain while the asset is being migrated to an empty presentation shell.
	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AActor* AvatarActor = ActorInfo->AvatarActor.Get();
	FInteractionQuery Query;
	Query.RequestingAvatar = AvatarActor;
	Query.RequestingController = ActorInfo->PlayerController.Get();
	Query.QueryOrigin = AvatarActor->GetActorLocation();

	FGameplayAbilityTargetingLocationInfo StartLocation;
	StartLocation.LocationType = EGameplayAbilityTargetingLocationType::ActorTransform;
	StartLocation.SourceActor = AvatarActor;
	StartLocation.SourceAbility = this;

	FocusTask = UAbilityTask_WaitForInteractableTargets_FocusSweep::WaitForInteractableTargets_FocusSweep(
		this,
		Query,
		StartLocation,
		Rpg_TraceChannel_Interaction,
		InteractionScanRange,
		InteractionScanRate,
		FocusSweepRadius,
		MaxFocusCandidates,
		false);
	FocusTask->InteractableObjectsChanged.AddDynamic(this, &ThisClass::HandleFocusedOptionsChanged);
	FocusTask->ReadyForActivation();

	if (ActorInfo->IsLocallyControlled())
	{
		NearbyTask = UAbilityTask_WaitForInteractableTargets_Nearby::WaitForInteractableTargets_Nearby(
			this,
			Query,
			Rpg_TraceChannel_Interaction,
			AwarenessScanRange,
			NearbyScanRate,
			MaxNearbyIndicators,
			false);
		NearbyTask->InteractableObjectsChanged.AddDynamic(this, &ThisClass::HandleNearbyOptionsChanged);
		NearbyTask->ReadyForActivation();
	}

	if (ActorInfo->IsNetAuthority())
	{
		GrantTask = UAbilityTask_GrantNearbyInteraction::GrantAbilitiesForNearbyInteractors(
			this,
			AwarenessScanRange,
			NearbyScanRate);
		GrantTask->ReadyForActivation();
	}

	StartWaitingForInput();
}

void URpgGameplayAbility_Interact::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	ClearInteractionIndicators();
	FocusTask = nullptr;
	NearbyTask = nullptr;
	GrantTask = nullptr;
	InputPressTask = nullptr;
	CurrentOptions.Reset();
	CurrentNearbyOptions.Reset();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void URpgGameplayAbility_Interact::UpdateInteractions(const TArray<FInteractionOption>& InteractiveOptions)
{
	// Existing GA_Interaction assets may still contain Lyra's legacy trace task for one migration cycle.
	// Once the native focus task owns orchestration, ignore those duplicate presentation callbacks.
	if (FocusTask)
	{
		return;
	}
	HandleFocusedOptionsChanged(InteractiveOptions);
}

void URpgGameplayAbility_Interact::HandleFocusedOptionsChanged(const TArray<FInteractionOption>& InteractiveOptions)
{
	const FString PreviousKey = CurrentOptions.IsEmpty() ? FString() : MakeOptionKey(CurrentOptions[0]);
	const ERpgInteractionPromptState PreviousState = CurrentOptions.IsEmpty()
		? ERpgInteractionPromptState::Hidden
		: CurrentOptions[0].PromptState;
	CurrentOptions = InteractiveOptions;
	if (CurrentOptions.Num() > 1)
	{
		CurrentOptions.SetNum(1);
	}

	const bool bHasFocus = !CurrentOptions.IsEmpty();
	const FString NewKey = bHasFocus ? MakeOptionKey(CurrentOptions[0]) : FString();
	const ERpgInteractionPromptState NewState = bHasFocus
		? CurrentOptions[0].PromptState
		: ERpgInteractionPromptState::Hidden;

	if (PreviousKey != NewKey)
	{
		FInteractionOption EmptyOption;
		OnFocusedOptionChanged.Broadcast(bHasFocus, bHasFocus ? CurrentOptions[0] : EmptyOption);
	}
	if (PreviousState != NewState || LastPromptState != NewState)
	{
		FInteractionOption EmptyOption;
		OnPromptStateChanged.Broadcast(NewState, bHasFocus ? CurrentOptions[0] : EmptyOption);
		LastPromptState = NewState;
	}
	RefreshInteractionIndicators();
}

void URpgGameplayAbility_Interact::HandleNearbyOptionsChanged(const TArray<FInteractionOption>& InteractiveOptions)
{
	CurrentNearbyOptions = InteractiveOptions;
	OnNearbyOptionsChanged.Broadcast(CurrentNearbyOptions);
	RefreshInteractionIndicators();
}

void URpgGameplayAbility_Interact::StartWaitingForInput()
{
	if (!IsActive())
	{
		return;
	}
	InputPressTask = UAbilityTask_WaitInputPress::WaitInputPress(this, false);
	InputPressTask->OnPress.AddDynamic(this, &ThisClass::HandleInputPressed);
	InputPressTask->ReadyForActivation();
}

void URpgGameplayAbility_Interact::HandleInputPressed(float TimeWaited)
{
	(void)TimeWaited;
	TriggerInteraction();
	StartWaitingForInput();
}

void URpgGameplayAbility_Interact::TriggerInteraction()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (!ActorInfo || !ActorInfo->IsNetAuthority() || !FocusTask)
	{
		return;
	}
	if (const UWorld* World = GetWorld())
	{
		const float Now = World->GetTimeSeconds();
		if (LastAuthorityTriggerTimeSeconds >= 0.0f && Now - LastAuthorityTriggerTimeSeconds < 0.02f)
		{
			return;
		}
		LastAuthorityTriggerTimeSeconds = Now;
	}

	FocusTask->ScanNow();
	FInteractionOption FocusedOption;
	if (!FocusTask->GetFocusedOption(FocusedOption) || FocusedOption.PromptState != ERpgInteractionPromptState::Ready)
	{
		return;
	}
	TriggerValidatedInteraction(FocusedOption);
}

bool URpgGameplayAbility_Interact::TriggerValidatedInteraction(const FInteractionOption& FocusedOption)
{
	UAbilitySystemComponent* InstigatorAbilitySystem = GetAbilitySystemComponentFromActorInfo();
	AActor* Instigator = GetAvatarActorFromActorInfo();
	if (!InstigatorAbilitySystem || !Instigator || !FocusedOption.TargetAbilitySystem || !FocusedOption.TargetInteractionAbilityHandle.IsValid())
	{
		return false;
	}

	FGameplayEventData Payload;
	if (!UInteractionStatics::BuildInteractionEventData(FocusedOption, Instigator, InstigatorAbilitySystem, Payload))
	{
		return false;
	}

	FInteractionOption ValidatedOption;
	FInteractionQuery AuthoritativeQuery;
	FText FailureReason;
	if (!UInteractionStatics::ValidateInteractionEventData(
			*GetCurrentActorInfo(),
			&Payload,
			ValidatedOption,
			AuthoritativeQuery,
			FailureReason))
	{
		UInteractionStatics::BroadcastInteractionMessage(
			this,
			RpgGameplayTags::Rpg_Interaction_Message_Rejected,
			FocusedOption,
			Instigator,
			false);
		return false;
	}

	UAbilitySystemComponent* ValidatedTargetAbilitySystem = ValidatedOption.TargetAbilitySystem;
	FGameplayAbilitySpec* TargetSpec = ValidatedTargetAbilitySystem
		? ValidatedTargetAbilitySystem->FindAbilitySpecFromHandle(ValidatedOption.TargetInteractionAbilityHandle)
		: nullptr;
	FGameplayAbilityActorInfo* TargetActorInfo = ValidatedTargetAbilitySystem
		? ValidatedTargetAbilitySystem->AbilityActorInfo.Get()
		: nullptr;
	if (!TargetSpec || !TargetSpec->Ability || !TargetActorInfo ||
		!TargetSpec->Ability->CanActivateAbility(TargetSpec->Handle, TargetActorInfo))
	{
		UInteractionStatics::BroadcastInteractionMessage(
			this,
			RpgGameplayTags::Rpg_Interaction_Message_Rejected,
			ValidatedOption,
			Instigator,
			false);
		return false;
	}

	FGameplayEventData ValidatedPayload;
	if (!UInteractionStatics::BuildInteractionEventData(
			ValidatedOption,
			Instigator,
			InstigatorAbilitySystem,
			ValidatedPayload))
	{
		UInteractionStatics::BroadcastInteractionMessage(
			this,
			RpgGameplayTags::Rpg_Interaction_Message_Rejected,
			ValidatedOption,
			Instigator,
			false);
		return false;
	}

	UInteractionStatics::BroadcastInteractionMessage(
		this,
		RpgGameplayTags::Rpg_Interaction_Message_Started,
		ValidatedOption,
		Instigator,
		true);

	const bool bTriggered = ValidatedTargetAbilitySystem->TriggerAbilityFromGameplayEvent(
		ValidatedOption.TargetInteractionAbilityHandle,
		TargetActorInfo,
		RpgGameplayTags::Ability_Interaction_Activate,
		&ValidatedPayload,
		*ValidatedTargetAbilitySystem);
	if (!bTriggered)
	{
		UInteractionStatics::BroadcastInteractionMessage(
			this,
			RpgGameplayTags::Rpg_Interaction_Message_Rejected,
			ValidatedOption,
			Instigator,
			false);
	}
	return bTriggered;
}

FString URpgGameplayAbility_Interact::MakeOptionKey(const FInteractionOption& Option)
{
	return UInteractionStatics::MakeStableOptionKey(Option);
}

void URpgGameplayAbility_Interact::RefreshInteractionIndicators()
{
	ARpgPlayerController* PlayerController = GetRpgPlayerControllerFromActorInfo();
	URpgIndicatorManagerComponent* IndicatorManager = PlayerController
		? URpgIndicatorManagerComponent::GetComponent(PlayerController)
		: nullptr;
	if (!IndicatorManager || !GetCurrentActorInfo() || !GetCurrentActorInfo()->IsLocallyControlled())
	{
		return;
	}
	ReconcileInteractionIndicators(IndicatorManager);
}

void URpgGameplayAbility_Interact::ReconcileInteractionIndicators(URpgIndicatorManagerComponent* IndicatorManager)
{
	check(IndicatorManager);
	const bool bShowFocus = !CurrentOptions.IsEmpty() && CurrentOptions[0].PromptState != ERpgInteractionPromptState::Hidden;
	if (bShowFocus)
	{
		const FInteractionOption& Option = CurrentOptions[0];
		const TSoftClassPtr<UUserWidget> DesiredWidgetClass = Option.Prompt.FocusWidgetClass.IsNull()
			? DefaultInteractionWidgetClass
			: Option.Prompt.FocusWidgetClass;
		USceneComponent* Anchor = Option.TargetRef.TargetComponent.Get();
		if (!Anchor && Option.TargetRef.TargetActor.IsValid())
		{
			Anchor = Option.TargetRef.TargetActor->GetRootComponent();
		}

		if (!FocusPromptData)
		{
			FocusPromptData = NewObject<URpgInteractionPromptData>(this);
		}
		FocusPromptData->UpdateFromOption(Option, Option.PromptState);

		// The actor canvas resolves the widget class synchronously from AddIndicator. Recreate only
		// when the authored class changes, and fully configure the replacement before registration.
		if (FocusIndicator && FocusIndicator->GetIndicatorClass() != DesiredWidgetClass)
		{
			IndicatorManager->RemoveIndicator(FocusIndicator);
			FocusIndicator = nullptr;
		}

		bool bRegisterIndicator = false;
		if (!FocusIndicator)
		{
			FocusIndicator = NewObject<UIndicatorDescriptor>(this);
			FocusIndicator->SetDataObject(FocusPromptData);
			// This ability owns the descriptor lifetime. Keeping it registered across a temporary
			// focus loss avoids widget-pool churn and lets range re-entry reuse the same prompt.
			FocusIndicator->SetAutoRemoveWhenIndicatorComponentIsNull(false);
			bRegisterIndicator = true;
		}
		FocusIndicator->SetSceneComponent(Anchor);
		FocusIndicator->SetAbsoluteWorldPosition(Option.GetInteractionWorldLocation());
		FocusIndicator->SetIndicatorClass(DesiredWidgetClass);
		FocusIndicator->SetPriority(Option.Prompt.InteractionPriority);
		FocusIndicator->SetDesiredVisibility(true);
		if (bRegisterIndicator)
		{
			IndicatorManager->AddIndicator(FocusIndicator);
		}
	}
	else if (FocusIndicator)
	{
		FocusIndicator->SetDesiredVisibility(false);
		if (FocusPromptData)
		{
			FocusPromptData->Clear();
		}
	}

	const FString FocusKey = bShowFocus ? MakeOptionKey(CurrentOptions[0]) : FString();
	TSet<FString> DesiredNearbyKeys;
	for (const FInteractionOption& Option : CurrentNearbyOptions)
	{
		const FString Key = MakeOptionKey(Option);
		if (Key == FocusKey || Option.PromptState == ERpgInteractionPromptState::Hidden)
		{
			continue;
		}
		DesiredNearbyKeys.Add(Key);
		URpgInteractionPromptData* Data = NearbyPromptData.FindRef(Key);
		if (!Data)
		{
			Data = NewObject<URpgInteractionPromptData>(this);
			NearbyPromptData.Add(Key, Data);
		}
		Data->UpdateFromOption(Option, ERpgInteractionPromptState::Nearby);

		const TSoftClassPtr<UUserWidget> DesiredWidgetClass = Option.Prompt.NearbyWidgetClass.IsNull()
			? DefaultNearbyWidgetClass
			: Option.Prompt.NearbyWidgetClass;
		UIndicatorDescriptor* Descriptor = NearbyIndicators.FindRef(Key);
		if (Descriptor && Descriptor->GetIndicatorClass() != DesiredWidgetClass)
		{
			IndicatorManager->RemoveIndicator(Descriptor);
			NearbyIndicators.Remove(Key);
			Descriptor = nullptr;
		}

		bool bRegisterIndicator = false;
		if (!Descriptor)
		{
			Descriptor = NewObject<UIndicatorDescriptor>(this);
			Descriptor->SetDataObject(Data);
			Descriptor->SetAutoRemoveWhenIndicatorComponentIsNull(false);
			NearbyIndicators.Add(Key, Descriptor);
			bRegisterIndicator = true;
		}
		USceneComponent* Anchor = Option.TargetRef.TargetComponent.Get();
		if (!Anchor && Option.TargetRef.TargetActor.IsValid())
		{
			Anchor = Option.TargetRef.TargetActor->GetRootComponent();
		}
		Descriptor->SetSceneComponent(Anchor);
		Descriptor->SetAbsoluteWorldPosition(Option.GetInteractionWorldLocation());
		Descriptor->SetIndicatorClass(DesiredWidgetClass);
		Descriptor->SetPriority(Option.Prompt.InteractionPriority);
		Descriptor->SetDesiredVisibility(true);
		if (bRegisterIndicator)
		{
			IndicatorManager->AddIndicator(Descriptor);
		}
	}

	for (auto It = NearbyIndicators.CreateIterator(); It; ++It)
	{
		if (!DesiredNearbyKeys.Contains(It.Key()))
		{
			IndicatorManager->RemoveIndicator(It.Value());
			NearbyPromptData.Remove(It.Key());
			It.RemoveCurrent();
		}
	}
}

void URpgGameplayAbility_Interact::ClearInteractionIndicators()
{
	ARpgPlayerController* PlayerController = GetRpgPlayerControllerFromActorInfo();
	if (URpgIndicatorManagerComponent* IndicatorManager = PlayerController
		? URpgIndicatorManagerComponent::GetComponent(PlayerController)
		: nullptr)
	{
		if (FocusIndicator)
		{
			IndicatorManager->RemoveIndicator(FocusIndicator);
		}
		for (const TPair<FString, TObjectPtr<UIndicatorDescriptor>>& Entry : NearbyIndicators)
		{
			IndicatorManager->RemoveIndicator(Entry.Value);
		}
	}
	FocusIndicator = nullptr;
	FocusPromptData = nullptr;
	NearbyIndicators.Reset();
	NearbyPromptData.Reset();
}
