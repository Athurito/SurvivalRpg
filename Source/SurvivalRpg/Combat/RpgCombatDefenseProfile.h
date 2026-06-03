#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataAsset.h"
#include "RpgCombatDefenseProfile.generated.h"

class URpgAbilitySystemComponent;
class URpgPawnExtensionComponent;
class APawn;
class AController;

USTRUCT(BlueprintType)
struct FRpgCombatDefenseProfileData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stagger")
	bool bCanBeStaggered = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stagger", meta = (ClampMin = "1.0"))
	float MaxStagger = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stagger", meta = (ClampMin = "0.0"))
	float IncomingStaggerDamageMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stagger", meta = (ClampMin = "0.0"))
	float StaggerDuration = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stagger", meta = (ClampMin = "0.0"))
	float StaggerImmunityDuration = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stagger", meta = (ClampMin = "0.0"))
	float StaggeredDamageTakenMultiplier = 1.0f;
};

UCLASS(BlueprintType, Const)
class SURVIVALRPG_API URpgCombatDefenseProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	const FRpgCombatDefenseProfileData& GetProfileData() const { return ProfileData; }

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Defense")
	FRpgCombatDefenseProfileData ProfileData;
};

UCLASS(BlueprintType, Blueprintable, ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class SURVIVALRPG_API URpgCombatDefenseProfileComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URpgCombatDefenseProfileComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintPure, Category = "Combat Defense")
	static URpgCombatDefenseProfileComponent* FindCombatDefenseProfileComponent(const AActor* Actor);

	UFUNCTION(BlueprintCallable, Category = "Combat Defense")
	void ApplyDefenseProfile();

protected:
	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void BindToPawnExtension();
	void UnbindFromPawnExtension();
	void HandleAbilitySystemInitialized();
	void HandleAbilitySystemUninitialized();
	void ApplyProfileToAbilitySystem(URpgAbilitySystemComponent* ASC);
	void ClearAppliedProfile();
	void ScheduleApplyRetry();
	const FRpgCombatDefenseProfileData& GetResolvedProfileData() const;

private:
	UPROPERTY(EditAnywhere, Category = "Combat Defense")
	TObjectPtr<const URpgCombatDefenseProfile> DefenseProfile;

	UPROPERTY(EditAnywhere, Category = "Combat Defense", meta = (EditCondition = "DefenseProfile == nullptr", EditConditionHides))
	FRpgCombatDefenseProfileData FallbackProfileData;

	UPROPERTY(Transient)
	TObjectPtr<URpgPawnExtensionComponent> BoundPawnExtension;

	UPROPERTY(Transient)
	TObjectPtr<URpgAbilitySystemComponent> AppliedAbilitySystemComponent;

	FTimerHandle ApplyRetryTimerHandle;
};

/**
 * Applies a combat defense profile to the currently possessed player pawn.
 *
 * This lives on the PlayerController so GF_Combat_Core can grant player guard/posture
 * rules without adding the same component to AI characters that derive from ARpgCharacter.
 */
UCLASS(BlueprintType, Blueprintable, ClassGroup = (Combat), meta = (BlueprintSpawnableComponent, DisplayName = "Player Combat Defense Profile"))
class SURVIVALRPG_API URpgPlayerCombatDefenseProfileComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URpgPlayerCombatDefenseProfileComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Rpg|Combat Defense")
	void ApplyDefenseProfile();

protected:
	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void BindToPlayerController();
	void UnbindFromPlayerController();
	void BindToPawn(APawn* Pawn);
	void UnbindFromPawnExtension();
	void HandleAbilitySystemInitialized();
	void HandleAbilitySystemUninitialized();
	void ApplyProfileToAbilitySystem(URpgAbilitySystemComponent* ASC);
	void ClearAppliedProfile();
	void ScheduleApplyRetry();
	const FRpgCombatDefenseProfileData& GetResolvedProfileData() const;

	UFUNCTION()
	void HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

	UPROPERTY(EditDefaultsOnly, Category = "Combat Defense")
	TObjectPtr<const URpgCombatDefenseProfile> DefenseProfile;

	UPROPERTY(EditAnywhere, Category = "Combat Defense", meta = (EditCondition = "DefenseProfile == nullptr", EditConditionHides))
	FRpgCombatDefenseProfileData FallbackProfileData;

	UPROPERTY(Transient)
	TObjectPtr<AController> BoundPlayerController;

	UPROPERTY(Transient)
	TObjectPtr<URpgPawnExtensionComponent> BoundPawnExtension;

	UPROPERTY(Transient)
	TObjectPtr<URpgAbilitySystemComponent> AppliedAbilitySystemComponent;

	FTimerHandle ApplyRetryTimerHandle;
};
