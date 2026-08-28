#pragma once

#include "AbilitySystemInterface.h"
#include "GameFramework/Actor.h"
#include "GenericTeamAgentInterface.h"

#include "RpgCombatNetworkTestTypes.generated.h"

class UCapsuleComponent;
class URpgAbilitySystemComponent;
class URpgDefenseSet;
class URpgHealthComponent;
class URpgHealthSet;
struct FGameplayEffectSpec;

/**
 * Replicated hostile target used by rendered melee network automation.
 *
 * The fixture deliberately keeps real GAS health and defense state while exposing only
 * deterministic, server-observed damage evidence to the test.
 */
UCLASS(NotBlueprintable, Transient)
class ARpgCombatNetworkTargetFixture final
	: public AActor
	, public IAbilitySystemInterface
	, public IGenericTeamAgentInterface
{
	GENERATED_BODY()

public:
	explicit ARpgCombatNetworkTargetFixture(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void SetGenericTeamId(const FGenericTeamId& NewTeamId) override;
	virtual FGenericTeamId GetGenericTeamId() const override;
	virtual void PostInitializeComponents() override;

	/** Query-only primitive that blocks the production weapon trace channel. */
	UCapsuleComponent* GetHitCollision() const { return HitCollision; }

	URpgAbilitySystemComponent* GetRpgAbilitySystemComponent() const
	{
		return AbilitySystemComponent;
	}

	URpgHealthSet* GetHealthSet() const { return HealthSet; }
	URpgDefenseSet* GetDefenseSet() const { return DefenseSet; }
	URpgHealthComponent* GetHealthComponent() const { return HealthComponent; }

	/** Number of authoritative health decreases carrying a real GameplayEffect spec. */
	int32 GetHealthDropCount() const { return HealthDropCount; }

	AActor* GetLastDamageInstigator() const { return LastDamageInstigator.Get(); }
	AActor* GetLastDamageCauser() const { return LastDamageCauser.Get(); }

	/** GameplayEffect context source object, normally the equipped weapon instance. */
	UObject* GetLastDamageSource() const { return LastDamageSource.Get(); }

	float GetLastHealthBeforeDamage() const { return LastHealthBeforeDamage; }
	float GetLastHealthAfterDamage() const { return LastHealthAfterDamage; }

	/** Clears observations without mutating authoritative health or GAS state. */
	void ResetDamageObservations();

private:
	void HandleHealthChanged(
		AActor* DamageInstigator,
		AActor* DamageCauser,
		const FGameplayEffectSpec* DamageEffectSpec,
		float DamageMagnitude,
		float OldValue,
		float NewValue);

private:
	UPROPERTY()
	TObjectPtr<UCapsuleComponent> HitCollision;

	UPROPERTY()
	TObjectPtr<URpgAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<URpgHealthSet> HealthSet;

	UPROPERTY()
	TObjectPtr<URpgDefenseSet> DefenseSet;

	UPROPERTY()
	TObjectPtr<URpgHealthComponent> HealthComponent;

	FGenericTeamId TeamId;
	int32 HealthDropCount = 0;
	TWeakObjectPtr<AActor> LastDamageInstigator;
	TWeakObjectPtr<AActor> LastDamageCauser;
	TWeakObjectPtr<UObject> LastDamageSource;
	float LastHealthBeforeDamage = 0.0f;
	float LastHealthAfterDamage = 0.0f;
};
