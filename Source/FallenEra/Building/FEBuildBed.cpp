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
	DOREPLIFETIME(AFEBuildBed, OwnerPlayerState);
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

void AFEBuildBed::SetOwnerPlayerState(APlayerState* NewOwner)
{
	if (HasAuthority())
	{
		OwnerPlayerState = NewOwner;
	}
}

APlayerState* AFEBuildBed::GetOwnerPlayerState() const
{
	return OwnerPlayerState;
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
	const bool bIsMine = OwnerPlayerState != nullptr && OwnerPlayerState == GetPlayerStateOf(InstigatorActor);
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

#undef LOCTEXT_NAMESPACE
