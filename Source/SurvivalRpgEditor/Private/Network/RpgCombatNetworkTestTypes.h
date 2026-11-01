#pragma once

#include "AbilitySystemInterface.h"
#include "GameFramework/Actor.h"
#include "GenericTeamAgentInterface.h"

#include "RpgCombatNetworkTestTypes.generated.h"

class AGameModeBase;
class ARpgGameModeBase;
class UBoxComponent;
class UCapsuleComponent;
class USceneComponent;
class URpgAbilitySystemComponent;
class URpgDefenseSet;
class URpgHealthComponent;
class URpgHealthSet;
class UWorld;
struct FGameplayEffectSpec;
struct FWorldInitializationValues;

/** Replicated static ground shared by the server and client combat test worlds. */
UCLASS(NotBlueprintable, Transient)
class ARpgCombatNetworkFloorFixture : public AActor
{
	GENERATED_BODY()

public:
	/** Creates a network-addressable floor without depending on a locomotion implementation. */
	explicit ARpgCombatNetworkFloorFixture(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

private:
	UPROPERTY()
	TObjectPtr<USceneComponent> SceneRoot;

protected:
	UPROPERTY()
	TObjectPtr<UBoxComponent> Collision;
};

/** Ragdoll-only ground uses the authored map's WorldStatic channel on every replicated instance. */
UCLASS(NotBlueprintable, Transient)
class ARpgMoverRagdollFloorFixture final : public ARpgCombatNetworkFloorFixture
{
	GENERATED_BODY()

public:
	explicit ARpgMoverRagdollFloorFixture(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

/**
 * Spawns the floor fixture and spaced player starts while a PIE GameMode initializes, before any pawn exists.
 *
 * The CQTest network map is empty. A floor spawned in a later test step races the pawns, which start falling
 * at once and can already be below its top when it appears. Start it in BEFORE_EACH, before the network
 * component builds the session, and keep it alive through network teardown.
 */
class FRpgScopedNetworkTestGround final
{
public:
	~FRpgScopedNetworkTestGround();

	/** Installs the InitGame hook for every authoritative PIE world created while this object lives. */
	void Start();

private:
	void OnGameModeInitialized(AGameModeBase* GameMode);

	FDelegateHandle Handle;
};

/**
 * Keeps every authoritative PIE world created while this object lives off disk before its GameMode can load a save.
 *
 * ARpgGameModeBase loads and saves the world (harvest stock, storage, progression) in its slots. Each isolated GameMode
 * gets disk persistence off and its own slot names, so a test never reads or overwrites a developer's save. Start it in
 * BEFORE_EACH, before the network component creates its worlds, and keep it alive through network teardown.
 */
class FRpgScopedPIEWorldSaveIsolation final
{
public:
	~FRpgScopedPIEWorldSaveIsolation();

	/** Installs the InitGame hook; call once. */
	void Start();

	/** Whether World is a PIE world whose GameMode, if it owns one, was isolated by this object. */
	bool IsWorldIsolated(UWorld* World) const;

private:
	void IsolateGameMode(AGameModeBase* InitializedGameMode);

	FString SlotPrefix;
	FDelegateHandle GameModeInitializedHandle;
	TMap<TWeakObjectPtr<const ARpgGameModeBase>, FString> GameModeSlots;
};

/**
 * Caps the game time every PIE world advances in one frame while this object lives.
 *
 * Rendered PIE shares the GPU with every other process. Under load the editor drops to about 10 fps, and the
 * authority samples a 0.18 s melee attack window only about twice, far coarser than the 60 Hz server the combat tests
 * model. With the cap, slow frames slow the worlds down instead of taking larger steps; frames
 * faster than the cap are unaffected. Start it in BEFORE_EACH, before the network component creates its worlds, and
 * keep it alive through network teardown.
 */
class FRpgScopedNetworkTestFrameTimeLimit final
{
public:
	~FRpgScopedNetworkTestFrameTimeLimit();

	/** Installs the hook for every PIE world initialized while this object lives; MaxFrameTime in seconds, > 0. */
	void Start(float InMaxFrameTime);

private:
	void OnPostWorldInitialization(UWorld* World, const FWorldInitializationValues InitializationValues);

	FDelegateHandle Handle;
	float MaxFrameTime = 0.0f;
};

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

	/** Query-only primitive that blocks the production weapon and projectile trace channels. */
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
