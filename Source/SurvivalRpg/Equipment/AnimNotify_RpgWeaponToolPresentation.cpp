#include "AnimNotify_RpgWeaponToolPresentation.h"

#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "RpgWeaponPresentationComponent.h"

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
	URpgWeaponPresentationComponent* PresentationComponent = OwningPawn ? OwningPawn->FindComponentByClass<URpgWeaponPresentationComponent>() : nullptr;
	if (PresentationComponent == nullptr)
	{
		return;
	}

	PresentationComponent->ApplyWeaponToolPresentationNotifyAction(Action);
}
