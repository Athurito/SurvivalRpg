#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SurvivalRpg/Interaction/IInteractableTarget.h"
#include "RpgPortalExitActor.generated.h"

class ARpgPortalActor;
class UStaticMeshComponent;
class USphereComponent;
class URpgGameplayAbility_ExitPortal;

UCLASS(Blueprintable)
class SURVIVALRPG_API ARpgPortalExitActor : public AActor, public IInteractableTarget
{
	GENERATED_BODY()

public:
	ARpgPortalExitActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void GatherInteractionOptions(const FInteractionQuery& InteractQuery, FInteractionOptionBuilder& InteractionBuilder) override;
	virtual void CustomizeInteractionEventData(const FGameplayTag& InteractionEventTag, FGameplayEventData& InOutEventData) override;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Portal|Exit")
	void ConfigureExitPortal(ARpgPortalActor* InOwningPortal);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Portal|Exit")
	bool TryUseExitPortal(AActor* ExitingActor);

	UFUNCTION(BlueprintPure, Category = "Portal|Exit")
	ARpgPortalActor* GetOwningPortal() const { return OwningPortal; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal|Exit")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal|Exit")
	TObjectPtr<USphereComponent> InteractionCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal|Exit")
	TObjectPtr<UStaticMeshComponent> ExitMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal|Interaction")
	TSubclassOf<URpgGameplayAbility_ExitPortal> ExitPortalAbilityClass;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Portal|Exit")
	TObjectPtr<ARpgPortalActor> OwningPortal;
};
