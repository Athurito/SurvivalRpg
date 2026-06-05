#pragma once

#include "Animation/AnimNotifies/AnimNotify.h"

#include "AnimNotify_RpgWeaponAttackWindow.generated.h"

UCLASS(meta = (DisplayName = "RPG Weapon Attack Window Start"))
class SURVIVALRPG_API UAnimNotify_RpgWeaponAttackWindowStart : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;
};

UCLASS(meta = (DisplayName = "RPG Weapon Attack Window End"))
class SURVIVALRPG_API UAnimNotify_RpgWeaponAttackWindowEnd : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;
};
