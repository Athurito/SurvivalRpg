#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpec.h"
#include "GameplayTagContainer.h"

#include "RpgAbilityBindingResolver.generated.h"

class URpgAbilitySystemComponent;
class URpgGameplayAbility;

/** Result of resolving a semantic ability id to one concrete granted GAS spec. */
UENUM(BlueprintType)
enum class ERpgAbilityBindingResolveResult : uint8
{
	/** The supplied ability id is not valid. */
	InvalidAbilityId,

	/** No currently granted ability spec owns the requested ability id. */
	Missing,

	/** Exactly one granted ability spec owns the requested ability id. */
	Unique,

	/** More than one granted spec owns the id; activation is blocked as a content configuration error. */
	Ambiguous
};

/** Non-reflected resolution payload used to bind and activate exactly one granted ability spec. */
struct SURVIVALRPG_API FRpgUniqueAbilityBindingResolution
{
	ERpgAbilityBindingResolveResult Result = ERpgAbilityBindingResolveResult::InvalidAbilityId;
	FGameplayAbilitySpecHandle SpecHandle;
	const URpgGameplayAbility* AbilityCDO = nullptr;
	int32 MatchCount = 0;

	bool IsUnique() const
	{
		return Result == ERpgAbilityBindingResolveResult::Unique && SpecHandle.IsValid();
	}
};

/** Shared server-side resolver for quick-access and weapon-ability bindings. */
class SURVIVALRPG_API FRpgAbilityBindingResolver
{
public:
	/**
	 * Resolves AbilityIdTag against dynamic spec-source tags and static ability asset tags.
	 * Ambiguous ids are logged as configuration errors and never select an arbitrary spec.
	 */
	static FRpgUniqueAbilityBindingResolution ResolveUniqueAbilityId(
		const URpgAbilitySystemComponent* AbilitySystemComponent,
		FGameplayTag AbilityIdTag,
		const UObject* BindingOwner = nullptr);
};
