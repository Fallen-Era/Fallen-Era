#include "Combat/Interface/FECombatPresentation.h"

#include "GameFramework/Actor.h"

USkeletalMeshComponent* IFE_CombatPresentation::FindFirstPersonMesh(const AActor* Actor)
{
	const IFE_CombatPresentation* Presentation = Cast<IFE_CombatPresentation>(Actor);
	return Presentation ? Presentation->GetCombatFirstPersonMesh() : nullptr;
}

UCameraComponent* IFE_CombatPresentation::FindCombatCamera(const AActor* Actor)
{
	const IFE_CombatPresentation* Presentation = Cast<IFE_CombatPresentation>(Actor);
	return Presentation ? Presentation->GetCombatCamera() : nullptr;
}
