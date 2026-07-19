// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ControllerComponent.h"
#include "RpgIndicatorManagerComponent.generated.h"


class UIndicatorDescriptor;

/**
 * Per-controller registry for local projected UI indicators.
 *
 * The component owns descriptor state and broadcasts changes. Presentation is
 * supplied by the authored URpgIndicatorLayer inside the CommonUI HUD layout.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgIndicatorManagerComponent : public UControllerComponent
{
public:
	GENERATED_BODY()
	explicit URpgIndicatorManagerComponent(const FObjectInitializer& ObjectInitializer);

	/** Returns the indicator registry attached to the supplied controller. */
	static URpgIndicatorManagerComponent* GetComponent(AController* Controller);

	/** Registers a locally presented descriptor with this controller. */
	UFUNCTION(BlueprintCallable, Category = Indicator)
	void AddIndicator(UIndicatorDescriptor* IndicatorDescriptor);
	
	/** Removes a descriptor and releases its presentation from this controller. */
	UFUNCTION(BlueprintCallable, Category = Indicator)
	void RemoveIndicator(UIndicatorDescriptor* IndicatorDescriptor);

	DECLARE_EVENT_OneParam(URpgIndicatorManagerComponent, FIndicatorEvent, UIndicatorDescriptor* Descriptor)
	FIndicatorEvent OnIndicatorAdded;
	FIndicatorEvent OnIndicatorRemoved;

	/** Read-only descriptor set consumed by the authored indicator layer. */
	const TArray<UIndicatorDescriptor*>& GetIndicators() const { return Indicators; }

private:
	/** Transient local presentation descriptors; never replicated or saved. */
	UPROPERTY()
	TArray<TObjectPtr<UIndicatorDescriptor>> Indicators;
};
