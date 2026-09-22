#include "HT/Interface/CombatPresentation.h"

#include "GameFramework/Actor.h"

USkeletalMeshComponent* IFE_CombatPresentation::FindFirstPersonMesh(const AActor* Actor)
{
	const IFE_CombatPresentation* Presentation = Cast<IFE_CombatPresentation>(Actor);
	return Presentation ? Presentation->GetCombatFirstPersonMesh() : nullptr;
}
