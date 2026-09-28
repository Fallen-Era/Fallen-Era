// Fill out your copyright notice in the Description page of Project Settings.

#include "FEBuildBed.h"
#include "FEBuildingSubsystem.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"

#define LOCTEXT_NAMESPACE "FEBuilding"

void AFEBuildBed::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AFEBuildBed, OwnerPlayerId);
}

void AFEBuildBed::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		if (UFEBuildingSubsystem* Subsystem = UFEBuildingSubsystem::Get(GetWorld()))
		{
			Subsystem->ClearRespawnBed(this);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void AFEBuildBed::SetOwnerPlayerId(const FString& NewOwnerId)
{
	if (HasAuthority())
	{
		OwnerPlayerId = NewOwnerId;
	}
}

const FString& AFEBuildBed::GetOwnerPlayerId() const
{
	return OwnerPlayerId;
}

APlayerState* AFEBuildBed::GetPlayerStateOf(const AActor* Actor)
{
	const APawn* Pawn = Cast<APawn>(Actor);
	return Pawn ? Pawn->GetPlayerState() : nullptr;
}

bool AFEBuildBed::CanInteractBuilt(AActor* InstigatorActor) const
{
	return GetPlayerStateOf(InstigatorActor) != nullptr;
}

FText AFEBuildBed::GetInteractTextBuilt(AActor* InstigatorActor) const
{
	const FString MyId = UFEBuildingSubsystem::GetStablePlayerId(GetPlayerStateOf(InstigatorActor));
	const bool bIsMine = !MyId.IsEmpty() && MyId == OwnerPlayerId;
	
	return bIsMine ? LOCTEXT("BedMine", "내 스폰 지점") : LOCTEXT("BedSet", "스폰 지점으로 설정");
}

void AFEBuildBed::InteractBuilt(AActor* InstigatorActor)
{
	APlayerState* PlayerState = GetPlayerStateOf(InstigatorActor);
	UFEBuildingSubsystem* Subsystem = UFEBuildingSubsystem::Get(GetWorld());
	if (PlayerState && Subsystem)
	{
		Subsystem->SetRespawnBed(PlayerState, this);
	}
}

void AFEBuildBed::WriteRecord(FFEBuildPieceRecord& OutRecord) const
{
	Super::WriteRecord(OutRecord);
	OutRecord.OwnerId = OwnerPlayerId;
}

void AFEBuildBed::ReadRecord(const FFEBuildPieceRecord& Record)
{
	Super::ReadRecord(Record);
	OwnerPlayerId = Record.OwnerId;
}

#undef LOCTEXT_NAMESPACE
