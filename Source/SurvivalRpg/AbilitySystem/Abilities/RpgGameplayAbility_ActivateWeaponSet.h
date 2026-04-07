#pragma once

#include "CoreMinimal.h"
#include "RpgGameplayAbility.h"
#include "RpgGameplayAbility_ActivateWeaponSet.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UAnimMontage;
class URpgEquipmentComponent;
class URpgWeaponPresentationComponent;

UCLASS()
class SURVIVALRPG_API URpgGameplayAbility_ActivateWeaponSet : public URpgGameplayAbility
{
	GENERATED_BODY()

public:
	URpgGameplayAbility_ActivateWeaponSet();

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

private:
	UFUNCTION()
	void OnEquipGameplayEventReceived(FGameplayEventData Payload);

	UFUNCTION()
	void OnEquipMontageCompleted();

	UFUNCTION()
	void OnEquipMontageInterrupted();

	UFUNCTION()
	void OnEquipMontageCancelled();

	bool ShouldDrivePresentationLocally(const FGameplayAbilityActorInfo* ActorInfo) const;
	void StartWaitingForEquipEvent(FGameplayTag EventTag);
	void ResetEquipTasks();
	URpgEquipmentComponent* ResolveEquipmentComponent(const FGameplayAbilityActorInfo* ActorInfo) const;
	URpgWeaponPresentationComponent* ResolvePresentationComponent(const FGameplayAbilityActorInfo* ActorInfo) const;
	const class URpgItemFragment_Visual* GetPrimaryPresentationVisualFragmentForWeaponSet(const URpgEquipmentComponent* EquipmentComponent, int32 InWeaponSetIndex) const;
	UAnimMontage* ResolvePresentationMontage(const URpgEquipmentComponent* EquipmentComponent, int32 InWeaponSetIndex, bool bUseEquipMontage) const;
	bool MontageUsesPresentationNotify(const UAnimMontage* Montage) const;
	void ApplyPredictedVisibleState() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment", meta = (AllowPrivateAccess = "true", ClampMin = "0"))
	int32 WeaponSetIndex = 0;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> ActiveMontageTask = nullptr;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UAbilityTask_WaitGameplayEvent>> ActiveEquipEventTasks;

	UPROPERTY(Transient)
	int32 ExpectedVisibleWeaponSetIndex = INDEX_NONE;
};
