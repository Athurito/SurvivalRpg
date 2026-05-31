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

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void HandleHealthChanged(URpgHealthComponent* HealthComponent, float OldValue, float NewValue, AActor* Instigator);

private:
	void TryStartBlock();
	void ReleaseBlock();

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
};
