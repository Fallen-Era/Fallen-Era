#pragma once

#include "Combat/Character/FEEnemyCharacter.h"
#include "GameplayEffect.h"
#include "FEEnemySettingsTestCharacter.generated.h"

/** Concrete native fixture for the abstract enemy base; no content assets required. */
UCLASS(NotBlueprintable, Transient)
class AFE_EnemySettingsTestCharacter : public AFE_EnemyCharacter
{
	GENERATED_BODY()

public:
	AFE_EnemySettingsTestCharacter();
	void ConfigurePreset(UFE_AISettingsDataAsset* Preset);
};

UCLASS(NotBlueprintable, Transient)
class UFE_EnemySettingsTestEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UFE_EnemySettingsTestEffect();
};
