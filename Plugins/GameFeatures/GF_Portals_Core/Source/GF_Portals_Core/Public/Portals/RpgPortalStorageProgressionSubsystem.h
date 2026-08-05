#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Subsystems/WorldSubsystem.h"

#include "RpgPortalStorageProgressionSubsystem.generated.h"

struct FRpgPortalCompletedMessage;

/**
 * World-lifetime bridge from the generic portal-completion message to shared
 * storage progression.
 *
 * The subsystem listens only in game and PIE worlds. Mutations remain
 * server-authoritative and idempotent inside the storage-progression hook;
 * clients never register a progression listener.
 */
UCLASS()
class GF_PORTALS_CORE_API URpgPortalStorageProgressionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

protected:
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

private:
	void TryRegisterPortalCompletedListener();
	void HandlePortalCompleted(
		FGameplayTag Channel,
		const FRpgPortalCompletedMessage& Message);

	/** Registration owned for this world's lifetime and released during subsystem teardown. */
	FGameplayMessageListenerHandle PortalCompletedListenerHandle;
};
