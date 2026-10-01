#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "MeleeTraceTask.generated.h"

class UGameplayAbility;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnFEMeleeTraceTick);

/** Ticks once per frame while a melee hit window is active. */
UCLASS()
class FALLENERA_API UFE_MeleeTraceTask : public UAbilityTask
{
	GENERATED_BODY()

public:
	UFE_MeleeTraceTask();

	UFUNCTION(BlueprintCallable, Category="Ability|Tasks", meta=(HidePin="OwningAbility", DefaultToSelf="OwningAbility", BlueprintInternalUseOnly="TRUE"))
	static UFE_MeleeTraceTask* StartMeleeTrace(UGameplayAbility* OwningAbility);

	UPROPERTY(BlueprintAssignable)
	FOnFEMeleeTraceTick OnTraceTick;

protected:
	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;
};
