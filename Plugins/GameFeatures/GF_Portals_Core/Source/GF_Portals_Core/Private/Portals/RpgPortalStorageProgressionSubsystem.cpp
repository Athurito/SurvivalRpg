#include "Portals/RpgPortalStorageProgressionSubsystem.h"

#include "Engine/World.h"
#include "GameplayTags/RpgPortalGameplayTags.h"
#include "Portals/RpgPortalMessages.h"
#include "Portals/RpgPortalStorageProgressionHook.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RpgPortalStorageProgressionSubsystem)

void URpgPortalStorageProgressionSubsystem::Initialize(
	FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Loaded worlds normally have their GameInstance already assigned here. The
	// begin-play retry also covers minimal/standalone worlds that attach it later.
	TryRegisterPortalCompletedListener();
}

void URpgPortalStorageProgressionSubsystem::Deinitialize()
{
	if (PortalCompletedListenerHandle.IsValid())
	{
		PortalCompletedListenerHandle.Unregister();
		PortalCompletedListenerHandle = FGameplayMessageListenerHandle();
	}

	Super::Deinitialize();
}

void URpgPortalStorageProgressionSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	TryRegisterPortalCompletedListener();
}

bool URpgPortalStorageProgressionSubsystem::DoesSupportWorldType(
	EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void URpgPortalStorageProgressionSubsystem::TryRegisterPortalCompletedListener()
{
	UWorld* World = GetWorld();
	if (PortalCompletedListenerHandle.IsValid() || !World ||
		World->GetNetMode() == NM_Client ||
		!UGameplayMessageSubsystem::HasInstance(World))
	{
		return;
	}

	UGameplayMessageSubsystem& MessageSubsystem =
		UGameplayMessageSubsystem::Get(World);
	PortalCompletedListenerHandle =
		MessageSubsystem.RegisterListener<FRpgPortalCompletedMessage>(
			RpgPortalGameplayTags::Rpg_Portal_Message_Completed,
			this,
			&ThisClass::HandlePortalCompleted);
}

void URpgPortalStorageProgressionSubsystem::HandlePortalCompleted(
	FGameplayTag Channel,
	const FRpgPortalCompletedMessage& Message)
{
	if (Channel != RpgPortalGameplayTags::Rpg_Portal_Message_Completed)
	{
		return;
	}

	// GameplayMessage payloads are immutable to listeners. The portal actor may
	// still invoke the same hook before broadcasting when it needs populated
	// result fields; the copied listener path is the generic progression fallback.
	FRpgPortalCompletedMessage ProgressionMessage = Message;
	FRpgPortalStorageProgressionHook::ApplyFirstEligibleCompletion(
		GetWorld(),
		ProgressionMessage);
}
