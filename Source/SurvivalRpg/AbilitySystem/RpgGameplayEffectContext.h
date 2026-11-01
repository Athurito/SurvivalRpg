// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayEffectTypes.h"

#include "RpgGameplayEffectContext.generated.h"

class AActor;
class FArchive;
class IRpgAbilitySourceInterface;
class UObject;
class UPhysicalMaterial;
enum class ERpgCombatImpactOutcome : uint8;

USTRUCT()
struct FRpgGameplayEffectContext : public FGameplayEffectContext
{
	GENERATED_BODY()

	FRpgGameplayEffectContext()
		: FGameplayEffectContext()
	{
	}

	FRpgGameplayEffectContext(AActor* InInstigator, AActor* InEffectCauser)
		: FGameplayEffectContext(InInstigator, InEffectCauser)
	{
	}

	/** Returns the wrapped FRpgGameplayEffectContext from the handle, or nullptr if it doesn't exist or is the wrong type */
	static FRpgGameplayEffectContext* ExtractEffectContext(struct FGameplayEffectContextHandle Handle);

	/** Sets the object used as the ability source */
	void SetAbilitySource(const IRpgAbilitySourceInterface* InObject, float InSourceLevel);

	/** Returns the ability source interface associated with the source object. Only valid on the authority. */
	const IRpgAbilitySourceInterface* GetAbilitySource() const;

	virtual FGameplayEffectContext* Duplicate() const override
	{
		FRpgGameplayEffectContext* NewContext = new FRpgGameplayEffectContext();
		*NewContext = *this;
		if (GetHitResult())
		{
			// Does a deep copy of the hit result
			NewContext->AddHitResult(*GetHitResult(), true);
		}
		return NewContext;
	}

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FRpgGameplayEffectContext::StaticStruct();
	}

	/** Overridden to serialize new fields */
	virtual bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess) override;

	/** Returns the physical material from the hit result if there is one */
	const UPhysicalMaterial* GetPhysicalMaterial() const;

	/**
	 * Server only, never replicated: the defense outcome URpgDamageExecution resolved for this context's latest
	 * application. Unset when the execution applied nothing (an evaded hit, a faction-filtered target, or no damage).
	 * An applier such as ARpgCombatProjectile resets it, applies once and reads it right afterwards.
	 */
	void SetImpactOutcome(ERpgCombatImpactOutcome InOutcome) { ImpactOutcome = InOutcome; }
	void ResetImpactOutcome() { ImpactOutcome.Reset(); }
	TOptional<ERpgCombatImpactOutcome> GetImpactOutcome() const { return ImpactOutcome; }

public:
	/** ID to allow the identification of multiple bullets that were part of the same cartridge */
	UPROPERTY()
	int32 CartridgeID = -1;

protected:
	/** Ability Source object (should implement IRpgAbilitySourceInterface). NOT replicated currently */
	UPROPERTY()
	TWeakObjectPtr<const UObject> AbilitySourceObject;

	/** See GetImpactOutcome. Server only and not serialized, like CartridgeID. */
	TOptional<ERpgCombatImpactOutcome> ImpactOutcome;
};

template<>
struct TStructOpsTypeTraits<FRpgGameplayEffectContext> : public TStructOpsTypeTraitsBase2<FRpgGameplayEffectContext>
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true
	};
};

