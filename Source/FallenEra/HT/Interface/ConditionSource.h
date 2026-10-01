#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "HT/Condition/CharacterConditionTypes.h"
#include "ConditionSource.generated.h"

UINTERFACE(BlueprintType)
class FALLENERA_API UFE_ConditionSource : public UInterface
{
	GENERATED_BODY()
};

/** Implemented by attackers that can inflict conditions when their damage succeeds. */
class FALLENERA_API IFE_ConditionSource
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="FallenEra|Status")
	TArray<FFE_ConditionApplicationChance> GetConditionApplicationChances() const;
};
