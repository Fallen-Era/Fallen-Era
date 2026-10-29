// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "WorldSaveSubsystem.generated.h"

class UWorldPersistenceTracker;
/**
 * 
 */
UCLASS()
class FALLENERA_API UWorldSaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
public:
	
	
private:
	UPROPERTY(Transient)
	TObjectPtr<UWorldPersistenceTracker> ActiveTracker;
	

};
