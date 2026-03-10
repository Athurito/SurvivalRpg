// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RpgDownedComponent.generated.h"


class UAbilitySystemComponent;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgDownedComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	void InitializeWithAbilitySystem(UAbilitySystemComponent* InASC);
	void UninitializeFromAbilitySystem();

	bool TryEnterDowned();
	void StartBleedout();
	void StopBleedout();

	bool IsDowned() const;

	void BeginRevive(AActor* Reviver);
	void CancelRevive(AActor* Reviver);
	void CompleteRevive(AActor* Reviver);

protected:
	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent = nullptr;

	UPROPERTY()
	TObjectPtr<class URpgHealthComponent> HealthComponent = nullptr;

	UPROPERTY(EditDefaultsOnly)
	float BleedoutDuration = 45.0f;

	FTimerHandle BleedoutTimerHandle;
};
