// Fill out your copyright notice in the Description page of Project Settings.


#include "WorldRegistrySaveGame.h"

#include "WorldGenerator/WorldPersistenceTracker.h"

int32 UWorldRegistrySaveGame::Find(const FString& DisplayName) const
{
	for (int32 Index = 0; Index < Worlds.Num(); ++Index)
	{
		const FWorldRegistryData& World = Worlds[Index];
		if (World.DisplayName.Equals(DisplayName))
		{
			UE_LOG(LogTemp, Log, TEXT("%s::%s : Found world '%s' at index %d (WorldId=%s)."),
				*GetClass()->GetName(), TEXT(__FUNCTION__), *World.DisplayName, Index, *World.WorldId.ToString());
			return Index;
		}
	}
	
	return INDEX_NONE;
}

int32 UWorldRegistrySaveGame::Find(const FGuid& WorldId) const
{
	for (int32 Index = 0; Index < Worlds.Num(); ++Index)
	{
		const FWorldRegistryData& World = Worlds[Index];
		if (World.WorldId == WorldId)
		{
			UE_LOG(LogTemp, Log, TEXT("%s::%s : Found world '%s' at index %d (WorldId=%s)."),
				*GetClass()->GetName(), TEXT(__FUNCTION__), *World.DisplayName, Index, *World.WorldId.ToString());
			return Index;
		}
	}
	
	return INDEX_NONE;
}

void UWorldRegistrySaveGame::AddRegistry(FWorldRegistryData& NewEntry)
{
	Worlds.Add(NewEntry);
}

void UWorldRegistrySaveGame::RemoveRegistry(int32 RemoveIndex)
{
	if (Worlds.IsValidIndex(RemoveIndex))
	{
		Worlds.RemoveAt(RemoveIndex);
	}
}
