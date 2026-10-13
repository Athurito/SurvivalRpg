#include "Blueprint/RpgEditorPlaytestTools.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputKeyEventArgs.h"

namespace RpgEditorPlaytestTools
{
	bool CanSendPIEInput(const APlayerController* Controller)
	{
		return IsInGameThread() && IsValid(Controller) && Controller->GetWorld()
			&& Controller->GetWorld()->WorldType == EWorldType::PIE && !Controller->GetWorld()->bIsTearingDown
			&& Controller->IsLocalController() && Controller->GetLocalPlayer() && Controller->PlayerInput;
	}
}

bool URpgEditorPlaytestTools::SetPIEInputKey(APlayerController* Controller, FName KeyName, bool bPressed)
{
	const FKey Key(KeyName);
	if (!RpgEditorPlaytestTools::CanSendPIEInput(Controller) || !Key.IsValid() || Key.IsAnalog())
	{
		return false;
	}
	return Controller->InputKey(FInputKeyEventArgs::CreateSimulated(Key,
		bPressed ? IE_Pressed : IE_Released, bPressed ? 1.0f : 0.0f));
}

bool URpgEditorPlaytestTools::SetPIEInputAxis(APlayerController* Controller, FName KeyName, float Value)
{
	const FKey Key(KeyName);
	if (!RpgEditorPlaytestTools::CanSendPIEInput(Controller) || !Key.IsValid() || !Key.IsAnalog()
		|| !FMath::IsFinite(Value))
	{
		return false;
	}
	return Controller->InputKey(FInputKeyEventArgs::CreateSimulated(Key, IE_Axis, FMath::Clamp(Value, -1.f, 1.f), 1));
}
