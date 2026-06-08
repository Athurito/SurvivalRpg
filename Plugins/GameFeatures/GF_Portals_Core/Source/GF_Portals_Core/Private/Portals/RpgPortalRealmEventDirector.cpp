#include "Portals/RpgPortalRealmEventDirector.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgPortalRealmEventDirector)

ARpgPortalRealmEventDirector::ARpgPortalRealmEventDirector(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
}

void ARpgPortalRealmEventDirector::HandleRealmStarted_Implementation(ARpgPortalActor* Portal, const URpgPortalEncounterDefinition* EncounterDefinition)
{
}

void ARpgPortalRealmEventDirector::HandleParticipantEntered_Implementation(ARpgPortalActor* Portal, AActor* Participant)
{
}

void ARpgPortalRealmEventDirector::HandleParticipantExited_Implementation(ARpgPortalActor* Portal, AActor* Participant)
{
}

void ARpgPortalRealmEventDirector::HandleBossDefeated_Implementation(ARpgPortalActor* Portal, AActor* Boss)
{
}

void ARpgPortalRealmEventDirector::HandleExitOpened_Implementation(ARpgPortalActor* Portal, AActor* ExitPortal)
{
}

void ARpgPortalRealmEventDirector::HandleRealmClosing_Implementation(ARpgPortalActor* Portal)
{
}
