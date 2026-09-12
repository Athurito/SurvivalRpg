// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "RpgCharacterMovementComponent.generated.h"


/**
 * FRpgCharacterGroundInfo
 *
 *	Information about the ground under the character.  It only gets updated as needed.
 */
USTRUCT(BlueprintType)
struct FRpgCharacterGroundInfo
{
	GENERATED_BODY()

	FRpgCharacterGroundInfo()
		: LastUpdateFrame(0)
		, GroundDistance(0.0f)
	{}

	uint64 LastUpdateFrame;

	UPROPERTY(BlueprintReadOnly)
	FHitResult GroundHitResult;

	UPROPERTY(BlueprintReadOnly)
	float GroundDistance;
};


UCLASS(Config = Game)
class SURVIVALRPG_API URpgCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	explicit URpgCharacterMovementComponent(const FObjectInitializer& ObjectInitializer);
	
	virtual bool CanAttemptJump() const override;

	/** Acquires one validated mantle obstacle ignore; server state replicates to simulated proxies only. */
	bool BeginMantleCollisionIgnore(UPrimitiveComponent* Component);

	/** Releases only the matching mantle obstacle; existing unrelated collision ignores are preserved. */
	void EndMantleCollisionIgnore(UPrimitiveComponent* ExpectedComponent);

	/** Current obstacle for this component's mantle collision lease, or null while ordinary movement owns collision. */
	UPrimitiveComponent* GetMantleCollisionComponent() const { return MantleCollisionComponent; }

	/** Root-motion rotation owns the same validated mantle lifetime on the predicting owner, server and simulated proxies. */
	bool IsMantleControllingRotation() const { return MantleCollisionComponent != nullptr; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Returns the current ground info.  Calling this will update the ground info if it's out of date.
	UFUNCTION(BlueprintCallable, Category = "Rpg|CharacterMovement")
	const FRpgCharacterGroundInfo& GetGroundInfo();

	//~UMovementComponent interface
	virtual FRotator GetDeltaRotation(float DeltaTime) const override;
	virtual float GetMaxSpeed() const override;
	//~End of UMovementComponent interface
protected:
	/** Preserves authored rotation warping during mantle even when ordinary locomotion enables physics rotation. */
	virtual void PhysicsRotation(float DeltaTime) override;

	// Cached ground info for the character.  Do not access this directly!  It's only updated when accessed via GetGroundInfo().
	FRpgCharacterGroundInfo CachedGroundInfo;

private:
	UFUNCTION()
	void OnRep_MantleCollisionComponent();

	/** Server-selected collision exception for simulated root-motion physics; the owning client predicts its own lease. */
	UPROPERTY(Transient, ReplicatedUsing = OnRep_MantleCollisionComponent)
	TObjectPtr<UPrimitiveComponent> MantleCollisionComponent;

	TWeakObjectPtr<UPrimitiveComponent> AppliedMantleCollisionComponent;
	bool bAddedMantleCollisionIgnore = false;
};
