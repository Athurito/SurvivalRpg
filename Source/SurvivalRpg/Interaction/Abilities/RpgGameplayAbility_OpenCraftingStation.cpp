#include "RpgGameplayAbility_OpenCraftingStation.h"

#include "GameFramework/Pawn.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/Crafting/RpgCraftingStationComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/UI/RpgUIScreenBlueprintLibrary.h"
#include "SurvivalRpg/UI/RpgUIScreenPayload.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgGameplayAbility_OpenCraftingStation)

URpgGameplayAbility_OpenCraftingStation::URpgGameplayAbility_OpenCraftingStation(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
}

void URpgGameplayAbility_OpenCraftingStation::ActivateAbility(
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
	URpgCraftingStationComponent* CraftingStation = FindCraftingStationComponent(TargetActor);
	URpgInventoryManagerComponent* PlayerInventory = FindPlayerInventory(PlayerController);
	URpgInventoryManagerComponent* OutputInventory = CraftingStation ? CraftingStation->GetOutputInventory() : nullptr;

	APawn* RequestingPawn = nullptr;
	if (PlayerController)
	{
		RequestingPawn = PlayerController->GetPawn();
	}
	else
	{
		RequestingPawn = Cast<APawn>(InteractingActor);
	}
	AActor* RequestingActor = RequestingPawn ? static_cast<AActor*>(RequestingPawn) : InteractingActor;

	if (!PlayerController || !CraftingStation || !PlayerInventory || !CraftingStation->CanActorAccess(RequestingActor))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
		return;
	}

	URpgCraftingStationScreenPayload* Payload = NewObject<URpgCraftingStationScreenPayload>(PlayerController);
	Payload->ScreenTag = RpgGameplayTags::UI_Screen_Crafting;
	Payload->PrimaryInventory = PlayerInventory;
	Payload->SecondaryInventory = OutputInventory;
	Payload->ContextActor = TargetActor;
	Payload->ContextComponent = CraftingStation;
	Payload->PlayerInventory = PlayerInventory;
	Payload->CraftingStation = CraftingStation;
	Payload->OutputInventory = OutputInventory;
	Payload->RequestingActor = RequestingActor;

	URpgUIScreenBlueprintLibrary::OpenUIScreen(PlayerController, RpgGameplayTags::UI_Screen_Crafting, Payload);

	EndAbility(Handle, ActorInfo, ActivationInfo, false, false);
}

ARpgPlayerController* URpgGameplayAbility_OpenCraftingStation::FindPlayerControllerForActor(AActor* Actor)
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

URpgInventoryManagerComponent* URpgGameplayAbility_OpenCraftingStation::FindPlayerInventory(ARpgPlayerController* PlayerController)
{
	const ARpgPlayerState* RpgPlayerState = PlayerController ? PlayerController->GetPlayerState<ARpgPlayerState>() : nullptr;
	return RpgPlayerState ? RpgPlayerState->GetInventoryManagerComponent() : nullptr;
}

URpgCraftingStationComponent* URpgGameplayAbility_OpenCraftingStation::FindCraftingStationComponent(AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<URpgCraftingStationComponent>() : nullptr;
}
