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
	void BeginSession()
	{
		SessionId = FGuid::NewGuid();
		CurrentRevision = 0;
		SavedRevision = 0;
	}
	
	void MarkDirty() { ++CurrentRevision; }
	
	bool IsDirty() const { return CurrentRevision != SavedRevision; }
	
	uint64 GetCurrentRevision() const { return CurrentRevision; }
	FGuid GetWorldId() { return RegistryData.WorldId; }
	FString& GetSlotName() {return RegistryData.ProfileSlotName; }
	FGuid GetSessionId() { return SessionId; }
	
	void AcknowledgeSaved(uint64 Revision)
	{
		if (Revision <= CurrentRevision)
		{
			SavedRevision = FMath::Max(SavedRevision, Revision);
		}
	}
	
	
	
protected:
	void SetWorldId(FGuid NewWorldId) { RegistryData.WorldId = NewWorldId; }
	void SetSessionId(FGuid NewSessionId) { SessionId = NewSessionId; }
	void SetSlotName(FString NewSlotName) { RegistryData.ProfileSlotName = NewSlotName; }
	
	FWorldRegistryData& GetRegistryData() { return RegistryData; }
	
	
	friend class UWorldRegistrySubsystem;
	
private:
	
	FWorldRegistryData RegistryData;
	FGuid SessionId;
	
	uint64 CurrentRevision = 0;
	uint64 SavedRevision = 0;
};
