#include "RpgDeadMovementMode.h"

#include "Components/SceneComponent.h"
#include "MoverDataModelTypes.h"

const FName URpgDeadMovementMode::ModeName(TEXT("RpgDead"));

void URpgDeadMovementMode::SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState)
{
	FMoverDefaultSyncState& OutputSync = OutputState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();
	const USceneComponent* UpdatedComponent = Params.MovingComps.UpdatedComponent.Get();
	const FVector Location = UpdatedComponent ? UpdatedComponent->GetComponentLocation() : OutputSync.GetLocation_WorldSpace();
	const FRotator Orientation = UpdatedComponent ? UpdatedComponent->GetComponentRotation() : OutputSync.GetOrientation_WorldSpace();
	OutputSync.SetTransforms_WorldSpace(Location, Orientation, FVector::ZeroVector, FVector::ZeroVector);
	OutputSync.MoveDirectionIntent = FVector::ZeroVector;
	OutputState.SyncState.MovementMode = ModeName;
	OutputState.MovementEndState.ResetToDefaults();
	if (USceneComponent* MovingComponent = Params.MovingComps.UpdatedComponent.Get())
	{
		MovingComponent->ComponentVelocity = FVector::ZeroVector;
	}
}
