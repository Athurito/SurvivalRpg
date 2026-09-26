#include "Blueprint/RpgEditorPlaytestTools.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputKeyEventArgs.h"

bool URpgEditorPlaytestTools::SetPIEInputKey(APlayerController* Controller, FName KeyName, bool bPressed)
{
	const FKey Key(KeyName);
	if (!IsInGameThread() || !IsValid(Controller) || !Controller->GetWorld()
		|| Controller->GetWorld()->WorldType != EWorldType::PIE || Controller->GetWorld()->bIsTearingDown
		|| !Controller->IsLocalController() || !Controller->GetLocalPlayer() || !Controller->PlayerInput
		|| !Key.IsValid() || Key.IsAnalog())
	{
		return false;
	}
	return Controller->InputKey(FInputKeyEventArgs::CreateSimulated(Key,
		bPressed ? IE_Pressed : IE_Released, bPressed ? 1.0f : 0.0f));
}
