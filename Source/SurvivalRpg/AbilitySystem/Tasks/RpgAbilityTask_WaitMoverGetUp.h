#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "RpgAbilityTask_WaitMoverGetUp.generated.h"

class UAnimMontage;
class URpgMoverRagdollComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FRpgMoverGetUpSelected, UAnimMontage*, Montage, float, StartTimeSeconds, float, PlayRate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRpgMoverGetUpRejected);

/** Waits for an authority-selected getup belonging to this exact GAS activation, independent of OnRep ordering. */
UCLASS()
class SURVIVALRPG_API URpgAbilityTask_WaitMoverGetUp : public UAbilityTask
{
	GENERATED_BODY()
public:
	/** Emits the matching server selection once the gameplay mesh has returned from physical simulation. */
	UPROPERTY(BlueprintAssignable) FRpgMoverGetUpSelected OnSelected;
	/** Ends the wait when this activation loses its live ragdoll episode or cannot bind its component. */
	UPROPERTY(BlueprintAssignable) FRpgMoverGetUpRejected OnRejected;
	/** Bind before or after selection; broadcasts once on server/owner and never starts a second montage itself. */
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (DisplayName = "Wait Rpg Mover Getup Selection", HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "true"))
	static URpgAbilityTask_WaitMoverGetUp* WaitRagdollGetUpSelection(UGameplayAbility* OwningAbility);
	virtual void Activate() override;
protected:
	virtual void OnDestroy(bool bInOwnerFinished) override;
private:
	TWeakObjectPtr<URpgMoverRagdollComponent> RagdollComponent;
	FDelegateHandle StateChangedHandle;
	void CheckSelection();
};
