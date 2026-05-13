// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GenericTeamAgentInterface.h"
#include "ModularPlayerState.h"
#include "RpgBasePlayerState.generated.h"

class UAbilitySystemComponent;
class URpgAbilitySystemComponent;
class URpgHealthSet;
class URpgPawnData;
struct FGameplayTagContainer;

UCLASS()
class SURVIVALRPG_API ARpgBasePlayerState : public AModularPlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	static const FName NAME_RpgAbilityReady;

	explicit ARpgBasePlayerState(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PostInitializeComponents() override;

	UFUNCTION(BlueprintCallable, Category = "AbilitySystem")
	void SendAbilitiesChangedEvent();

	UFUNCTION(BlueprintCallable, Category = "Rpg|PlayerState")
	URpgAbilitySystemComponent* GetRpgAbilitySystemComponent() const;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	template<class T>
	const T* GetPawnData() const { return Cast<T>(PawnData); }

	void SetPawnData(const URpgPawnData* InPawnData);

	UFUNCTION(BlueprintPure, Category = "Rpg|Team")
	int32 GetTeamId() const;

	FGenericTeamId GetGenericTeamId() const;

	static ETeamAttitude::Type GetTeamAttitudeTowardsActor(FGenericTeamId OwnTeamId, const AActor& Other);

protected:
	UFUNCTION()
	void OnRep_PawnData();

	void ApplyStartupLooseTags(const FGameplayTagContainer& TagContainer) const;
	void SetTeamIdFromPawnData(const URpgPawnData& InPawnData);

protected:
	UPROPERTY(VisibleAnywhere, Category = "Rpg|AbilitySystem")
	TObjectPtr<URpgAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(ReplicatedUsing = OnRep_PawnData, VisibleAnywhere, Category = "Pawn")
	TObjectPtr<const URpgPawnData> PawnData;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "Rpg|Team")
	uint8 TeamId = 255;

private:
	UPROPERTY()
	TObjectPtr<const URpgHealthSet> HealthSet;
};
