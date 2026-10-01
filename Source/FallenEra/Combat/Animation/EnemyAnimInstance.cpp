#include "Combat/Animation/EnemyAnimInstance.h"

#include "Combat/Character/EnemyCharacter.h"

void UFE_EnemyAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	EnemyCharacter = Cast<AFE_EnemyCharacter>(TryGetPawnOwner());
	bSettingsCached = false;
	TryCacheAISettings();
}

void UFE_EnemyAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	if (!bSettingsCached)
	{
		if (!EnemyCharacter.IsValid())
		{
			EnemyCharacter = Cast<AFE_EnemyCharacter>(TryGetPawnOwner());
		}
		TryCacheAISettings();
	}
}

void UFE_EnemyAnimInstance::TryCacheAISettings()
{
	if (!EnemyCharacter.IsValid() || !EnemyCharacter->IsAISettingsCached())
	{
		return;
	}

	AISettings = EnemyCharacter->GetCachedAISettings();
	LocomotionBlendSpace = AISettings.LocomotionBlendSpace;
	bSettingsCached = true;
}
