// Fill out your copyright notice in the Description page of Project Settings.


#include "WeaponManager.h"


UWeaponManager::UWeaponManager()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}


// Called when the game starts
void UWeaponManager::BeginPlay()
{
	Super::BeginPlay();
}
