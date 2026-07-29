#include "RpgInventoryContainerComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
#include "RpgInventoryManagerComponent.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"
#include "SurvivalRpg/Interaction/Abilities/RpgGameplayAbility_OpenStorageContainer.h"
#include "SurvivalRpg/Interaction/InteractionQuery.h"
#include "SurvivalRpg/Inventory/RpgDroppedInventoryActor.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgInventoryContainerComponent)

namespace
{
	void FlushContainerReplication(AActor& OwnerActor)
	{
		OwnerActor.FlushNetDormancy();
		OwnerActor.ForceNetUpdate();
	}
}

URpgInventoryContainerComponent::URpgInventoryContainerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	OpenContainerOption.InteractionTag = RpgGameplayTags::Rpg_Interaction_Action_OpenStorage;
	OpenContainerOption.Prompt.ActionText = NSLOCTEXT("RpgInventory", "OpenStorageContainerText", "Open");
	OpenContainerOption.Prompt.TargetText = NSLOCTEXT("RpgInventory", "OpenStorageContainerSubText", "Storage");
	OpenContainerOption.Prompt.InteractionPriority = 50;
	OpenContainerOption.InteractionAbilityToGrant = URpgGameplayAbility_OpenStorageContainer::StaticClass();
}

void URpgInventoryContainerComponent::GatherInteractionOptions(const FInteractionQuery& InteractQuery, FInteractionOptionBuilder& InteractionBuilder)
{
	// Dropped inventories expose their owner-sensitive Collect option through the actor itself.
	// This component still supplies authoritative transfer/access checks after the loot screen opens.
	if (GetOwner() && GetOwner()->IsA<ARpgDroppedInventoryActor>())
	{
		return;
	}
	if (!bAccessible && bHideInteractionWhenInaccessible)
	{
		return;
	}

	FInteractionOption Option = OpenContainerOption;
	Option.InteractionTag = RpgGameplayTags::Rpg_Interaction_Action_OpenStorage;
	Option.TargetRef.TargetActor = GetOwner();
	Option.TargetRef.TargetComponent = Cast<UPrimitiveComponent>(InteractionAnchor.Get());
	Option.TargetRef.WorldLocation = GetInteractionWorldLocation();
	Option.Prompt.InteractionRange = InteractionRadius > 0.0f
		? InteractionRadius
		: Option.Prompt.InteractionRange;
	const bool bSemanticallyAccessible = bAccessible && GetOwner() && InteractQuery.RequestingAvatar.IsValid();
	Option.Availability = bSemanticallyAccessible
		? ERpgInteractionAvailability::Available
		: ERpgInteractionAvailability::Blocked;
	if (!bSemanticallyAccessible)
	{
		Option.Prompt.BlockedReason = NSLOCTEXT("RpgInventory", "StorageUnavailable", "Storage is unavailable");
	}
	InteractionBuilder.AddInteractionOption(Option);
}

void URpgInventoryContainerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, bAccessible);
	DOREPLIFETIME(ThisClass, InteractionRadius);
	DOREPLIFETIME(ThisClass, PersistentContainerId);
	DOREPLIFETIME(ThisClass, TransferPolicy);
}

URpgInventoryManagerComponent* URpgInventoryContainerComponent::GetInventoryManager() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<URpgInventoryManagerComponent>() : nullptr;
}

bool URpgInventoryContainerComponent::CanActorAccess(const AActor* RequestingActor) const
{
	const AActor* OwnerActor = GetOwner();
	if (!bAccessible || OwnerActor == nullptr || RequestingActor == nullptr)
	{
		return false;
	}

	if (InteractionRadius <= 0.0f)
	{
		return true;
	}

	return FVector::DistSquared(GetInteractionWorldLocation(), RequestingActor->GetActorLocation()) <= FMath::Square(InteractionRadius);
}

void URpgInventoryContainerComponent::SetContainerAccessible(bool bNewAccessible)
{
	if (AActor* OwnerActor = GetOwner();
		OwnerActor && OwnerActor->HasAuthority() && bAccessible != bNewAccessible)
	{
		bAccessible = bNewAccessible;
		FlushContainerReplication(*OwnerActor);
	}
}

void URpgInventoryContainerComponent::SetInteractionRadius(float NewInteractionRadius)
{
	if (AActor* OwnerActor = GetOwner(); OwnerActor && OwnerActor->HasAuthority())
	{
		const float SanitizedRadius = FMath::IsFinite(NewInteractionRadius)
			? FMath::Max(0.0f, NewInteractionRadius)
			: 0.0f;
		if (InteractionRadius != SanitizedRadius)
		{
			InteractionRadius = SanitizedRadius;
			FlushContainerReplication(*OwnerActor);
		}
	}
}

void URpgInventoryContainerComponent::SetInteractionAnchor(USceneComponent* NewInteractionAnchor)
{
	if (!NewInteractionAnchor || NewInteractionAnchor->GetOwner() == GetOwner())
	{
		InteractionAnchor = NewInteractionAnchor;
	}
}

FVector URpgInventoryContainerComponent::GetInteractionWorldLocation() const
{
	if (const USceneComponent* Anchor = InteractionAnchor.Get();
		Anchor && Anchor->GetOwner() == GetOwner())
	{
		return Anchor->GetComponentLocation();
	}

	return GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
}

void URpgInventoryContainerComponent::SetTransferPolicy(
	ERpgInventoryContainerTransferPolicy NewTransferPolicy)
{
	if (AActor* OwnerActor = GetOwner(); OwnerActor && OwnerActor->HasAuthority())
	{
		if (static_cast<uint8>(NewTransferPolicy) <=
				static_cast<uint8>(ERpgInventoryContainerTransferPolicy::WithdrawOnly) &&
			TransferPolicy != NewTransferPolicy)
		{
			TransferPolicy = NewTransferPolicy;
			FlushContainerReplication(*OwnerActor);
		}
	}
}

bool URpgInventoryContainerComponent::CanReceiveTransferFrom(
	const URpgInventoryManagerComponent* SourceInventory) const
{
	const URpgInventoryManagerComponent* ManagedInventory = GetInventoryManager();
	return SourceInventory && ManagedInventory &&
		(SourceInventory == ManagedInventory ||
		 TransferPolicy == ERpgInventoryContainerTransferPolicy::Bidirectional);
}

void URpgInventoryContainerComponent::ConfigureAsDeathLootContainer()
{
	bAccessible = false;
	bHideInteractionWhenInaccessible = true;
	TransferPolicy = ERpgInventoryContainerTransferPolicy::WithdrawOnly;
	bAllowCraftingAccess = false;
}

#if WITH_EDITOR
EDataValidationResult URpgInventoryContainerComponent::IsDataValid(
	FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(
		Super::IsDataValid(Context),
		EDataValidationResult::Valid);
	if (!FMath::IsFinite(InteractionRadius) || InteractionRadius < 0.0f)
	{
		Context.AddError(NSLOCTEXT(
			"RpgInventoryContainer",
			"InvalidInteractionRadius",
			"Interaction Radius must be finite and at least zero centimeters."));
		Result = EDataValidationResult::Invalid;
	}
	if (static_cast<uint8>(TransferPolicy) >
		static_cast<uint8>(ERpgInventoryContainerTransferPolicy::WithdrawOnly))
	{
		Context.AddError(NSLOCTEXT(
			"RpgInventoryContainer",
			"InvalidTransferPolicy",
			"Transfer Policy contains an unknown value."));
		Result = EDataValidationResult::Invalid;
	}
	return Result;
}
#endif
