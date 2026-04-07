#include "AnimNotify_RpgWeaponToolPresentation.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "SurvivalRpg/GameplayTags/GameplayTags.h"
#include "RpgWeaponPresentationComponent.h"

namespace
{
	FGameplayTag ResolveEquipGameplayEventTag(ERpgWeaponToolPresentationNotifyAction Action)
	{
		switch (Action)
		{
		case ERpgWeaponToolPresentationNotifyAction::HolsterVisuals:
			return RpgGameplayTags::GameplayEvent_Equip_HolsterVisible;

		case ERpgWeaponToolPresentationNotifyAction::DrawActiveSet:
			return RpgGameplayTags::GameplayEvent_Equip_DrawActiveSet;

		case ERpgWeaponToolPresentationNotifyAction::ApplyCurrentState:
		default:
			return RpgGameplayTags::GameplayEvent_Equip_ApplyCurrentState;
		}
	}
}

FString UAnimNotify_RpgWeaponToolPresentation::GetNotifyName_Implementation() const
{
	switch (Action)
	{
	case ERpgWeaponToolPresentationNotifyAction::HolsterVisuals:
		return TEXT("RPG Weapon Tool Presentation (Holster)");

	case ERpgWeaponToolPresentationNotifyAction::DrawActiveSet:
		return TEXT("RPG Weapon Tool Presentation (Draw)");

	case ERpgWeaponToolPresentationNotifyAction::ApplyCurrentState:
	default:
		return TEXT("RPG Weapon Tool Presentation (Apply)");
	}
}

void UAnimNotify_RpgWeaponToolPresentation::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	APawn* OwningPawn = MeshComp ? Cast<APawn>(MeshComp->GetOwner()) : nullptr;
	if (OwningPawn == nullptr)
	{
		return;
	}

	// Send gameplay event through ASC (for a locally running ability to pick up).
	if (IAbilitySystemInterface* AbilitySystemInterface = Cast<IAbilitySystemInterface>(OwningPawn))
	{
		if (UAbilitySystemComponent* AbilitySystemComponent = AbilitySystemInterface->GetAbilitySystemComponent())
		{
			FGameplayEventData Payload;
			Payload.EventTag = ResolveEquipGameplayEventTag(Action);
			Payload.Instigator = OwningPawn;
			Payload.Target = OwningPawn;
			AbilitySystemComponent->HandleGameplayEvent(Payload.EventTag, &Payload);

			// For the locally controlled pawn the ability's event tasks drive the
			// presentation – no need to do it again here.
			if (OwningPawn->IsLocallyControlled())
			{
				return;
			}
		}
	}

	// For remote pawns (or pawns without an ASC) directly drive the presentation
	// component because no ability event task is listening on this machine.
	if (URpgWeaponPresentationComponent* PresentationComponent = OwningPawn->FindComponentByClass<URpgWeaponPresentationComponent>())
	{
		switch (Action)
		{
		case ERpgWeaponToolPresentationNotifyAction::HolsterVisuals:
			PresentationComponent->ShowHolstered();
			break;

		case ERpgWeaponToolPresentationNotifyAction::DrawActiveSet:
		case ERpgWeaponToolPresentationNotifyAction::ApplyCurrentState:
		default:
			PresentationComponent->SyncToAuthoritativeState();
			break;
		}
	}
}
