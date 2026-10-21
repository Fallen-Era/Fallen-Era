#pragma once

#include "CoreMinimal.h"
#include "Combat/Weapon/Attachment/FEWeaponAttachmentData.h"
#include "FEWeaponSightData.generated.h"

class UTexture2D;
class UFE_WeaponScopeWidget;

UENUM(BlueprintType)
enum class EFE_WeaponSightMode : uint8
{
	IronSights UMETA(DisplayName="기계식 조준기 (3D)"),
	RedDot UMETA(DisplayName="레드닷 (3D)"),
	Scope2D UMETA(DisplayName="전체 화면 스코프 (2D)")
};

UCLASS(BlueprintType)
class FALLENERA_API UFE_WeaponSightData : public UFE_WeaponAttachmentData
{
	GENERATED_BODY()
public:
	UFE_WeaponSightData();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sight")
	EFE_WeaponSightMode Mode = EFE_WeaponSightMode::IronSights;

	/** On the mounted sight mesh; for DefaultSight, on the weapon mesh. X points forward, Z points up. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sight|Alignment")
	FName AimSocketName = TEXT("AimSocket");

	/** Distance along the camera's forward axis. Keeps the socket centered without placing it on the near plane. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sight|Alignment", meta=(ClampMin="1.0", Units="cm"))
	float EyeReliefDistance = 12.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sight|Camera", meta=(ClampMin="5.0", ClampMax="120.0", Units="deg"))
	float AimFieldOfView = 65.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sight|Camera", meta=(ClampMin="0.0", Units="s"))
	float BlendInTime = 0.2f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sight|Camera", meta=(ClampMin="0.0", Units="s"))
	float BlendOutTime = 0.15f;

	/** Empty uses the native full-screen circular mask/crosshair widget. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sight|Scope2D")
	TSoftClassPtr<UFE_WeaponScopeWidget> ScopeWidgetClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sight|Scope2D")
	TSoftObjectPtr<UTexture2D> ReticleTexture;
};
