// Copyright Epic Games, Inc. All Rights Reserved.

#include "RpgGameplayEffectContext.h"

#include "RpgAbilitySourceInterface.h"
#include "Engine/HitResult.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

#include "Iris/ReplicationState/PropertyNetSerializerInfoRegistry.h"
#include "Serialization/GameplayEffectContextNetSerializer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgGameplayEffectContext)

class FArchive;

FRpgGameplayEffectContext* FRpgGameplayEffectContext::ExtractEffectContext(struct FGameplayEffectContextHandle Handle)
{
	FGameplayEffectContext* BaseEffectContext = Handle.Get();
	if ((BaseEffectContext != nullptr) && BaseEffectContext->GetScriptStruct()->IsChildOf(FRpgGameplayEffectContext::StaticStruct()))
	{
		return static_cast<FRpgGameplayEffectContext*>(BaseEffectContext);
	}

	return nullptr;
}

bool FRpgGameplayEffectContext::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	FGameplayEffectContext::NetSerialize(Ar, Map, bOutSuccess);

	// Not serialized for post-activation use:
	// CartridgeID

	return true;
}

namespace UE::Net
{
	// Forward to FGameplayEffectContextNetSerializer
	// Note: If FRpgGameplayEffectContext::NetSerialize() is modified, a custom NetSerializer must be implemented as the current fallback will no longer be sufficient.
	UE_NET_IMPLEMENT_FORWARDING_NETSERIALIZER_AND_REGISTRY_DELEGATES(RpgGameplayEffectContext, FGameplayEffectContextNetSerializer);
}

void FRpgGameplayEffectContext::SetAbilitySource(const IRpgAbilitySourceInterface* InObject, float InSourceLevel)
{
	AbilitySourceObject = MakeWeakObjectPtr(Cast<const UObject>(InObject));
	//SourceLevel = InSourceLevel;
}

const IRpgAbilitySourceInterface* FRpgGameplayEffectContext::GetAbilitySource() const
{
	return Cast<IRpgAbilitySourceInterface>(AbilitySourceObject.Get());
}

const UPhysicalMaterial* FRpgGameplayEffectContext::GetPhysicalMaterial() const
{
	if (const FHitResult* HitResultPtr = GetHitResult())
	{
		return HitResultPtr->PhysMaterial.Get();
	}
	return nullptr;
}

