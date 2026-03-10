// Fill out your copyright notice in the Description page of Project Settings.


#include "RpgRespawnComponent.h"

#include "SurvivalRpg/SurvivalRpg.h"
#include "SurvivalRpg/Core/Game/RpgGameModeBase.h"


URpgRespawnComponent::URpgRespawnComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.bCanEverTick = true;

	SetIsReplicatedByDefault(true);
}

void URpgRespawnComponent::OnUnregister()
{
	SetComponentTickEnabled(false);
	Super::OnUnregister();
}

// ---------------------------------------------------------------------------
// Respawn Flow
// ---------------------------------------------------------------------------

void URpgRespawnComponent::StartRespawnTimer()
{
	if (RespawnState != ERpgRespawnState::Alive)
	{
		return;
	}

	bRespawnTimerElapsed = false;
	RespawnTimerRemaining = RespawnDelay;

	SetRespawnState(ERpgRespawnState::WaitingForRespawn);

	if (RespawnDelay <= 0.0f)
	{
		OnRespawnTimerExpired();
	}
	else
	{
		SetComponentTickEnabled(true);
	}

	UE_LOG(LogRpg, Log, TEXT("RpgRespawnComponent: [%s] respawn timer started. Delay: %.1fs"), *GetNameSafe(GetOwner()), RespawnDelay);
}

void URpgRespawnComponent::RequestRespawn()
{
	if (!CanRespawnNow())
	{
		UE_LOG(LogRpg, Warning, TEXT("RpgRespawnComponent: [%s] cannot respawn yet."), *GetNameSafe(GetOwner()));
		return;
	}

	// Forward to GameMode on the server.
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		// We are on the server — call GameMode directly.
		if (UWorld* World = GetWorld())
		{
			if (ARpgGameModeBase* GM = World->GetAuthGameMode<ARpgGameModeBase>())
			{
				if (APawn* Pawn = Cast<APawn>(GetOwner()))
				{
					if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
					{
						GM->RequestPlayerRespawn(PC);
						return;
					}
				}
			}
		}
	}

	// Client path: use Server RPC.
	ServerRequestRespawn();
}

void URpgRespawnComponent::ServerRequestRespawn_Implementation()
{
	if (UWorld* World = GetWorld())
	{
		if (ARpgGameModeBase* GM = World->GetAuthGameMode<ARpgGameModeBase>())
		{
			if (APawn* Pawn = Cast<APawn>(GetOwner()))
			{
				if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
				{
					GM->RequestPlayerRespawn(PC);
				}
			}
		}
	}
}

void URpgRespawnComponent::OnServerRespawnExecuted(const FTransform& RespawnTransform)
{
	SetComponentTickEnabled(false);
	bRespawnTimerElapsed = false;
	RespawnTimerRemaining = 0.0f;

	SetRespawnState(ERpgRespawnState::Alive);

	OnRespawnCompleted.Broadcast(RespawnTransform);

	UE_LOG(LogRpg, Log, TEXT("RpgRespawnComponent: [%s] respawn completed at %s."), *GetNameSafe(GetOwner()), *RespawnTransform.GetLocation().ToString());
}

// ---------------------------------------------------------------------------
// Tick — counts down respawn delay
// ---------------------------------------------------------------------------

void URpgRespawnComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (RespawnState != ERpgRespawnState::WaitingForRespawn || bRespawnTimerElapsed)
	{
		SetComponentTickEnabled(false);
		return;
	}

	RespawnTimerRemaining = FMath::Max(RespawnTimerRemaining - DeltaTime, 0.0f);
	OnRespawnTimerTick.Broadcast(RespawnTimerRemaining, RespawnDelay);

	if (RespawnTimerRemaining <= 0.0f)
	{
		OnRespawnTimerExpired();
	}
}

float URpgRespawnComponent::GetRespawnTimeRemaining() const
{
	return RespawnTimerRemaining;
}

// ---------------------------------------------------------------------------
// Internal
// ---------------------------------------------------------------------------

void URpgRespawnComponent::SetRespawnState(ERpgRespawnState NewState)
{
	if (RespawnState == NewState) return;

	RespawnState = NewState;
	OnRespawnStateChanged.Broadcast(NewState);
}

void URpgRespawnComponent::OnRespawnTimerExpired()
{
	bRespawnTimerElapsed = true;
	SetComponentTickEnabled(false);

	UE_LOG(LogRpg, Log, TEXT("RpgRespawnComponent: [%s] respawn timer elapsed. Player can now respawn."), *GetNameSafe(GetOwner()));
}
