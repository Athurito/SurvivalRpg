#include "RpgTaggedActor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgTaggedActor)

ARpgTaggedActor::ARpgTaggedActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void ARpgTaggedActor::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{
	TagContainer.AppendTags(StaticGameplayTags);
}

#if WITH_EDITOR
bool ARpgTaggedActor::CanEditChange(const FProperty* InProperty) const
{
	// Actor name tags would be confused with the gameplay tags this actor exposes.
	if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(AActor, Tags))
	{
		return false;
	}

	return Super::CanEditChange(InProperty);
}
#endif
