#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "CombatPresentation.generated.h"

class USkeletalMeshComponent;
class AActor;

/** Optional presentation extension; ordinary characters only need their default Mesh. */
UINTERFACE(MinimalAPI, meta=(CannotImplementInterfaceInBlueprint))
class UFE_CombatPresentation : public UInterface
{
	GENERATED_BODY()
};

class FALLENERA_API IFE_CombatPresentation
{
	GENERATED_BODY()

public:
	static USkeletalMeshComponent* FindFirstPersonMesh(const AActor* Actor);
	virtual USkeletalMeshComponent* GetCombatFirstPersonMesh() const = 0;
};
