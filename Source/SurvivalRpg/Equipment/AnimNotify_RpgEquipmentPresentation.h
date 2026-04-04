#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "RpgEquipmentComponent.h"
#include "AnimNotify_RpgEquipmentPresentation.generated.h"

UCLASS(meta = (DisplayName = "RPG Equipment Presentation"))
class SURVIVALRPG_API UAnimNotify_RpgEquipmentPresentation : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual FString GetNotifyName_Implementation() const override;
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment|Presentation", meta = (AllowPrivateAccess = "true", ToolTip = "Controls how the visible weapon presentation changes at this exact frame. Apply Current State snaps to the currently active weapon set, Holster Visuals hides or stows the currently shown set, and Draw Active Set shows the currently active set after a delay."))
	ERpgEquipmentPresentationNotifyAction Action = ERpgEquipmentPresentationNotifyAction::ApplyCurrentState;
};
