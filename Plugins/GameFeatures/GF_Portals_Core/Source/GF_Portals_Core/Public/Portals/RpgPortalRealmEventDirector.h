#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RpgPortalRealmEventDirector.generated.h"

class ARpgPortalActor;
class URpgPortalEncounterDefinition;

/**
 * Optional server-side realm director spawned into a streamed portal realm.
 *
 * Blueprints can derive from this actor to drive local weather, lighting helpers,
 * VFX, audio, hazards, boss-phase triggers, or other realm events without making
 * ARpgPortalActor own presentation-specific logic.
 */
UCLASS(Blueprintable)
class GF_PORTALS_CORE_API ARpgPortalRealmEventDirector : public AActor
{
	GENERATED_BODY()

public:
	ARpgPortalRealmEventDirector(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintNativeEvent, Category = "Portal|Realm")
	void HandleRealmStarted(ARpgPortalActor* Portal, const URpgPortalEncounterDefinition* EncounterDefinition);

	UFUNCTION(BlueprintNativeEvent, Category = "Portal|Realm")
	void HandleParticipantEntered(ARpgPortalActor* Portal, AActor* Participant);

	UFUNCTION(BlueprintNativeEvent, Category = "Portal|Realm")
	void HandleParticipantExited(ARpgPortalActor* Portal, AActor* Participant);

	UFUNCTION(BlueprintNativeEvent, Category = "Portal|Realm")
	void HandleBossDefeated(ARpgPortalActor* Portal, AActor* Boss);

	UFUNCTION(BlueprintNativeEvent, Category = "Portal|Realm")
	void HandleExitOpened(ARpgPortalActor* Portal, AActor* ExitPortal);

	UFUNCTION(BlueprintNativeEvent, Category = "Portal|Realm")
	void HandleRealmClosing(ARpgPortalActor* Portal);
};
