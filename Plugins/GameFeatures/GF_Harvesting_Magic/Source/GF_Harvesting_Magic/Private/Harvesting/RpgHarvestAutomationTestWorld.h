#pragma once

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Harvesting/RpgHarvestableComponent.h"
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
