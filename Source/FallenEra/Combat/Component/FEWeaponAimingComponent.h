#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/Weapon/FEWeaponItemData.h"
#include "FEWeaponAimingComponent.generated.h"

class UFE_EquipmentComponent;
class UFE_WeaponSightData;
class UFE_WeaponScopeWidget;
class UCameraComponent;
class UMeshComponent;
class USkeletalMeshComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnFEAimAlphaChanged, float);

/** Local ADS/charge sway presentation. Ability/Equipment own input, authority and stat changes. */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class FALLENERA_API UFE_WeaponAimingComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UFE_WeaponAimingComponent();
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	bool CanAim() const;
	/** Solves the arms pose while preserving its scale, regardless of optic socket scale. */
	static FTransform CalculateAlignedArmsTransform(const FTransform& SightInArms,
		const FTransform& DesiredSightInParent, const FVector& ArmsScale);
	bool BeginAim();
	void EndAim(bool bImmediate = false);
	/** Release old mesh ownership before equipment destroys/repositions it; retain the local camera blend. */
	void PrepareForEquipmentChange();
	/** Charge-only motion: no optic alignment, FOV changes, or ADS state/stat changes. */
	void BeginChargeSway(UObject* Source, const FFE_WeaponAimSwaySettings& Settings);
	void EndChargeSway(UObject* Source);
	/** Intentional camera-local offset. Do not also add this offset to the same weapon in AnimBP. */
	UFUNCTION(BlueprintPure, Category="Combat|Aim")
	FTransform GetAimSwayTransform() const;
	/** [Owning Client Only] Called per committed firearm shot; never changes ballistic aim. */
	void ApplyShotRecoil(const FFE_WeaponVisualRecoilSettings& Settings, float RecoilControl);
	UFUNCTION(BlueprintPure, Category="Combat|Aim")
	FTransform GetVisualRecoilTransform() const;
	UFUNCTION(BlueprintPure, Category="Combat|Aim")
	float GetAimAlpha() const;
	UFUNCTION(BlueprintPure, Category="Combat|Aim")
	bool IsAiming() const { return bIsAiming; }
	/** Weapon-space grip sockets for optional first-person hand IK. */
	UFUNCTION(BlueprintPure, Category="Combat|Aim")
	FTransform GetWeaponSocketWorldTransform(FName SocketName) const;
	FOnFEAimAlphaChanged& OnAimAlphaChanged() { return AimAlphaChanged; }
private:
	/** Direct component/BP callers do not necessarily own the aim GA's activation tag. */
	TWeakObjectPtr<class UAbilitySystemComponent> DirectAimStateAbilitySystem;
	UPROPERTY(ReplicatedUsing=OnRep_IsAiming)
	bool bIsAiming = false;
	UFUNCTION() void OnRep_IsAiming();
	void RefreshAimView();
	UPROPERTY(Transient)
	TObjectPtr<const UFE_WeaponSightData> Sight;
	UPROPERTY(Transient)
	TObjectPtr<UFE_WeaponScopeWidget> ScopeWidget;
	TWeakObjectPtr<UCameraComponent> Camera;
	TWeakObjectPtr<UMeshComponent> WeaponMesh;
	TWeakObjectPtr<USkeletalMeshComponent> ArmsMesh;
	TWeakObjectPtr<UMeshComponent> SightMesh;
	FTransform OriginalArmsTransform;
	float OriginalFOV = 90.0f;
	float OriginalFirstPersonFOV = 70.0f;
	float AimAlpha = 0.0f;
	FFE_WeaponAimSwaySettings CachedSwaySettings;
	FVector SwayLocation = FVector::ZeroVector;
	FVector SwayLocationVelocity = FVector::ZeroVector;
	FVector SwayRotation = FVector::ZeroVector;
	FVector SwayRotationVelocity = FVector::ZeroVector;
	FRotator PreviousViewRotation = FRotator::ZeroRotator;
	float MovementPhase = 0.0f;
	bool bCapturedPresentation = false;
	bool bDetachedWeaponPresentation = false;
	float CameraBlendStartFOV = 90.0f;
	float CameraBlendStartFirstPersonFOV = 70.0f;
	float CameraBlendElapsed = 0.0f;
	float CameraBlendDuration = 0.0f;
	bool bFirstPersonPresentation = true;
	TWeakObjectPtr<UObject> ChargeSwaySource;
	FFE_WeaponAimSwaySettings ChargeSwaySettings;
	FTransform ChargeArmsTransform;
	bool bChargeSwayCaptured = false;
	bool bOriginalArmsVisible = true;
	bool bOriginalWeaponVisible = true;
	FOnFEAimAlphaChanged AimAlphaChanged;
	FFE_WeaponVisualRecoilSettings CachedVisualRecoilSettings;
	FVector VisualRecoilLocation = FVector::ZeroVector;
	FVector VisualRecoilLocationVelocity = FVector::ZeroVector;
	FVector VisualRecoilRotation = FVector::ZeroVector;
	FVector VisualRecoilRotationVelocity = FVector::ZeroVector;
	bool IsLocallyControlled() const;
	void UpdatePresentation();
	void UpdateSway(float DeltaTime);
	void ResetSway();
	void UpdateVisualRecoil(float DeltaTime);
	void ResetVisualRecoil();
	void RestorePresentation();
	void ReleaseMeshPresentation();
	void StartCameraBlend(float Duration);
	void StartChargeSwayPresentation();
	void StopChargeSwayPresentation();
};
