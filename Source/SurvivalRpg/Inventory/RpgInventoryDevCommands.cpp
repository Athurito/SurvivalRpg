#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "SurvivalRpg/Core/Player/RpgPlayerState.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemDefinition.h"
#include "SurvivalRpg/Inventory/RpgInventoryItemInstance.h"
#include "SurvivalRpg/Inventory/RpgInventoryManagerComponent.h"
#include "SurvivalRpg/SurvivalRpg.h"

namespace RpgInventoryDevCommands
{
	void GrantItem(const TArray<FString>& Args, UWorld* World)
	{
		if (!World || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogRpg, Warning, TEXT("Rpg.Inventory.GrantItem runs on the server; use the host's window."));
			return;
		}
		if (Args.Num() < 1)
		{
			UE_LOG(LogRpg, Warning, TEXT("Usage: Rpg.Inventory.GrantItem <ItemDefinitionClassPath> [Count]"));
			return;
		}

		UClass* ItemClass = LoadClass<URpgInventoryItemDefinition>(nullptr, *Args[0]);
		const int32 Count = Args.IsValidIndex(1) ? FMath::Max(1, FCString::Atoi(*Args[1])) : 1;
		const APlayerController* PlayerController = World->GetFirstPlayerController();
		const ARpgPlayerState* PlayerState = PlayerController ? PlayerController->GetPlayerState<ARpgPlayerState>() : nullptr;
		URpgInventoryManagerComponent* Inventory = PlayerState ? PlayerState->GetInventoryManagerComponent() : nullptr;
		if (!ItemClass || !Inventory)
		{
			UE_LOG(LogRpg, Warning, TEXT("Rpg.Inventory.GrantItem needs an item definition class (%s) and the host player's inventory."), *Args[0]);
			return;
		}

		// The ordinary authority grant: it places the items like a pickup and fails when they do not fit.
		const URpgInventoryItemInstance* Granted = Inventory->GrantItemDefinition(ItemClass, Count);
		UE_LOG(LogRpg, Log, TEXT("Rpg.Inventory.GrantItem: %s x%d %s."), *GetNameSafe(ItemClass), Count,
			Granted ? TEXT("granted") : TEXT("did not fit"));
	}

	static FAutoConsoleCommandWithWorldAndArgs GrantItemCommand(
		TEXT("Rpg.Inventory.GrantItem"),
		TEXT("Server: grants items to the first local player's inventory, for playtests. ")
		TEXT("Args: <ItemDefinitionClassPath> [Count, default 1]."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&GrantItem));
}

#endif
