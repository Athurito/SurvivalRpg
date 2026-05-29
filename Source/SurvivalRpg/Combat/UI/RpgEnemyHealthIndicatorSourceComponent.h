#pragma once

#include "Components/ActorComponent.h"
#include "SurvivalRpg/UI/IndicatorSystem/IndicatorDescriptor.h"

#include "RpgEnemyHealthIndicatorSourceComponent.generated.h"

class UIndicatorDescriptor;
class URpgHealthComponent;
class UUserWidget;

/**
 * Client presentation component that registers one projected health indicator after an enemy is damaged.
 */
UCLASS(ClassGroup = (RpgCombat), Blueprintable, meta = (BlueprintSpawnableComponent, DisplayName = "Enemy Health Indicator Source"))
class SURVIVALRPG_API URpgEnemyHealthIndicatorSourceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URpgEnemyHealthIndicatorSourceComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleHealthChanged(URpgHealthComponent* HealthComponent, float OldValue, float NewValue, AActor* Instigator);

	UFUNCTION()
	void HandleDeathFinished(AActor* OwningActor);

	void ShowIndicator();
	void RemoveIndicators();

	UPROPERTY(EditDefaultsOnly, Category = "Indicator")
	TSoftClassPtr<UUserWidget> IndicatorWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Indicator")
	EActorCanvasProjectionMode ProjectionMode = EActorCanvasProjectionMode::ActorBoundingBox;

	UPROPERTY(EditDefaultsOnly, Category = "Indicator")
	FVector BoundingBoxAnchor = FVector(0.5f, 0.5f, 1.0f);

	UPROPERTY(EditDefaultsOnly, Category = "Indicator")
	FVector2D ScreenSpaceOffset = FVector2D(0.0f, -12.0f);

	UPROPERTY(EditDefaultsOnly, Category = "Indicator")
	int32 Priority = 0;

	UPROPERTY(Transient)
	TObjectPtr<URpgHealthComponent> BoundHealthComponent;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UIndicatorDescriptor>> ActiveIndicators;
};
