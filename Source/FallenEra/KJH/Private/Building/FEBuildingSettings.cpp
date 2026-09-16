// Fill out your copyright notice in the Description page of Project Settings.

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
