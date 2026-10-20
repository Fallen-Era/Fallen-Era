#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "UObject/Interface.h"
#include "FECombatPresentation.generated.h"

class USkeletalMeshComponent;
class UCameraComponent;
class AActor;
class ACharacter;

/** Camera selection is query-only. Damage must still trace from Origin along Direction. */
struct FALLENERA_API FFE_CombatAim
{
	FVector Origin = FVector::ZeroVector;
	FVector Direction = FVector::ForwardVector;
	FVector CameraStart = FVector::ZeroVector;
	FVector CameraEnd = FVector::ZeroVector;
	FVector AimPoint = FVector::ZeroVector;
	bool bHasCameraTarget = false;
};

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
	static UCameraComponent* FindCombatCamera(const AActor* Actor);
	/** Attack origin is independent of the active presentation camera; aim rotation remains view-driven. */
	static void GetCombatTraceView(const ACharacter* Character, FVector& OutLocation, FRotator& OutRotation);
	/** Resolves TP center parallax once per attack; FP keeps its existing straight trace without an extra query. */
	static FFE_CombatAim ResolveCombatAim(const ACharacter* Character, float Range, ECollisionChannel Channel);
	static bool UsesFirstPersonView(const AActor* Actor);
	/** Visible weapon/mesh selection can lag the aiming camera while ADS is blending in. */
	static bool UsesFirstPersonMeshes(const AActor* Actor);
	virtual USkeletalMeshComponent* GetCombatFirstPersonMesh() const = 0;
	virtual UCameraComponent* GetCombatCamera() const = 0;
	virtual UCameraComponent* GetCombatTraceCamera() const { return GetCombatCamera(); }
	virtual bool IsCombatFirstPersonView() const { return true; }
	virtual bool AreCombatFirstPersonMeshesVisible() const { return IsCombatFirstPersonView(); }
	/** Temporary ADS override; does not change the player's preferred exploration perspective. */
	virtual void SetCombatAimView(bool bAiming, float BlendTime) {}
};
