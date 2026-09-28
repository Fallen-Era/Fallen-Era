// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeCategories.h"
#include "UObject/Object.h"
#include "Data/Payload.h"
#include "WorldPersistenceTracker.generated.h"

/**
 * 
 */
UCLASS()
class FALLENERA_API UWorldPersistenceTracker : public UObject
{
	GENERATED_BODY()
	
public:
	void MarkDirty(){ ++CurrentRevision; }
	
	bool IsDirty() const { return CurrentRevision != SavedRevision; }
	
	void AcknowledgeSaved(uint64 Revision){ SavedRevision = FMath::Max(SavedRevision, Revision); }
	
	FGuid GetWorldId() { return WorldId; }
	FGuid GetSessionId() { return SessionId; }
	
protected:
	void SetWorldId(FGuid NewWorldId) { WorldId = NewWorldId; }
	void SetSessionId(FGuid NewSessionId) { SessionId = NewSessionId; }
	
private:
	FGuid WorldId;
	FGuid SessionId;
	
	uint64 CurrentRevision = 0;
	uint64 SavedRevision = 0;
};
