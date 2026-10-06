// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WorldVoxelPersistenceBridge.generated.h"

/**
 * 
 */
UCLASS()
class FALLENERA_API UWorldVoxelPersistenceBridge : public UWorldSubsystem
{
	GENERATED_BODY()
	
	
public:
	TWeakObjectPtr<UWorldPersistenceTracker> Tracker;
};
