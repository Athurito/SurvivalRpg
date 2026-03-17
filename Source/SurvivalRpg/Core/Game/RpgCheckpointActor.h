// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RpgCheckpointActor.generated.h"

class ARpgPlayerController;
class UArrowComponent;
class UPrimitiveComponent;
class USceneComponent;
class USphereComponent;
struct FHitResult;

UCLASS(Blueprintable)
class SURVIVALRPG_API ARpgCheckpointActor : public AActor
{
	GENERATED_BODY()

public:
	ARpgCheckpointActor();

	UFUNCTION(BlueprintCallable, Category = "Rpg|Checkpoint")
	void ActivateCheckpointForActor(AActor* ActivatingActor);

	UFUNCTION(BlueprintCallable, Category = "Rpg|Checkpoint")
	void ActivateCheckpointForPlayerController(ARpgPlayerController* PlayerController);

	UFUNCTION(BlueprintPure, Category = "Rpg|Checkpoint")
	FTransform GetRespawnTransform() const;

protected:
	UFUNCTION()
	void HandleActivationSphereBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	bool TryResolvePlayerController(AActor* ActivatingActor, ARpgPlayerController*& OutPlayerController) const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Checkpoint")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Checkpoint")
	TObjectPtr<USphereComponent> ActivationSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rpg|Checkpoint")
	TObjectPtr<UArrowComponent> RespawnPoint;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rpg|Checkpoint")
	bool bActivateOnOverlap = true;
};
