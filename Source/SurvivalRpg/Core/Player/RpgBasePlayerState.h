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

/**
 * Shared PlayerState base for player and AI-controlled pawns.
 *
 * Owns the ASC, replicated PawnData, HealthSet, startup ability grants, and team id.
 * Pawns become ASC avatars through PawnExtension; they do not own the ASC.
 */
UCLASS()
class SURVIVALRPG_API ARpgBasePlayerState : public AModularPlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	/** Extension event sent when this PlayerState's ability grants are ready. */
	static const FName NAME_RpgAbilityReady;

	explicit ARpgBasePlayerState(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~ AActor interface
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PostInitializeComponents() override;
	//~ End AActor interface

	/** Sends an actor extension event so GameFeature actions can grant abilities after the ASC is ready. */
	UFUNCTION(BlueprintCallable, Category = "AbilitySystem")
	void SendAbilitiesChangedEvent();

	UFUNCTION(BlueprintCallable, Category = "Rpg|PlayerState")
	URpgAbilitySystemComponent* GetRpgAbilitySystemComponent() const;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	template<class T>
	const T* GetPawnData() const { return Cast<T>(PawnData); }

	/** Sets replicated PawnData, applies startup tags, grants AbilitySets, and initializes team data. Authority-only. */
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
	/** Ability system component owned by this PlayerState and reused across pawn respawns. */
	UPROPERTY(VisibleAnywhere, Category = "Rpg|AbilitySystem")
	TObjectPtr<URpgAbilitySystemComponent> AbilitySystemComponent;

	/** Data asset that drives pawn setup and startup ability grants. */
	UPROPERTY(ReplicatedUsing = OnRep_PawnData, VisibleAnywhere, Category = "Pawn")
	TObjectPtr<const URpgPawnData> PawnData;

	/** Replicated team id sourced from PawnData. 255 is treated as NoTeam/neutral. */
	UPROPERTY(Replicated, VisibleAnywhere, Category = "Rpg|Team")
	uint8 TeamId = 255;

private:
	/** Health attribute set owned by the PlayerState ASC. */
	UPROPERTY()
	TObjectPtr<const URpgHealthSet> HealthSet;
};
