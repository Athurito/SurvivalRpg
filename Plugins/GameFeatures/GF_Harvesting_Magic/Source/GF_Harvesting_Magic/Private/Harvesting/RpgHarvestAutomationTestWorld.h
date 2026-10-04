#pragma once

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Harvesting/RpgHarvestAutomationTestTypes.h"
#include "Harvesting/RpgHarvestableComponent.h"
#include "Harvesting/RpgHarvestInstanceStockComponent.h"
#include "UObject/UnrealType.h"

namespace RpgHarvestAutomation
{
	/** Standalone authoritative test world whose timers advance only when a test asks for it. */
	class FScopedTestWorld
	{
	public:
		FScopedTestWorld()
		{
			GameInstance = NewObject<UGameInstance>(GEngine, NAME_None, RF_Transient);
			if (!GameInstance)
			{
				return;
			}

			GameInstance->AddToRoot();
			GameInstance->InitializeStandalone();
			World = GameInstance->GetWorld();
		}

		~FScopedTestWorld()
		{
			UWorld* WorldToDestroy = World;
			if (GameInstance)
			{
				GameInstance->Shutdown();
			}

			if (WorldToDestroy)
			{
				GEngine->DestroyWorldContext(WorldToDestroy);
				WorldToDestroy->DestroyWorld(false);
			}

			if (GameInstance)
			{
				GameInstance->RemoveFromRoot();
			}
			GFrameCounter = CachedFrameCounter;
		}

		UWorld* GetWorld() const
		{
			return World;
		}

		void PrimeTimerManager() const
		{
			if (World)
			{
				++GFrameCounter;
				World->GetTimerManager().Tick(0.0f);
			}
		}

		void AdvanceTimers(float DeltaSeconds) const
		{
			if (World)
			{
				// Respawn deadlines intentionally use authoritative world time while
				// FTimerManager owns the tick-free wakeup queue. A standalone test
				// world does not advance either clock automatically, so keep them in
				// lockstep without ticking unrelated actors.
				World->TimeSeconds += DeltaSeconds;
				World->UnpausedTimeSeconds += DeltaSeconds;
				World->RealTimeSeconds += DeltaSeconds;
				++GFrameCounter;
				World->GetTimerManager().Tick(DeltaSeconds);
			}
		}

	private:
		const uint64 CachedFrameCounter = GFrameCounter;
		TObjectPtr<UGameInstance> GameInstance = nullptr;
		TObjectPtr<UWorld> World = nullptr;
	};

	/**
	 * Gives World a GameState with the instance stock component, the way the harvesting GameFeature adds it.
	 * Returns the stock component, or null when World is null.
	 */
	inline URpgHarvestInstanceStockComponent* AddInstanceStock(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}

		AGameStateBase* GameState = World->GetGameState();
		if (!GameState)
		{
			FActorSpawnParameters SpawnParameters;
			SpawnParameters.ObjectFlags = RF_Transient;
			SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			GameState = World->SpawnActor<AGameStateBase>(SpawnParameters);
			if (!GameState)
			{
				return nullptr;
			}
			World->SetGameState(GameState);
		}
		if (!GameState->HasActorBegunPlay())
		{
			GameState->DispatchBeginPlay();
		}

		URpgHarvestInstanceStockComponent* Stock =
			NewObject<URpgHarvestInstanceStockComponent>(GameState, TEXT("HarvestInstanceStock"));
		Stock->RegisterComponent();
		return Stock;
	}

	/**
	 * Spawns a non-replicated owner of cube instances at Locations, relative to ActorLocation, with Profile; the
	 * instances begin play like a loaded or streamed-in PCG partition actor.
	 */
	inline ARpgHarvestAutomationInstancesActor* SpawnInstances(
		UWorld* World,
		URpgHarvestProfile* Profile,
		const TArray<FVector>& Locations,
		const FVector& ActorLocation = FVector::ZeroVector)
	{
		UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		ARpgHarvestAutomationInstancesActor* Actor = World
			? World->SpawnActorDeferred<ARpgHarvestAutomationInstancesActor>(
				ARpgHarvestAutomationInstancesActor::StaticClass(),
				FTransform(ActorLocation),
				nullptr,
				nullptr,
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn)
			: nullptr;
		if (!Actor || !Actor->Instances || !Cube)
		{
			return nullptr;
		}

		Actor->Instances->ConfigureProfile(Profile);
		Actor->Instances->SetStaticMesh(Cube);
		for (const FVector& Location : Locations)
		{
			Actor->Instances->AddInstance(FTransform(Location));
		}
		Actor->FinishSpawning(FTransform(ActorLocation));
		if (!Actor->HasActorBegunPlay())
		{
			Actor->DispatchBeginPlay();
		}
		return Actor;
	}

	/** Builds a request for InstanceIndex the way an ability does: it observes the revision before committing. */
	inline FRpgHarvestRequest MakeInstanceRequest(
		URpgHarvestableInstancesComponent* Instances,
		const int32 InstanceIndex,
		AActor* Harvester,
		const int32 RequestedSections = 1)
	{
		FRpgHarvestRequest Request;
		Request.Harvester = Harvester;
		Request.AbilityId = FGameplayTag::RequestGameplayTag(TEXT("Ability.Harvesting.Manual"));
		Request.HarvestPower = 1.0f;
		Request.RequestedSections = RequestedSections;
		FTransform InstanceTransform;
		if (Instances && Instances->GetAuthoredInstanceTransform(InstanceIndex, InstanceTransform, true))
		{
			Request.Hit = FHitResult(Instances->GetOwner(), Instances, InstanceTransform.GetLocation(), FVector::UpVector);
			Request.Hit.Item = InstanceIndex;
			Request.ExpectedRevision = IRpgHarvestableTarget::Execute_GetHarvestRevision(Instances, Request.Hit);
		}
		return Request;
	}

	/** Writes the designer-placed weak points of Node, which are protected editor data, the way a Blueprint default does. */
	inline bool ConfigureWeakPoints(URpgHarvestableComponent* Node, const TArray<FVector>& Locations, const float Radius)
	{
		const FArrayProperty* LocationsProperty =
			FindFProperty<FArrayProperty>(URpgHarvestableComponent::StaticClass(), TEXT("WeakPointLocations"));
		const FFloatProperty* RadiusProperty =
			FindFProperty<FFloatProperty>(URpgHarvestableComponent::StaticClass(), TEXT("WeakPointRadius"));
		if (!Node || !LocationsProperty || !RadiusProperty)
		{
			return false;
		}
		*LocationsProperty->ContainerPtrToValuePtr<TArray<FVector>>(Node) = Locations;
		RadiusProperty->SetPropertyValue_InContainer(Node, Radius);
		return true;
	}
}

#endif // WITH_DEV_AUTOMATION_TESTS
