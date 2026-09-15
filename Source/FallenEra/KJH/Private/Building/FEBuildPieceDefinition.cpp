// Fallen Era 건설 시스템 (KJH)

#include "Building/FEBuildPieceDefinition.h"

const FPrimaryAssetType UFEBuildPieceDefinition::AssetType = FName(TEXT("BuildPiece"));
const FName UFEBuildPieceDefinition::RuntimeBundle = FName(TEXT("Runtime"));

FPrimaryAssetId UFEBuildPieceDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(AssetType, GetFName());
}
