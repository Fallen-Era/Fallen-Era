#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "ImpactGameplayCue.generated.h"

/**
 * One burst cue for all weapon impact presentation.
 * Create a Blueprint child under the configured GameplayCueNotifyPath so the
 * GameplayCueManager can discover and register the native handler.
 */
UCLASS(Blueprintable)
class FALLENERA_API UFE_ImpactGameplayCue : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

public:
	UFE_ImpactGameplayCue();

	virtual bool OnExecute_Implementation(
		AActor* MyTarget,
		const FGameplayCueParameters& Parameters) const override;
};
