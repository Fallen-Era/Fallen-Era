#include "Combat/Spawn/FEMonsterSpawnAnchor.h"

#include "Components/SceneComponent.h"
#include "Combat/Spawn/FEMonsterSpawnSubsystem.h"

AFE_MonsterSpawnAnchor::AFE_MonsterSpawnAnchor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void AFE_MonsterSpawnAnchor::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority() && bSpawnEnabled)
	{
		if (UFE_MonsterSpawnSubsystem* SpawnSubsystem = GetWorld()->GetSubsystem<UFE_MonsterSpawnSubsystem>())
		{
			SpawnSubsystem->RegisterSpawnAnchor(this);
		}
	}
}

void AFE_MonsterSpawnAnchor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UFE_MonsterSpawnSubsystem* SpawnSubsystem = World->GetSubsystem<UFE_MonsterSpawnSubsystem>())
		{
			SpawnSubsystem->UnregisterSpawnAnchor(this);
		}
	}
	Super::EndPlay(EndPlayReason);
}
