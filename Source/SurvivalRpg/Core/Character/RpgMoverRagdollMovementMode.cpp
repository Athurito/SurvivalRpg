#include "RpgMoverRagdollMovementMode.h"

#include "Components/SceneComponent.h"
#include "MoverDataModelTypes.h"
#include "RpgMoverRagdollTypes.h"

const FName URpgMoverRagdollMovementMode::ModeName(TEXT("Ragdoll"));

void URpgMoverRagdollMovementMode::SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState)
{
	FMoverDefaultSyncState& Output = OutputState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();
	// Mover already restored the historical start transform before calling the mode. Holding that capsule
	// never consumes the separately simulated mesh or client-provided bone transforms.
	USceneComponent* Updated = Params.MovingComps.UpdatedComponent.Get();
	const FVector Location = Updated ? Updated->GetComponentLocation() : Output.GetLocation_WorldSpace();
	const FRotator Rotation = Updated ? Updated->GetComponentRotation() : Output.GetOrientation_WorldSpace();
	Output.SetTransforms_WorldSpace(Location, Rotation, FVector::ZeroVector, FVector::ZeroVector);
	Output.MoveDirectionIntent = FVector::ZeroVector;
	OutputState.SyncState.MovementMode = ModeName;
	OutputState.MovementEndState.ResetToDefaults();
	if (Updated) { Updated->ComponentVelocity = FVector::ZeroVector; }
}
