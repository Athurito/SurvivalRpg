#include "AnimNotify_RpgEquipmentPresentation.h"

#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"

FString UAnimNotify_RpgEquipmentPresentation::GetNotifyName_Implementation() const
{
	switch (Action)
	{
	case ERpgEquipmentPresentationNotifyAction::HolsterVisuals:
		return TEXT("RPG Equipment Presentation (Holster)");

	case ERpgEquipmentPresentationNotifyAction::DrawActiveSet:
		return TEXT("RPG Equipment Presentation (Draw)");

	case ERpgEquipmentPresentationNotifyAction::ApplyCurrentState:
	default:
		return TEXT("RPG Equipment Presentation (Apply)");
	}
}

void UAnimNotify_RpgEquipmentPresentation::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	APawn* OwningPawn = MeshComp ? Cast<APawn>(MeshComp->GetOwner()) : nullptr;
	ARpgPlayerState* PlayerState = OwningPawn ? OwningPawn->GetPlayerState<ARpgPlayerState>() : nullptr;
	URpgEquipmentComponent* EquipmentComponent = PlayerState ? PlayerState->GetEquipmentComponent() : nullptr;
	if (EquipmentComponent == nullptr)
	{
		return;
	}

	EquipmentComponent->ApplyPresentationNotifyAction(Action);
}
