// Fill out your copyright notice in the Description page of Project Settings.

#include "FEBuildPieceDefinition.h"

const FPrimaryAssetType UFEBuildPieceDefinition::AssetType = FName(TEXT("BuildPiece"));
const FName UFEBuildPieceDefinition::RuntimeBundle = FName(TEXT("Runtime"));

FPrimaryAssetId UFEBuildPieceDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(AssetType, GetFName());
}

void UFEBuildPieceDefinition::GetMaterialChain(TArray<const UFEBuildPieceDefinition*>& OutChain) const
{
	OutChain.Reset();

	// 자신부터 UpgradeTo 를 따라간다. 실수로 순환(철 → 나무)을 연결해도 이미 본 정의에서 멈춘다.
	// 로드하지 않는다 — UFEBuildingSubsystem::OnWorldBeginPlay 가 BuildPiece DA 를 전부 미리 받아 둔다
	for (const UFEBuildPieceDefinition* Current = this; Current != nullptr && !OutChain.Contains(Current); Current = Current->UpgradeTo.Get())
	{
		OutChain.Add(Current);
	}
}
