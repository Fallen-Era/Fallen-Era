#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "FEEncounterTarget.generated.h"

/** Spawn systems issue aggro requests without depending on a particular AI controller. */
UINTERFACE(MinimalAPI, meta=(CannotImplementInterfaceInBlueprint))
class UFE_EncounterTarget : public UInterface
{
	GENERATED_BODY()
};

class FALLENERA_API IFE_EncounterTarget
{
	GENERATED_BODY()
public:
	virtual bool SetEncounterTarget(AActor* TargetActor) = 0;
};
