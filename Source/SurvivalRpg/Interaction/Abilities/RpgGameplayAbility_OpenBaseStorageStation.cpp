#include "RpgGameplayAbility_OpenBaseStorageStation.h"

#include "GameFramework/Pawn.h"
#include "SurvivalRpg/Base/RpgBaseStorageStationComponent.h"
#include "SurvivalRpg/Core/Player/RpgPlayerController.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/UI/RpgUIScreenBlueprintLibrary.h"
#include "SurvivalRpg/UI/RpgUIScreenPayload.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgGameplayAbility_OpenBaseStorageStation)

URpgGameplayAbility_OpenBaseStorageStation::URpgGameplayAbility_OpenBaseStorageStation(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
}

void URpgGameplayAbility_OpenBaseStorageStation::ActivateAbility(
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
	URpgBaseStorageStationComponent* StationComponent = FindStorageStationComponent(TargetActor);
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

	if (!PlayerController || !StationComponent || !PlayerInventory || !StationComponent->CanActorAccess(RequestingActor))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
		return;
	}

	URpgBaseStorageScreenPayload* Payload = NewObject<URpgBaseStorageScreenPayload>(PlayerController);
	Payload->ScreenTag = RpgGameplayTags::UI_Screen_BaseTerminal;
	Payload->PrimaryInventory = PlayerInventory;
	Payload->SecondaryInventory = StationComponent->GetArmoryInventory();
	Payload->ContextActor = TargetActor;
	Payload->ContextComponent = StationComponent;
	Payload->PlayerInventory = PlayerInventory;
	Payload->BaseStorage = StationComponent->GetBaseStorage();
	Payload->ArmoryInventory = StationComponent->GetArmoryInventory();
	Payload->StationComponent = StationComponent;
	Payload->AllowedResources = StationComponent->GetAllowedResourceDefinitions();

	URpgUIScreenBlueprintLibrary::OpenUIScreen(PlayerController, RpgGameplayTags::UI_Screen_BaseTerminal, Payload);

	EndAbility(Handle, ActorInfo, ActivationInfo, false, false);
}

ARpgPlayerController* URpgGameplayAbility_OpenBaseStorageStation::FindPlayerControllerForActor(AActor* Actor)
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

URpgInventoryManagerComponent* URpgGameplayAbility_OpenBaseStorageStation::FindPlayerInventory(ARpgPlayerController* PlayerController)
{
	const ARpgPlayerState* RpgPlayerState = PlayerController ? PlayerController->GetPlayerState<ARpgPlayerState>() : nullptr;
	return RpgPlayerState ? RpgPlayerState->GetInventoryManagerComponent() : nullptr;
}

URpgBaseStorageStationComponent* URpgGameplayAbility_OpenBaseStorageStation::FindStorageStationComponent(AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<URpgBaseStorageStationComponent>() : nullptr;
}
