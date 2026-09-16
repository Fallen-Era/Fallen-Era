#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"
#include "AnimNotify_SendGameplayEvent.generated.h"

/** Sends a Gameplay Event to the owning actor when an animation notify is reached. */
UCLASS(Blueprintable)
class FALLENERA_API UFE_AnimNotify_SendGameplayEvent : public UAnimNotify
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gameplay Event", meta=(Categories="GameplayEvent"))
	FGameplayTag EventTag;

	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;
};
