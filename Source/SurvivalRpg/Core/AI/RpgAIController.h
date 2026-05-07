// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ModularAIController.h"
#include "RpgAIController.generated.h"

class URpgAIPawnData;
class URpgPawnExtensionComponent;
class UStateTreeAIComponent;

UCLASS()
class SURVIVALRPG_API ARpgAIController : public AModularAIController
{
	GENERATED_BODY()

public:
	explicit ARpgAIController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void InitPlayerState() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	const URpgAIPawnData* GetDefaultPawnData() const { return DefaultPawnData; }

protected:
	void HandlePawnAbilitySystemInitialized();
	void StopStateTreeLogic(const FString& Reason);

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rpg|AI")
	TObjectPtr<const URpgAIPawnData> DefaultPawnData;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|AI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStateTreeAIComponent> StateTreeComponent;
};
