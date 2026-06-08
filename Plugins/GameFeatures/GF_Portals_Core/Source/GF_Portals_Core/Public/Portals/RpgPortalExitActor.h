#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SurvivalRpg/Interaction/IInteractableTarget.h"
#include "RpgPortalExitActor.generated.h"

class ARpgPortalActor;
class UStaticMeshComponent;
class USphereComponent;
class URpgGameplayAbility_ExitPortal;

/**
 * Spawned interaction actor used to leave a completed portal realm.
 *
 * The exit portal is configured with its owning overworld portal, forwards the
 * interaction through the generic interaction ability, and lets the owning
 * portal decide the authoritative return transform and sealable state.
 */
UCLASS(Blueprintable)
class GF_PORTALS_CORE_API ARpgPortalExitActor : public AActor, public IInteractableTarget
{
	GENERATED_BODY()

public:
	ARpgPortalExitActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void GatherInteractionOptions(const FInteractionQuery& InteractQuery, FInteractionOptionBuilder& InteractionBuilder) override;
	virtual void CustomizeInteractionEventData(const FGameplayTag& InteractionEventTag, FGameplayEventData& InOutEventData) override;

	/** Assigns the overworld portal that owns this exit. Must be called before FinishSpawningActor. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Portal|Exit")
	void ConfigureExitPortal(ARpgPortalActor* InOwningPortal);

	/** Requests that the owning portal teleport the actor back to the overworld. Server-only. */
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

	/** Interaction ability granted when this exit is usable. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Portal|Interaction")
	TSubclassOf<URpgGameplayAbility_ExitPortal> ExitPortalAbilityClass;

	/** Replicated owning portal that receives exit interaction requests. */
	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Portal|Exit")
	TObjectPtr<ARpgPortalActor> OwningPortal;
};
