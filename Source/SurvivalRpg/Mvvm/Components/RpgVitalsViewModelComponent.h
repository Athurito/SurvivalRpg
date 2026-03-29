// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RpgVitalsViewModelComponent.generated.h"


class UAbilitySystemComponent;
class UPlayerVitalsViewmodel;

// Legacy compatibility component kept so existing blueprints still load.
// Player vitals are now owned by URpgUiSubsystem and resolved via the local player.
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgVitalsViewModelComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPlayerVitalsViewmodel* GetViewModel() const { return ViewModel; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UPlayerVitalsViewmodel> ViewModel;

	UFUNCTION()
	void HandlePawnChanged(APawn* OldPawn, APawn* NewPawn);

	void BindFromPawn(APawn* Pawn);
};
