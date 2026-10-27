#include "Combat/Interface/FECombatPresentation.h"

#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "Camera/CameraComponent.h"
#include "Combat/Interface/FEDamageable.h"
#include "Engine/World.h"

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

bool IFE_CombatPresentation::UsesFirstPersonView(const AActor* Actor)
{
	const IFE_CombatPresentation* Presentation = Cast<IFE_CombatPresentation>(Actor);
	return Presentation && Presentation->IsCombatFirstPersonView();
}

bool IFE_CombatPresentation::UsesFirstPersonMeshes(const AActor* Actor)
{
	const IFE_CombatPresentation* Presentation = Cast<IFE_CombatPresentation>(Actor);
	return Presentation && Presentation->AreCombatFirstPersonMeshesVisible();
}

void IFE_CombatPresentation::GetCombatTraceView(const ACharacter* Character, FVector& OutLocation, FRotator& OutRotation)
{
	OutLocation = Character ? Character->GetPawnViewLocation() : FVector::ZeroVector;
	OutRotation = Character ? Character->GetBaseAimRotation() : FRotator::ZeroRotator;
	if (!Character) { return; }
	if (const AController* Controller = Character->GetController())
	{
		Controller->GetPlayerViewPoint(OutLocation, OutRotation);
	}
	if (const IFE_CombatPresentation* Presentation = Cast<IFE_CombatPresentation>(Character))
	{
		if (const UCameraComponent* TraceCamera = Presentation->GetCombatTraceCamera())
		{
			// Inactive FP cameras may have stale rotation in TP. Only use their world location.
			OutLocation = TraceCamera->GetComponentLocation();
		}
	}
}

FFE_CombatAim IFE_CombatPresentation::ResolveCombatAim(const ACharacter* Character, float Range, ECollisionChannel Channel)
{
	FFE_CombatAim Aim;
	FRotator ViewRotation;
	GetCombatTraceView(Character, Aim.Origin, ViewRotation);
	Aim.Direction = ViewRotation.Vector();
	Range = FMath::IsFinite(Range) ? FMath::Max(0.0f, Range) : 0.0f;
	Aim.CameraStart = Aim.Origin;
	Aim.CameraEnd = Aim.Origin + Aim.Direction * Range;
	Aim.AimPoint = Aim.CameraEnd;
	const IFE_CombatPresentation* Presentation = Cast<IFE_CombatPresentation>(Character);
	const UCameraComponent* Camera = Presentation ? Presentation->GetCombatCamera() : nullptr;
	if (!Camera || Presentation->IsCombatFirstPersonView() || !Character->GetWorld() || Range <= KINDA_SMALL_NUMBER)
	{
		return Aim;
	}
	Aim.CameraStart = Camera->GetComponentLocation();
	// The boom length must not consume weapon reach, especially for short melee attacks.
	const float CameraBehindOrigin = FMath::Max(0.0f, FVector::DotProduct(Aim.Origin - Aim.CameraStart, Aim.Direction));
	Aim.CameraEnd = Aim.CameraStart + Aim.Direction * (Range + CameraBehindOrigin);
	Aim.AimPoint = Aim.CameraEnd;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(FE_ThirdPersonAim), true, Character);
	TArray<FHitResult> Hits;
	Character->GetWorld()->LineTraceMultiByChannel(Hits, Aim.CameraStart, Aim.CameraEnd, Channel, Query);
	for (const FHitResult& Hit : Hits)
	{
		const bool bDamageable = Hit.GetActor() && Hit.GetActor()->GetClass()->ImplementsInterface(UFE_Damageable::StaticClass());
		const bool bInFront = !Hit.ImpactPoint.ContainsNaN() &&
			FVector::DotProduct(Hit.ImpactPoint - Aim.Origin, Aim.Direction) > KINDA_SMALL_NUMBER;
		if ((Hit.bBlockingHit || bDamageable) && !Hit.bStartPenetrating && bInFront)
		{
			Aim.AimPoint = Hit.ImpactPoint;
			Aim.bHasCameraTarget = true;
			break;
		}
		if (Hit.bBlockingHit) { break; }
	}
	Aim.Direction = (Aim.AimPoint - Aim.Origin).GetSafeNormal(SMALL_NUMBER, Aim.Direction);
	return Aim;
}
