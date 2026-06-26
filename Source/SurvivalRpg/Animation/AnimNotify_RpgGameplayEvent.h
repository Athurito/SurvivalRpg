#pragma once

#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"

#include "AnimNotify_RpgGameplayEvent.generated.h"

/**
 * Sends a designer-selected GameplayEvent tag from an animation montage to the owning actor's ASC.
 *
 * Item-use abilities can wait for this event to apply effects exactly at authored montage timing, for example when
 * a potion reaches the character's mouth. The event is cosmetic-data driven, but the ability still executes on server.
 */
UCLASS(meta = (DisplayName = "RPG Gameplay Event"))
class SURVIVALRPG_API UAnimNotify_RpgGameplayEvent : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

protected:
	/** GameplayEvent tag sent to the mesh owner's ASC when this notify fires. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gameplay Event", meta = (Categories = "GameplayEvent"))
	FGameplayTag EventTag;
};
