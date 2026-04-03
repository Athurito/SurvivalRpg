#pragma once

#include "CoreMinimal.h"
#include "RpgItemFragment.h"
#include "RpgItemFragment_Visual.generated.h"

UCLASS(BlueprintType)
class SURVIVALRPG_API URpgItemFragment_Visual : public URpgItemFragment
{
	GENERATED_BODY()

public:
	TSubclassOf<AActor> GetEquippedActorClass() const { return EquippedActorClass; }
	const FName& GetEquippedSocketName() const { return EquippedSocketName; }
	const FName& GetStowedSocketName() const { return StowedSocketName; }
	const FTransform& GetEquippedRelativeTransform() const { return EquippedRelativeTransform; }
	const FTransform& GetStowedRelativeTransform() const { return StowedRelativeTransform; }
	bool ShouldHideWhenInactiveWithoutStowedSocket() const { return bHideWhenInactiveWithoutStowedSocket; }

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AActor> EquippedActorClass = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual", meta = (AllowPrivateAccess = "true"))
	FName EquippedSocketName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual", meta = (AllowPrivateAccess = "true"))
	FName StowedSocketName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual", meta = (AllowPrivateAccess = "true"))
	FTransform EquippedRelativeTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual", meta = (AllowPrivateAccess = "true"))
	FTransform StowedRelativeTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual", meta = (AllowPrivateAccess = "true"))
	bool bHideWhenInactiveWithoutStowedSocket = true;
};
