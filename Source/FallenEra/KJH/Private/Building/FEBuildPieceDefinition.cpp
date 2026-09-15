// Fill out your copyright notice in the Description page of Project Settings.

#include "Building/FEBuildPieceDefinition.h"

const FPrimaryAssetType UFEBuildPieceDefinition::AssetType = FName(TEXT("BuildPiece"));
const FName UFEBuildPieceDefinition::RuntimeBundle = FName(TEXT("Runtime"));

FPrimaryAssetId UFEBuildPieceDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(AssetType, GetFName());
}
