#pragma once

#include "CoreMinimal.h"
#include "Portals/RpgPortalActor.h"

#include "RpgPortalNetworkTestTypes.generated.h"

class URpgPortalEncounterDefinition;

/**
 * Overworld outbreak portal for tests: its built-in encounter spawns one plain actor as enemy, StartOutbreak makes it
 * sealable at once, and it records every closed presentation this machine plays.
 */
UCLASS(NotBlueprintable, Transient)
class ARpgNetworkAutomationPortalFixture final : public ARpgPortalActor
{
	GENERATED_BODY()

public:
	explicit ARpgNetworkAutomationPortalFixture(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Server: starts the built-in outbreak and makes the portal sealable. */
	bool StartOutbreak();

	/** bInitialState of every closed presentation this machine played, oldest first. */
	const TArray<bool>& GetClosedPresentations() const { return ClosedPresentations; }

protected:
	//~ ARpgPortalActor interface
	virtual void OnPortalClosedPresentation_Implementation(bool bInitialState) override;
	//~ End ARpgPortalActor interface

private:
	UPROPERTY()
	TObjectPtr<URpgPortalEncounterDefinition> Outbreak;

	TArray<bool> ClosedPresentations;
};
