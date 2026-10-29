// Fill out your copyright notice in the Description page of Project Settings.

#include "FEBuildWorkbench.h"

#define LOCTEXT_NAMESPACE "FEBuilding"

bool AFEBuildWorkbench::CanInteractBuilt(AActor* InstigatorActor) const
{
	return true;
}

FText AFEBuildWorkbench::GetInteractTextBuilt(AActor* InstigatorActor) const
{
	return LOCTEXT("Craft", "제작");
}

void AFEBuildWorkbench::InteractBuilt(AActor* InstigatorActor)
{
	UE_LOG(LogFEBuilding, Log, TEXT("%s used workbench %s"), *GetNameSafe(InstigatorActor), *GetName());
	OnWorkbenchUsed(InstigatorActor);
}

void AFEBuildWorkbench::InteractBuiltLocal(AActor* InstigatorActor)
{
	OnWorkbenchUsedLocal(InstigatorActor);
}

#undef LOCTEXT_NAMESPACE
