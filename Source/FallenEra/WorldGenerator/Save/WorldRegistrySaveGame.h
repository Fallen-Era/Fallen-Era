// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "WorldGenerator/Data/Payload.h"
#include "WorldRegistrySaveGame.generated.h"

/**
 * 
 */
UCLASS()
class FALLENERA_API UWorldRegistrySaveGame : public USaveGame
{
	GENERATED_BODY()
	
	
public:
	int32 Find(const FString& DisplayName) const;
	int32 Find(const FGuid& WorldId) const;
	TArray<FWorldRegistryData>& GetWorldRegistryList() { return Worlds; }
	
protected:
	void AddRegistry(FWorldRegistryData& NewEntry);
	void RemoveRegistry(int32 RemoveIndex);
	
	
friend class UWorldRegistrySubsystem;
	
private:
	UPROPERTY()
	TArray<FWorldRegistryData> Worlds;
};
