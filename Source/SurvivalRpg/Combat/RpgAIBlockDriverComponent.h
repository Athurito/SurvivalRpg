#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RpgAIBlockDriverComponent.generated.h"

class URpgHealthComponent;

UCLASS(BlueprintType, Blueprintable, ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgAIBlockDriverComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URpgAIBlockDriverComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * Server: enables or disables the chance to raise the block after taking damage. The StateTree's combat control task
	 * disables it while it decides when to block. Runtime state, not replicated.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "AI Defense")
	void SetBlockOnDamageEnabled(bool bEnabled);

	/**
	 * Server: presses block and holds it for Duration seconds (at least one frame). False when a block is already held
	 * or the block ability does not start, for example while staggered or attacking.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "AI Defense")
	bool HoldBlock(float Duration);

	/** Server: releases a held block before its duration ends. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "AI Defense")
	void ReleaseBlock();

	/** Whether this driver currently holds the block input. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "AI Defense")
	bool IsBlockHeld() const { return bBlockInputHeld; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void HandleHealthChanged(URpgHealthComponent* HealthComponent, float OldValue, float NewValue, AActor* Instigator);

private:
	void TryStartBlock();
	/** Presses block; true when the block ability runs afterwards. */
	bool PressBlock(float Duration);

private:
	UPROPERTY(EditDefaultsOnly, Category = "AI Defense", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BlockChanceOnDamage = 0.45f;

	UPROPERTY(EditDefaultsOnly, Category = "AI Defense", meta = (ClampMin = "0.0"))
	float BlockHoldDuration = 0.8f;

	UPROPERTY(EditDefaultsOnly, Category = "AI Defense", meta = (ClampMin = "0.0"))
	float BlockCooldown = 2.0f;

	UPROPERTY(Transient)
	TObjectPtr<URpgHealthComponent> CachedHealthComponent;

	FTimerHandle ReleaseBlockTimerHandle;
	float LastBlockStartTime = -1000.0f;
	bool bBlockInputHeld = false;
	bool bBlockOnDamageEnabled = true;
};
