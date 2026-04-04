#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "RpgEquipmentComponent.h"
#include "AnimNotify_RpgWeaponToolPresentation.generated.h"

UCLASS(meta = (DisplayName = "RPG Weapon Tool Presentation"))
class SURVIVALRPG_API UAnimNotify_RpgWeaponToolPresentation : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual FString GetNotifyName_Implementation() const override;
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Tool|Presentation", meta = (AllowPrivateAccess = "true", ToolTip = "Controls how the visible weapon or tool presentation changes at this exact frame. Apply Current State snaps to the currently active weapon-tool state, Holster Visuals hides or stows the currently shown item, and Draw Active Set shows the currently active weapon-tool set after a delay."))
	ERpgWeaponToolPresentationNotifyAction Action = ERpgWeaponToolPresentationNotifyAction::ApplyCurrentState;
};
