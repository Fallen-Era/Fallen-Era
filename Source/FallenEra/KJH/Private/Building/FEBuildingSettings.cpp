// Fallen Era 건설 시스템 (KJH)

#include "Building/FEBuildingSettings.h"

UFEBuildingSettings::UFEBuildingSettings()
{
	CategoryName = TEXT("Game");
	SectionName = TEXT("Building");
}

const UFEBuildingSettings* UFEBuildingSettings::Get()
{
	return GetDefault<UFEBuildingSettings>();
}

int32 UFEBuildingSettings::GetYawStepCount() const
{
	return FMath::Max(1, FMath::RoundToInt(360.f / RotationStepDeg));
}
