#include "HT/Animation/AnimNotify_SendGameplayEvent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "GameFramework/Actor.h"

void UFE_AnimNotify_SendGameplayEvent::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	(void)Animation;
	(void)EventReference;

	if (!MeshComp || !EventTag.IsValid())
	{
		return;
	}

	AActor* OwnerActor = MeshComp->GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	FGameplayEventData Payload;
	Payload.EventTag = EventTag;
	Payload.Instigator = OwnerActor;
	Payload.Target = OwnerActor;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(OwnerActor, EventTag, Payload);
}

FString UFE_AnimNotify_SendGameplayEvent::GetNotifyName_Implementation() const
{
	return EventTag.IsValid()
		? FString::Printf(TEXT("Send Gameplay Event: %s"), *EventTag.ToString())
		: TEXT("Send Gameplay Event");
}
