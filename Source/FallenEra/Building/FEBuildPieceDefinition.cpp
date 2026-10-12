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
	// 다음 단계 DA 는 동기 로드 — 소프트 참조만 가진 작은 데이터라 메시·머티리얼은 따라오지 않는다
	for (const UFEBuildPieceDefinition* Current = this; Current != nullptr && !OutChain.Contains(Current); Current = Current->UpgradeTo.LoadSynchronous())
	{
		OutChain.Add(Current);
	}
}
