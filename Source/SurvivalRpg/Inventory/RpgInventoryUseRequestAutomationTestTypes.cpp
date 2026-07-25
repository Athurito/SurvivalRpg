#include "RpgInventoryUseRequestAutomationTestTypes.h"

#include "RpgInventoryFragment_ItemTraits.h"
#include "RpgInventoryItemInstance.h"
#include "RpgInventoryManagerComponent.h"
#include "RpgInventoryUiActionComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryUseRequestAutomationTestTypes)

namespace RpgInventoryUseRequestAutomationTestTypes
{
	struct FReentrantUseState
	{
		TWeakObjectPtr<URpgInventoryUiActionComponent> ActionComponent;
		TWeakObjectPtr<URpgInventoryManagerComponent> Inventory;
		FRpgInventoryUseRequest Request;
		int32 ActivationCount = 0;
		int32 StackCountBeforeReentrantRetry = INDEX_NONE;
		int32 StackCountAfterReentrantRetry = INDEX_NONE;
		float ObservedEventMagnitude = 0.0f;
		bool bIssuedReentrantRetry = false;
	};

	FReentrantUseState GReentrantUseState;
}

URpgInventoryUseRequestAutomationAbility::
	URpgInventoryUseRequestAutomationAbility(
		const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

void URpgInventoryUseRequestAutomationAbility::ConfigureReentrantRequest(
	URpgInventoryUiActionComponent* InActionComponent,
	URpgInventoryManagerComponent* InInventory,
	const FRpgInventoryUseRequest& InRequest)
{
	using namespace RpgInventoryUseRequestAutomationTestTypes;
	GReentrantUseState = FReentrantUseState();
	GReentrantUseState.ActionComponent = InActionComponent;
	GReentrantUseState.Inventory = InInventory;
	GReentrantUseState.Request = InRequest;
}

void URpgInventoryUseRequestAutomationAbility::ResetTestState()
{
	RpgInventoryUseRequestAutomationTestTypes::GReentrantUseState =
		RpgInventoryUseRequestAutomationTestTypes::FReentrantUseState();
}

int32 URpgInventoryUseRequestAutomationAbility::GetActivationCount()
{
	return RpgInventoryUseRequestAutomationTestTypes::
		GReentrantUseState.ActivationCount;
}

bool URpgInventoryUseRequestAutomationAbility::DidIssueReentrantRetry()
{
	return RpgInventoryUseRequestAutomationTestTypes::
		GReentrantUseState.bIssuedReentrantRetry;
}

int32 URpgInventoryUseRequestAutomationAbility::
	GetStackCountBeforeReentrantRetry()
{
	return RpgInventoryUseRequestAutomationTestTypes::
		GReentrantUseState.StackCountBeforeReentrantRetry;
}

int32 URpgInventoryUseRequestAutomationAbility::
	GetStackCountAfterReentrantRetry()
{
	return RpgInventoryUseRequestAutomationTestTypes::
		GReentrantUseState.StackCountAfterReentrantRetry;
}

float URpgInventoryUseRequestAutomationAbility::GetObservedEventMagnitude()
{
	return RpgInventoryUseRequestAutomationTestTypes::
		GReentrantUseState.ObservedEventMagnitude;
}

void URpgInventoryUseRequestAutomationAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		TriggerEventData);

	using namespace RpgInventoryUseRequestAutomationTestTypes;
	FReentrantUseState& State = GReentrantUseState;
	++State.ActivationCount;
	State.ObservedEventMagnitude = TriggerEventData
		? TriggerEventData->EventMagnitude
		: 0.0f;

	if (!State.bIssuedReentrantRetry)
	{
		State.bIssuedReentrantRetry = true;
		URpgInventoryManagerComponent* Inventory = State.Inventory.Get();
		URpgInventoryUiActionComponent* ActionComponent =
			State.ActionComponent.Get();
		URpgInventoryItemInstance* Item = Inventory
			? Inventory->FindItemById(State.Request.ItemId)
			: nullptr;
		State.StackCountBeforeReentrantRetry = Inventory && Item
			? Inventory->GetItemStackCount(Item)
			: INDEX_NONE;

		if (ActionComponent)
		{
			ActionComponent->RequestUseInventoryItemById(
				Inventory,
				State.Request);
		}

		Item = Inventory
			? Inventory->FindItemById(State.Request.ItemId)
			: nullptr;
		State.StackCountAfterReentrantRetry = Inventory && Item
			? Inventory->GetItemStackCount(Item)
			: INDEX_NONE;
	}

	EndAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		false,
		false);
}

URpgInventoryUseRequestAutomationItemDefinition::
	URpgInventoryUseRequestAutomationItemDefinition(
		const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = FText::FromString(
		TEXT("Automation Exactly-Once Consumable"));

	URpgInventoryFragment_SpatialItem* SpatialFragment =
		CreateDefaultSubobject<URpgInventoryFragment_SpatialItem>(
			TEXT("Spatial"));
	SpatialFragment->Footprint.Width = 1;
	SpatialFragment->Footprint.Height = 1;
	SpatialFragment->bAllowRotation = true;
	Fragments.Add(SpatialFragment);

	URpgInventoryFragment_ItemTraits* TraitsFragment =
		CreateDefaultSubobject<URpgInventoryFragment_ItemTraits>(
			TEXT("Traits"));
	TraitsFragment->ItemCategory =
		ERpgInventoryItemCategory::Consumable;
	TraitsFragment->bCanStack = true;
	TraitsFragment->MaxStackSize = 10;
	Fragments.Add(TraitsFragment);

	URpgInventoryFragment_UsableItem* UsableFragment =
		CreateDefaultSubobject<URpgInventoryFragment_UsableItem>(
			TEXT("Usable"));
	UsableFragment->UseAbility =
		URpgInventoryUseRequestAutomationAbility::StaticClass();
	UsableFragment->ConsumeCount = 1;
	UsableFragment->bConsumeOnActivationAccepted = true;
	UsableFragment->bOnlyFromPlayerInventory = true;
	Fragments.Add(UsableFragment);
}
