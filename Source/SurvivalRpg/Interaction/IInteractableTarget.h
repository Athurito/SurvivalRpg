// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "InteractionOption.h"
#include "IInteractableTarget.generated.h"

struct FInteractionQuery;

/** Scoped builder that stamps every provider-authored option with its producing interface. */
class FInteractionOptionBuilder
{
public:
	FInteractionOptionBuilder(TScriptInterface<IInteractableTarget> InterfaceTargetScope, TArray<FInteractionOption>& InteractOptions)
		: Scope(InterfaceTargetScope)
		, Options(InteractOptions)
	{
	}

	/** Adds one option to the current query result without transferring gameplay authority to presentation code. */
	void AddInteractionOption(const FInteractionOption& Option)
	{
		FInteractionOption& OptionEntry = Options.Add_GetRef(Option);
		OptionEntry.InteractableTarget = Scope;
	}

private:
	TScriptInterface<IInteractableTarget> Scope;
	TArray<FInteractionOption>& Options;
};

/** Native-only interaction provider contract used by actors and actor components. */
UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UInteractableTarget : public UInterface
{
	GENERATED_BODY()
};

/** Supplies interaction options and, optionally, commits simple validated actions on the server. */
class IInteractableTarget
{
	GENERATED_BODY()

public:
	/** Gathers current semantic options for the supplied query mode without mutating gameplay state. */
	virtual void GatherInteractionOptions(const FInteractionQuery& InteractQuery, FInteractionOptionBuilder& OptionBuilder) = 0;

	/** Adds provider-owned data to a server-built GAS payload; receivers must still revalidate it. */
	virtual void CustomizeInteractionEventData(const FGameplayTag& InteractionEventTag, FGameplayEventData& InOutEventData) { }

	/**
	 * Executes a previously re-gathered and validated simple interaction on authority.
	 * Specialized GAS abilities may ignore this seam and perform their own committed action.
	 */
	virtual bool CommitInteraction(const FInteractionQuery& AuthoritativeQuery, const FInteractionOption& ValidatedOption) { return false; }
};
