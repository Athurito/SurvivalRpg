// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgGameModeBase.h"

#include "RpgWorldSettings.h"
#include "SurvivalRpg/Core/Character/BasePawnData.h"
#include "SurvivalRpg/Core/Character/RpgPawnExtensionComponent.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"

void ARpgGameModeBase::PostLogin(APlayerController* NewPlayer)
{
	if (const ARpgWorldSettings* WorldSettings = Cast<ARpgWorldSettings>(GetWorld()->GetWorldSettings()))
	{
		if (const UBasePawnData* PawnData = WorldSettings->GetDefaultPawnData())
		{
			if (ARpgPlayerState* PlayerState = NewPlayer->GetPlayerState<ARpgPlayerState>())
			{
				PlayerState->SetPawnData(PawnData);
			}
		}
	}
	Super::PostLogin(NewPlayer);
}

APawn* ARpgGameModeBase::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform)
{
	FActorSpawnParameters SpawnParams;
	SpawnParams.Instigator = GetInstigator();
	SpawnParams.ObjectFlags |= RF_Transient;
	SpawnParams.bDeferConstruction = true;
	
	if (UClass* PawnClass = GetDefaultPawnClassForController(NewPlayer))
	{
		if (APawn* SpawnedPawn = GetWorld()->SpawnActor<APawn>(PawnClass, SpawnTransform, SpawnParams))
		{
			if (URpgPawnExtensionComponent* PawnExt = URpgPawnExtensionComponent::FindPawnExtensionComponent(SpawnedPawn))
			{
				PawnExt->SetPawnData(GetPawnDataForController(NewPlayer));
			}
			
			SpawnedPawn->FinishSpawning(SpawnTransform);
			return SpawnedPawn;
		}
	}
	return nullptr;
}

const UBasePawnData* ARpgGameModeBase::GetPawnDataForController(const AController* InController) const
{
	if (!InController) return nullptr;
	if (ARpgPlayerState* Ps = InController->GetPlayerState<ARpgPlayerState>())
	{
		if (const UBasePawnData* PawnData = Ps->GetPawnData<UBasePawnData>())
		{
			return PawnData;
		}
	}
	return nullptr;
}
