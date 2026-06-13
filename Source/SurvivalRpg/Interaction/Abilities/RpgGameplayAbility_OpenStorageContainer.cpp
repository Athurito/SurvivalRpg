#include "RpgGameplayAbility_OpenStorageContainer.h"

#include "GameFramework/Pawn.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgInventoryContainerComponent.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/UI/RpgUIScreenBlueprintLibrary.h"
#include "SurvivalRpg/UI/RpgUIScreenPayload.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgGameplayAbility_OpenStorageContainer)

URpgGameplayAbility_OpenStorageContainer::URpgGameplayAbility_OpenStorageContainer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
}

void URpgGameplayAbility_OpenStorageContainer::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	AActor* InteractingActor = TriggerEventData ? const_cast<AActor*>(ToRawPtr(TriggerEventData->Instigator)) : nullptr;
	if (InteractingActor == nullptr && ActorInfo && ActorInfo->AvatarActor.IsValid())
	{
		InteractingActor = ActorInfo->AvatarActor.Get();
	}

	ARpgPlayerController* PlayerController = FindPlayerControllerForActor(InteractingActor);
	if (PlayerController == nullptr)
	{
		PlayerController = Cast<ARpgPlayerController>(GetControllerFromActorInfo());
	}

	AActor* TargetActor = TriggerEventData ? const_cast<AActor*>(ToRawPtr(TriggerEventData->Target)) : nullptr;
	URpgInventoryContainerComponent* ContainerComponent = FindContainerComponent(TargetActor);
	URpgInventoryManagerComponent* ContainerInventory = ContainerComponent ? ContainerComponent->GetInventoryManager() : nullptr;
	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory(PlayerController);

	const APawn* RequestingPawn = nullptr;
	if (PlayerController)
	{
		RequestingPawn = PlayerController->GetPawn();
	}
	else
	{
		RequestingPawn = Cast<APawn>(InteractingActor);
	}
	const AActor* RequestingActor = RequestingPawn ? static_cast<const AActor*>(RequestingPawn) : InteractingActor;

	if (!PlayerController || !ContainerComponent || !ContainerInventory || !PlayerInventory || !ContainerComponent->CanActorAccess(RequestingActor))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
		return;
	}

	URpgInventoryScreenPayload* Payload = NewObject<URpgInventoryScreenPayload>(PlayerController);
	Payload->ScreenTag = RpgGameplayTags::UI_Screen_Storage;
	Payload->PrimaryInventory = PlayerInventory;
	Payload->SecondaryInventory = ContainerInventory;
	Payload->ContextActor = TargetActor;
	Payload->ContextComponent = ContainerComponent;

	URpgUIScreenBlueprintLibrary::OpenUIScreen(PlayerController, RpgGameplayTags::UI_Screen_Storage, Payload);

	EndAbility(Handle, ActorInfo, ActivationInfo, false, false);
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
