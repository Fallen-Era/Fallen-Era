#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Rendering/DrawElements.h"
#include "FEWeaponScopeWidget.generated.h"

class UFE_WeaponSightData;

/** Full-screen ADS overlay. BP subclasses may replace the default painted scope. */
UCLASS(Blueprintable)
class FALLENERA_API UFE_WeaponScopeWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void SetSightData(const UFE_WeaponSightData* Sight);
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry,
		const FSlateRect& CullingRect, FSlateWindowElementList& Elements, int32 LayerId,
		const FWidgetStyle& WidgetStyle, bool bParentEnabled) const override;
protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Scope")
	bool bDrawDefaultScope = true;
	UPROPERTY(BlueprintReadOnly, Transient, Category="Scope")
	TObjectPtr<const UFE_WeaponSightData> SightData;
	UPROPERTY(Transient)
	FSlateBrush ReticleBrush;
private:
	mutable FVector2D CachedSize = FVector2D::ZeroVector;
	mutable TArray<FVector2f> MaskPoints;
	mutable TArray<SlateIndex> MaskIndices;
	void UpdateMask(FVector2D Size) const;
};
