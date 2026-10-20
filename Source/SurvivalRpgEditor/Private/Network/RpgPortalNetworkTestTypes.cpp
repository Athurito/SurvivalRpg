#include "Network/RpgPortalNetworkTestTypes.h"

#include "Portals/RpgPortalEncounterDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgPortalNetworkTestTypes)

ARpgNetworkAutomationPortalFixture::ARpgNetworkAutomationPortalFixture(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAlwaysRelevant = true;
	bAutoStartOnBeginPlay = false;

	// A default subobject, so clients resolve the replicated definition by name.
	Outbreak = CreateDefaultSubobject<URpgPortalEncounterDefinition>(TEXT("Outbreak"));
	Outbreak->EncounterMode = ERpgPortalEncounterMode::BrokenOutbreak;
	Outbreak->SpawnRadius = 300.0f;
	FRpgPortalEnemySpawnEntry& Enemy = Outbreak->EnemySpawnEntries.AddDefaulted_GetRef();
	Enemy.EnemyClass = AActor::StaticClass();
	Enemy.Count = 1;
}

bool ARpgNetworkAutomationPortalFixture::StartOutbreak()
{
	ConfigureEncounterDefinition(Outbreak);
	StartEncounter();
	return ForceSealableForTesting();
}

void ARpgNetworkAutomationPortalFixture::OnPortalClosedPresentation_Implementation(const bool bInitialState)
{
	ClosedPresentations.Add(bInitialState);
}
