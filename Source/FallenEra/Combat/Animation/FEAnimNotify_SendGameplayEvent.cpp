#include "Combat/Animation/FEAnimNotify_SendGameplayEvent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Combat/Interface/FECombatPresentation.h"

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
	// The first-person montage mirrors presentation, not gameplay timing. Do not send the same event twice.
	const ACharacter* Character = Cast<ACharacter>(OwnerActor);
	if (Character && MeshComp != Character->GetMesh() && MeshComp == IFE_CombatPresentation::FindFirstPersonMesh(OwnerActor))
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
