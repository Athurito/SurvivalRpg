// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Engine/HitResult.h"
#include "InteractionTypes.h"
#include "InteractionQuery.generated.h"


/** Context passed to providers while gathering local, nearby, or authoritative interaction options. */
USTRUCT(BlueprintType)
struct FInteractionQuery
{
	GENERATED_BODY()

public:
	/** The requesting pawn. */
	UPROPERTY(BlueprintReadWrite)
	TWeakObjectPtr<AActor> RequestingAvatar;

	/** Allow us to specify a controller - does not need to match the owner of the requesting avatar. */
	UPROPERTY(BlueprintReadWrite)
	TWeakObjectPtr<AController> RequestingController;

	/** Optional provider-specific, runtime-only context. It is never accepted as authoritative client data. */
	UPROPERTY(BlueprintReadWrite)
	TWeakObjectPtr<UObject> OptionalObjectData;

	/** Distinguishes presentation queries from the final server-authoritative validation query. */
	UPROPERTY(BlueprintReadWrite)
	ERpgInteractionQueryMode QueryMode = ERpgInteractionQueryMode::Focus;

	/** World-space center used for distance and nearby-instance queries, in centimeters. */
	UPROPERTY(BlueprintReadWrite)
	FVector QueryOrigin = FVector::ZeroVector;

	/** Radius or maximum distance associated with this query, in centimeters. */
	UPROPERTY(BlueprintReadWrite)
	float QueryRadius = 0.0f;

	/** Hit that identified the candidate; Item carries the ISM/HISM instance index. */
	UPROPERTY(BlueprintReadWrite)
	FHitResult CandidateHit;
};
