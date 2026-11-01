#include "AnimNotify_RpgGameplayEvent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameplayEffectTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNotify_RpgGameplayEvent)

UAnimNotify_RpgGameplayEvent::UAnimNotify_RpgGameplayEvent()
{
#if WITH_EDITORONLY_DATA
	// Animation editor previews skip it; PIE and games still fire. Notify enforces it for branching points too.
	bShouldFireInEditor = false;
#endif
}

void UAnimNotify_RpgGameplayEvent::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

#if WITH_EDITOR
	// The engine checks ShouldFireInEditor only for queued notifies; a branching point reaches Notify in previews too.
	const UWorld* World = MeshComp ? MeshComp->GetWorld() : nullptr;
	if (World && World->WorldType == EWorldType::EditorPreview && !ShouldFireInEditor())
	{
		return;
	}
#endif

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

FString UAnimNotify_RpgGameplayEvent::GetNotifyName_Implementation() const
{
	return EventTag.IsValid() ? EventTag.ToString() : TEXT("RPG Gameplay Event");
}
