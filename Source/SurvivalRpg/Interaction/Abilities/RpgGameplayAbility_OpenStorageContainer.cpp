#include "RpgGameplayAbility_OpenStorageContainer.h"

#include "GameFramework/Pawn.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgInventoryContainerComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/Interaction/InteractionOption.h"
#include "SurvivalRpg/Interaction/InteractionQuery.h"
#include "SurvivalRpg/Interaction/InteractionStatics.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgGameplayAbility_OpenStorageContainer)

URpgGameplayAbility_OpenStorageContainer::URpgGameplayAbility_OpenStorageContainer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

void URpgGameplayAbility_OpenStorageContainer::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	FInteractionOption ValidatedOption;
	FInteractionQuery AuthoritativeQuery;
	FText FailureReason;
	if (!ActorInfo || !UInteractionStatics::ValidateInteractionEventData(
			*ActorInfo,
			TriggerEventData,
			ValidatedOption,
			AuthoritativeQuery,
			FailureReason))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AActor* InteractingActor = ActorInfo->AvatarActor.Get();
	AActor* TargetActor = ValidatedOption.TargetRef.TargetActor.Get();
	ARpgPlayerController* PlayerController = Cast<ARpgPlayerController>(ActorInfo->PlayerController.Get());
	URpgInventoryContainerComponent* ContainerComponent = FindContainerComponent(TargetActor);
	const bool bCanOpen = PlayerController && ContainerComponent &&
		ContainerComponent->GetInventoryManager() && FindPlayerInventory(PlayerController) &&
		ContainerComponent->CanActorAccess(InteractingActor) &&
		CommitAbility(Handle, ActorInfo, ActivationInfo);
	if (!bCanOpen)
	{
		UInteractionStatics::BroadcastInteractionMessage(
			this,
			RpgGameplayTags::Rpg_Interaction_Message_Rejected,
			ValidatedOption,
			InteractingActor,
			false);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	PlayerController->ClientOpenStorageInteraction(TargetActor);
	UInteractionStatics::BroadcastInteractionMessage(
		this,
		RpgGameplayTags::Rpg_Interaction_Message_Ended,
		ValidatedOption,
		InteractingActor,
		true);
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

ARpgPlayerController* URpgGameplayAbility_OpenStorageContainer::FindPlayerControllerForActor(AActor* Actor)
{
	if (Actor == nullptr)
	{
		return nullptr;
	}

	if (ARpgPlayerController* PlayerController = Cast<ARpgPlayerController>(Actor))
	{
		return PlayerController;
	}

	if (const APawn* Pawn = Cast<APawn>(Actor))
	{
		return Cast<ARpgPlayerController>(Pawn->GetController());
	}

	return Actor->GetOwner() ? FindPlayerControllerForActor(Actor->GetOwner()) : nullptr;
}

URpgInventoryManagerComponent* URpgGameplayAbility_OpenStorageContainer::FindPlayerInventory(ARpgPlayerController* PlayerController)
{
	const ARpgPlayerState* RpgPlayerState = PlayerController ? PlayerController->GetPlayerState<ARpgPlayerState>() : nullptr;
	return RpgPlayerState ? RpgPlayerState->GetInventoryManagerComponent() : nullptr;
}

URpgInventoryContainerComponent* URpgGameplayAbility_OpenStorageContainer::FindContainerComponent(AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<URpgInventoryContainerComponent>() : nullptr;
}
