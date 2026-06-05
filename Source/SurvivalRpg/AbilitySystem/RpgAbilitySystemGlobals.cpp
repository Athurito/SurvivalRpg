#include "RpgAbilitySystemGlobals.h"

#include "RpgGameplayEffectContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgAbilitySystemGlobals)

URpgAbilitySystemGlobals::URpgAbilitySystemGlobals(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FGameplayEffectContext* URpgAbilitySystemGlobals::AllocGameplayEffectContext() const
{
	return new FRpgGameplayEffectContext();
}
