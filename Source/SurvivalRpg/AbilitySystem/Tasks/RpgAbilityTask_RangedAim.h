#pragma once

#include "Abilities/Tasks/AbilityTask.h"
#include "SurvivalRpg/Combat/RpgRangedAim.h"

#include "RpgAbilityTask_RangedAim.generated.h"

class URpgAbilitySystemComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRpgRangedAimInterrupted, FName, Reason);

/**
 * Holds a ranged aim stance for the owning ability's activation, such as the bow's held aim or its drawn shot.
 *
 * The task acquires one aim token on the ability system: the stance's movement cap (tuned through
 * Ability.Tuning.Ranged.AimMovement once per activation) and its camera. Tokens of the aim and the shot stack, so ending
 * either never clears the other, and the camera follows their combined state. The stance ends with the task.
 *
 * On owner and authority the task interrupts the stance by cancelling its ability when the avatar leaves the ground
 * (jump, fall, traversal), a sprint starts, or another ability runs in an exclusive activation group (dodge, attack,
 * stagger, death); abilities tagged Ability.Type.RangedAim never interrupt it. A cancelled activation whose input is
 * still held waits for its release, so a held button never restarts the stance.
 */
UCLASS()
class SURVIVALRPG_API URpgAbilityTask_RangedAim : public UAbilityTask
{
	GENERATED_BODY()

public:
	URpgAbilityTask_RangedAim(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The stance was interrupted for Reason; the owning ability is cancelled right after. Owner and authority only. */
	UPROPERTY(BlueprintAssignable)
	FRpgRangedAimInterrupted OnInterrupted;

	/** Holds the aim stance with AimDefinition, for abilities without a ranged weapon. */
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (DisplayName = "Hold Ranged Aim", HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "true"))
	static URpgAbilityTask_RangedAim* HoldRangedAim(UGameplayAbility* OwningAbility, const FRpgRangedAimDefinition& AimDefinition);

	/** Holds the aim stance of the equipped ranged weapon, the owning ability's source object. */
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (DisplayName = "Hold Ranged Aim From Weapon", HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "true"))
	static URpgAbilityTask_RangedAim* HoldRangedAimFromWeapon(UGameplayAbility* OwningAbility);

	/** This activation's movement cap in cm/s after tuning, zero for none. */
	UFUNCTION(BlueprintPure, Category = "Ability|Tasks")
	float GetMovementSpeedLimit() const { return MovementSpeedLimit; }

	/** True while the task holds its aim token. */
	UFUNCTION(BlueprintPure, Category = "Ability|Tasks")
	bool IsHoldingAim() const { return Token != 0; }

	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;

#if WITH_DEV_AUTOMATION_TESTS
	uint32 GetTokenForTests() const { return Token; }
#endif

protected:
	virtual void OnDestroy(bool bInOwnerFinished) override;

private:
	/** Why the stance must end now, or None. */
	FName FindInterruption() const;
	void HandleAbilityCancelled();
	void ReleaseToken();

	FRpgRangedAimDefinition Definition;
	bool bDefinitionFromWeapon = false;
	TWeakObjectPtr<URpgAbilitySystemComponent> CapturedASC;
	FGameplayAbilitySpecHandle CapturedSpec;
	FDelegateHandle CancelledHandle;
	uint32 Token = 0;
	float MovementSpeedLimit = 0.0f;
	bool bSprintAtStart = false;
	bool bInterrupting = false;
};
