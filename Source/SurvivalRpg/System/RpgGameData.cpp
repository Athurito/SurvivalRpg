// Fill out your copyright notice in the Description page of Project Settings.

#include "RpgGameData.h"

#include "RpgAssetManager.h"

URpgGameData::URpgGameData()
{
}

const URpgGameData& URpgGameData::Get()
{
	return URpgAssetManager::Get().GetGameData();
}
