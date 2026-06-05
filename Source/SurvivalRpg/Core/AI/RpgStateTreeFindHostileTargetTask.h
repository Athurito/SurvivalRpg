#pragma once

#include "CoreMinimal.h"
#include "Tasks/StateTreeAITask.h"
#include "RpgStateTreeFindHostileTargetTask.generated.h"

class AAIController;

USTRUCT(BlueprintType)
struct SURVIVALRPG_API FRpgStateTreeFindHostileTargetTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Output)
	TObjectPtr<AActor> TargetActor = nullptr;

	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.0"))
	float SearchRadius = 3000.0f;

	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.0"))
	float AttackRange = 180.0f;

	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.0"))
	float MoveAcceptableRadius = 140.0f;

	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.05"))
	float RepathInterval = 0.25f;

	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bMoveTowardTarget = true;

	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bSetAttackDriverTarget = true;

	float TimeUntilNextMoveRequest = 0.0f;
};

USTRUCT(meta = (DisplayName = "Find Hostile Target", Category = "Rpg|AI"))
struct SURVIVALRPG_API FRpgStateTreeFindHostileTargetTask : public FStateTreeAITaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FRpgStateTreeFindHostileTargetTaskInstanceData;

	FRpgStateTreeFindHostileTargetTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
	virtual FName GetIconName() const override { return FName("StateTreeEditorStyle|Node.Find"); }
	virtual FColor GetIconColor() const override { return UE::StateTree::Colors::Grey; }
#endif

private:
	static AAIController* ResolveAIController(const FStateTreeExecutionContext& Context);
	static bool IsAlive(const AActor* Actor);
	static bool IsValidHostileTarget(const AActor* Owner, const AActor* Candidate);
	static AActor* FindNearestHostileTarget(const AActor* Owner, float SearchRadius);
	static void SetAttackDriverTarget(AActor* Owner, AActor* Target);
	static bool IsTargetInRange(const AActor* Owner, const AActor* Target, float Range);
	static void RequestMove(AAIController& Controller, AActor& Target, float AcceptableRadius);
};
