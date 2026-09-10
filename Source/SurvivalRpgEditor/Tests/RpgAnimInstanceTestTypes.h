// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SurvivalRpg/Animation/RpgAnimInstance.h"
#include "SurvivalRpg/Core/Character/RpgCharacter.h"

#include "RpgAnimInstanceTestTypes.generated.h"

/** Transient character exposing role setup without possession, network sockets, or gameplay startup. */
UCLASS(NotBlueprintable, Transient)
class ARpgAnimInstanceTestCharacter final : public ARpgCharacter
{
	GENERATED_BODY()

public:
	/** Sets only the fixture's local role; remote ownership is configured through SetAutonomousProxy. */
	void SetTestLocalRole(ENetRole InRole) { SetRole(InRole); }
};

/** Counts actual engine graph updates rather than the game-thread pre-update callbacks. */
UCLASS(NotBlueprintable, Transient)
class URpgAnimInstanceTestInstance final : public URpgAnimInstance
{
	GENERATED_BODY()

public:
	/** Disables only the RPG guard for an engine-baseline control using the identical proxy. */
	bool bUseRpgTimingGuard = true;

	virtual bool CanRunParallelWork() const override;

	/** Number of graph dispatches completed by this transient instance. */
	int32 GetGraphUpdateCount();

	/** Sum, in seconds, of the deltas actually delivered to the animation graph. */
	float GetGraphElapsedSeconds();

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;
};
