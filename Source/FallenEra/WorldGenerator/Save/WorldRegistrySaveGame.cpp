// Fill out your copyright notice in the Description page of Project Settings.


#include "WorldRegistrySaveGame.h"

#include "WorldGenerator/WorldPersistenceTracker.h"

bool UWorldRegistrySaveGame::Find(FString DisplayName)
{
	for (auto& World : Worlds)
	{
		if (World.DisplayName.Equals(DisplayName))
		{
			UE_LOG(LogTemp, Log, TEXT("%s::%s : Found World."), *GetClass()->GetName(), TEXT(__FUNCTION__));
			return true;
		}
	}
	
	return false;
}

void UWorldRegistrySaveGame::AddRegistry(FWorldRegistryData& NewEntry)
{
	Worlds.Add(NewEntry);
}

void UWorldRegistrySaveGame::RemoveRegistry(FWorldRegistryData& NewEntry)
{
}
