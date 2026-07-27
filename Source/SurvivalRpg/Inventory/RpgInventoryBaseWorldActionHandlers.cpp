#include "RpgInventoryUiActionDomainHandlers.h"

#include "SurvivalRpg/Base/RpgBaseBuildableDefinition.h"
#include "SurvivalRpg/Base/RpgBaseCampActor.h"
#include "SurvivalRpg/Base/RpgBaseConstructionSiteActor.h"
#include "SurvivalRpg/Crafting/RpgCraftingRecipeDefinition.h"
#include "SurvivalRpg/Crafting/RpgCraftingStationComponent.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"

void FRpgBaseBuildingActionHandler::PlaceBuildable(
	ARpgBaseCampActor* BaseCamp,
	URpgBaseBuildableDefinition* BuildableDefinition,
	FTransform BuildTransform,
	bool bAutoContributeFromBase)
{
	AActor* RequestingActor = GetRequestingActor();
	if (!BaseCamp || !BuildableDefinition || !RequestingActor)
	{
		UE_LOG(
			LogRpgInventoryUiActions,
			Warning,
			TEXT("Place buildable failed: missing input. Owner=%s BaseCamp=%s Buildable=%s RequestingActor=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(BaseCamp),
			*GetNameSafe(BuildableDefinition),
			*GetNameSafe(RequestingActor));
		return;
	}

	if (!BaseCamp->CanPlaceBuildableAtTransform(
			BuildableDefinition,
			BuildTransform,
			RequestingActor))
	{
		UE_LOG(
			LogRpgInventoryUiActions,
			Warning,
			TEXT("Place buildable failed: placement validation denied. Owner=%s BaseCamp=%s Buildable=%s BuildActorClass=%s BaseDist=%.0f BuilderDist=%.0f BuildLocation=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(BaseCamp),
			*GetNameSafe(BuildableDefinition),
			*GetNameSafe(BuildableDefinition->BuildActorClass),
			FVector::Dist(
				BaseCamp->GetActorLocation(),
				BuildTransform.GetLocation()),
			FVector::Dist(
				RequestingActor->GetActorLocation(),
				BuildTransform.GetLocation()),
			*BuildTransform.GetLocation().ToCompactString());
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(
			LogRpgInventoryUiActions,
			Warning,
			TEXT("Place buildable failed: world missing. Owner=%s BaseCamp=%s Buildable=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(BaseCamp),
			*GetNameSafe(BuildableDefinition));
		return;
	}

	TSubclassOf<ARpgBaseConstructionSiteActor> ConstructionSiteClass =
		BuildableDefinition->ConstructionSiteActorClass;
	if (!ConstructionSiteClass)
	{
		ConstructionSiteClass =
			ARpgBaseConstructionSiteActor::StaticClass();
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = BaseCamp;
	SpawnParams.Instigator = Cast<APawn>(RequestingActor);
	SpawnParams.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	ARpgBaseConstructionSiteActor* ConstructionSite =
		World->SpawnActor<ARpgBaseConstructionSiteActor>(
			ConstructionSiteClass,
			BuildTransform,
			SpawnParams);
	if (!ConstructionSite)
	{
		UE_LOG(
			LogRpgInventoryUiActions,
			Warning,
			TEXT("Place buildable failed: construction site spawn failed. Owner=%s BaseCamp=%s Buildable=%s SiteClass=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(BaseCamp),
			*GetNameSafe(BuildableDefinition),
			*GetNameSafe(ConstructionSiteClass));
		return;
	}

	UE_LOG(
		LogRpgInventoryUiActions,
		Log,
		TEXT("Place buildable succeeded: Site=%s BaseCamp=%s Buildable=%s BuildActorClass=%s AutoContributeFromBase=%s Location=%s"),
		*GetNameSafe(ConstructionSite),
		*GetNameSafe(BaseCamp),
		*GetNameSafe(BuildableDefinition),
		*GetNameSafe(BuildableDefinition->BuildActorClass),
		bAutoContributeFromBase ? TEXT("true") : TEXT("false"),
		*BuildTransform.GetLocation().ToCompactString());

	ConstructionSite->InitializeConstructionSite(
		BaseCamp,
		BuildableDefinition);
	if (bAutoContributeFromBase &&
		IsValid(ConstructionSite) &&
		!ConstructionSite->IsConstructionComplete())
	{
		ConstructionSite->ContributeAllResources(
			RequestingActor,
			true);
	}
}

void FRpgBaseBuildingActionHandler::ContributeAll(
	ARpgBaseConstructionSiteActor* ConstructionSite,
	bool bAllowBaseStorage)
{
	AActor* RequestingActor = GetRequestingActor();
	if (ConstructionSite && RequestingActor)
	{
		ConstructionSite->ContributeAllResources(
			RequestingActor,
			bAllowBaseStorage);
	}
}

void FRpgBaseBuildingActionHandler::ContributeMaterial(
	ARpgBaseConstructionSiteActor* ConstructionSite,
	TSubclassOf<URpgInventoryItemDefinition> ItemDefinition,
	int32 StackCount,
	bool bAllowBaseStorage)
{
	AActor* RequestingActor = GetRequestingActor();
	if (ConstructionSite &&
		RequestingActor &&
		ItemDefinition &&
		StackCount > 0)
	{
		ConstructionSite->ContributeMaterial(
			RequestingActor,
			ItemDefinition,
			StackCount,
			bAllowBaseStorage);
	}
}

void FRpgCraftingActionHandler::CraftRecipe(
	URpgCraftingStationComponent* CraftingStation,
	URpgCraftingRecipeDefinition* RecipeDefinition,
	int32 Quantity)
{
	AActor* RequestingActor = GetRequestingActor();
	if (!CraftingStation ||
		!RecipeDefinition ||
		!RequestingActor ||
		!CraftingStation->CanCraftRecipeQuantity(
			RequestingActor,
			RecipeDefinition,
			Quantity))
	{
		return;
	}

	CraftingStation->QueueCraftRecipe(
		RequestingActor,
		RecipeDefinition,
		Quantity);
}

void FRpgCraftingActionHandler::CancelCraftJob(
	URpgCraftingStationComponent* CraftingStation,
	FGuid JobId)
{
	AActor* RequestingActor = GetRequestingActor();
	if (CraftingStation && RequestingActor)
	{
		CraftingStation->CancelCraftJob(RequestingActor, JobId);
	}
}

void FRpgCraftingActionHandler::PauseStation(
	URpgCraftingStationComponent* CraftingStation)
{
	AActor* RequestingActor = GetRequestingActor();
	if (CraftingStation && RequestingActor)
	{
		CraftingStation->PauseCraftingStation(RequestingActor);
	}
}

void FRpgCraftingActionHandler::ResumeStation(
	URpgCraftingStationComponent* CraftingStation)
{
	AActor* RequestingActor = GetRequestingActor();
	if (CraftingStation && RequestingActor)
	{
		CraftingStation->ResumeCraftingStation(RequestingActor);
	}
}

void FRpgCraftingActionHandler::SetOutputAutoDepositEnabled(
	URpgCraftingStationComponent* CraftingStation,
	bool bEnabled)
{
	AActor* RequestingActor = GetRequestingActor();
	if (CraftingStation && RequestingActor)
	{
		CraftingStation->SetCraftingOutputAutoDepositEnabled(
			RequestingActor,
			bEnabled);
	}
}
