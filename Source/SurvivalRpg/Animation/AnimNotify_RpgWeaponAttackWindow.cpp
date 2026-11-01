#include "AnimNotify_RpgWeaponAttackWindow.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameplayEffectTypes.h"
#include "SurvivalRpg/GameplayTags/RpgGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNotify_RpgWeaponAttackWindow)

namespace
{
	void SendWeaponAttackWindowEvent(USkeletalMeshComponent* MeshComp, FGameplayTag EventTag)
	{
		AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
		if (!Owner || !EventTag.IsValid())
		{
			return;
		}

		FGameplayEventData Payload;
		Payload.EventTag = EventTag;
		Payload.Instigator = Owner;
		Payload.Target = Owner;

		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Owner, EventTag, Payload);
	}
}

UAnimNotify_RpgWeaponAttackWindowStart::UAnimNotify_RpgWeaponAttackWindowStart()
{
#if WITH_EDITORONLY_DATA
	// The engine skips this only for animation editor meshes (UDebugSkelMeshComponent); PIE and games still fire.
	bShouldFireInEditor = false;
#endif
}

void UAnimNotify_RpgWeaponAttackWindowStart::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	SendWeaponAttackWindowEvent(MeshComp, RpgGameplayTags::GameplayEvent_Weapon_Attack_Window_Start);
}

FString UAnimNotify_RpgWeaponAttackWindowStart::GetNotifyName_Implementation() const
{
	return TEXT("Attack Window Start");
}

UAnimNotify_RpgWeaponAttackWindowEnd::UAnimNotify_RpgWeaponAttackWindowEnd()
{
#if WITH_EDITORONLY_DATA
	bShouldFireInEditor = false;
#endif
}

void UAnimNotify_RpgWeaponAttackWindowEnd::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	SendWeaponAttackWindowEvent(MeshComp, RpgGameplayTags::GameplayEvent_Weapon_Attack_Window_End);
}

FString UAnimNotify_RpgWeaponAttackWindowEnd::GetNotifyName_Implementation() const
{
	return TEXT("Attack Window End");
}
