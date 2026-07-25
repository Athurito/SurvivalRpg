#pragma once

#include "RpgInventoryItemDefinition.h"
#include "SurvivalRpg/AbilitySystem/Abilities/RpgGameplayAbility.h"

#include "RpgInventoryUseRequestAutomationTestTypes.generated.h"

class URpgInventoryManagerComponent;
class URpgInventoryUiActionComponent;
struct FRpgInventoryUseRequest;

/**
 * Editor-only ability fixture that issues one identical reentrant item-use
 * request while the original GAS activation is still in progress.
 */
UCLASS(NotBlueprintable, Transient)
class URpgInventoryUseRequestAutomationAbility final
	: public URpgGameplayAbility
{
	GENERATED_BODY()

public:
	explicit URpgInventoryUseRequestAutomationAbility(
		const FObjectInitializer& ObjectInitializer =
			FObjectInitializer::Get());

	virtual bool IsEditorOnly() const override { return true; }

	static void ConfigureReentrantRequest(
		URpgInventoryUiActionComponent* InActionComponent,
		URpgInventoryManagerComponent* InInventory,
		const FRpgInventoryUseRequest& InRequest);
	static void ResetTestState();
	static int32 GetActivationCount();
	static bool DidIssueReentrantRetry();
	static int32 GetStackCountBeforeReentrantRetry();
	static int32 GetStackCountAfterReentrantRetry();
	static float GetObservedEventMagnitude();

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
};

/**
 * Editor-only stackable consumable used to verify item-use replay protection
 * within the UI action facade's bounded request cache.
 */
UCLASS(NotBlueprintable, Transient)
class URpgInventoryUseRequestAutomationItemDefinition final
	: public URpgInventoryItemDefinition
{
	GENERATED_BODY()

public:
	explicit URpgInventoryUseRequestAutomationItemDefinition(
		const FObjectInitializer& ObjectInitializer =
			FObjectInitializer::Get());

	virtual bool IsEditorOnly() const override { return true; }
};
