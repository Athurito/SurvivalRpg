// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "IActorIndicatorWidget.generated.h"

class UIndicatorDescriptor;

/**
 * Implemented by projected indicator widgets that need the descriptor's data object.
 */
UINTERFACE(MinimalAPI, BlueprintType)
class UIndicatorWidgetInterface : public UInterface
{
	GENERATED_BODY()
};

class IIndicatorWidgetInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, Category = "Indicator")
	void BindIndicator(UIndicatorDescriptor* Indicator);

	UFUNCTION(BlueprintNativeEvent, Category = "Indicator")
	void UnbindIndicator(const UIndicatorDescriptor* Indicator);
};
