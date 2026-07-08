// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "View/MVVMViewModelContextResolver.h"

#include "PlayerVitalsResolver.generated.h"

/**
 * 
 */
class ULocalPlayer;

UCLASS()
class SURVIVALRPG_API UPlayerVitalsResolver : public UMVVMViewModelContextResolver
{
	GENERATED_BODY()
	
public:
	virtual UObject* CreateInstance(const UClass* ExpectedType, const UUserWidget* UserWidget, const UMVVMView* View) const override;
	virtual void DestroyInstance(UObject* ViewModel, const UMVVMView* View) const override {}

private:
	ULocalPlayer* ResolveLocalPlayer(const UUserWidget* UserWidget) const;
};
