// Fill out your copyright notice in the Description page of Project Settings.

#include "Building/FEBuildingSettings.h"

UFEBuildingSettings::UFEBuildingSettings()
{
	CategoryName = TEXT("Game");
	SectionName = TEXT("Building");

	MaxSupportDistance.Add(EFEBuildMaterial::Wood, 4);
	MaxSupportDistance.Add(EFEBuildMaterial::Stone, 6);
	MaxSupportDistance.Add(EFEBuildMaterial::Metal, 8);
}

const UFEBuildingSettings* UFEBuildingSettings::Get()
{
	return GetDefault<UFEBuildingSettings>();
}

int32 UFEBuildingSettings::GetYawStepCount() const
{
	return FMath::Max(1, FMath::RoundToInt(360.f / RotationStepDeg));
}

int32 UFEBuildingSettings::GetMaxSupportDistance(EFEBuildMaterial Material) const
{
	if (const int32* Found = MaxSupportDistance.Find(Material))
	{
		return *Found;
	}
	const int32* Wood = MaxSupportDistance.Find(EFEBuildMaterial::Wood);
	return Wood ? *Wood : 4;
}
