// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"

#include "RpgActivatableWidget.generated.h"

struct FUIInputConfig;
class UWidgetTree;

UENUM(BlueprintType)
enum class ERpgWidgetInputMode : uint8
{
	Default,
	GameAndMenu,
	Game,
	Menu
};

/**
 * 
 */
UCLASS(Abstract, Blueprintable)
class SURVIVALRPG_API URpgActivatableWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()
	
public:
	explicit URpgActivatableWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

#if WITH_EDITOR
	virtual void ValidateCompiledWidgetTree(const UWidgetTree& BlueprintWidgetTree, class IWidgetCompilerLog& CompileLog) const override;
#endif

protected:
	UPROPERTY(EditDefaultsOnly, Category = Input)
	ERpgWidgetInputMode InputConfig = ERpgWidgetInputMode::Default;

	UPROPERTY(EditDefaultsOnly, Category = Input)
	EMouseCaptureMode GameMouseCaptureMode = EMouseCaptureMode::CapturePermanently;
};
