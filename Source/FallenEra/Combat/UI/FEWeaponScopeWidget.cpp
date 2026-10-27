#include "Combat/UI/FEWeaponScopeWidget.h"

#include "Combat/Weapon/Attachment/FEWeaponSightData.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"

void UFE_WeaponScopeWidget::SetSightData(const UFE_WeaponSightData* Sight)
{
	SightData = Sight;
	ReticleBrush.SetResourceObject(Sight ? Sight->ReticleTexture.Get() : nullptr);
	ReticleBrush.DrawAs = ESlateBrushDrawType::Image;
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UFE_WeaponScopeWidget::UpdateMask(FVector2D Size) const
{
	if (CachedSize == Size) { return; }
	CachedSize = Size;
	MaskPoints.Reset();
	MaskIndices.Reset();
	const FVector2D Half = Size * 0.5;
	const double Radius = FMath::Min(Half.X, Half.Y) * 0.92;
	TArray<double> Angles;
	for (int32 Index = 0; Index < 96; ++Index) { Angles.Add(Index * UE_TWO_PI / 96.0); }
	// Include the exact rectangle corners so no viewport corner remains uncovered.
	for (const FVector2D Corner : {Half, FVector2D(-Half.X, Half.Y), -Half, FVector2D(Half.X, -Half.Y)})
	{
		double Angle = FMath::Atan2(Corner.Y, Corner.X);
		Angles.Add(Angle < 0.0 ? Angle + UE_TWO_PI : Angle);
	}
	Angles.Sort();
	for (double Angle : Angles)
	{
		const FVector2D Direction(FMath::Cos(Angle), FMath::Sin(Angle));
		const double OuterRadius = FMath::Min(Half.X / FMath::Max(FMath::Abs(Direction.X), UE_SMALL_NUMBER),
			Half.Y / FMath::Max(FMath::Abs(Direction.Y), UE_SMALL_NUMBER));
		MaskPoints.Add(FVector2f(Half + Direction * Radius));
		MaskPoints.Add(FVector2f(Half + Direction * OuterRadius));
	}
	for (int32 Index = 0; Index < Angles.Num(); ++Index)
	{
		const SlateIndex A = Index * 2;
		const SlateIndex B = ((Index + 1) % Angles.Num()) * 2;
		MaskIndices.Append({A, static_cast<SlateIndex>(A + 1), B,
			B, static_cast<SlateIndex>(A + 1), static_cast<SlateIndex>(B + 1)});
	}
}

int32 UFE_WeaponScopeWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geometry,
	const FSlateRect& CullingRect, FSlateWindowElementList& Elements, int32 LayerId,
	const FWidgetStyle& WidgetStyle, bool bParentEnabled) const
{
	if (bDrawDefaultScope && Geometry.GetLocalSize().GetMin() > 0.0f)
	{
		const FVector2D Size = Geometry.GetLocalSize();
		UpdateMask(Size);
		TArray<FSlateVertex> Vertices;
		Vertices.Reserve(MaskPoints.Num());
		const float Opacity = WidgetStyle.GetColorAndOpacityTint().A;
		const FColor Color = FLinearColor(0, 0, 0, Opacity).ToFColor(true);
		for (FVector2f Point : MaskPoints)
		{
			Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(
				Geometry.GetAccumulatedRenderTransform(), Point, FVector2f::ZeroVector, Color));
		}
		const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");
		FSlateDrawElement::MakeCustomVerts(Elements, LayerId++,
			FSlateApplication::Get().GetRenderer()->GetResourceHandle(*White), Vertices, MaskIndices, nullptr, 0, 0);
		const FVector2D Center = Size * 0.5;
		const float Diameter = Size.GetMin() * 0.92f;
		if (ReticleBrush.GetResourceObject())
		{
			FSlateDrawElement::MakeBox(Elements, LayerId++, Geometry.ToPaintGeometry(
				FVector2D(Diameter), FSlateLayoutTransform(Center - FVector2D(Diameter * 0.5f))),
				&ReticleBrush, ESlateDrawEffect::None, FLinearColor(1, 1, 1, Opacity));
		}
		else
		{
			for (const FVector2D Axis : {FVector2D(1, 0), FVector2D(0, 1)})
			{
				TArray<FVector2D> Points{Center - Axis * Diameter * 0.5f, Center + Axis * Diameter * 0.5f};
				FSlateDrawElement::MakeLines(Elements, LayerId, Geometry.ToPaintGeometry(), Points,
					ESlateDrawEffect::None, FLinearColor(0, 0, 0, Opacity), true, 1.0f);
			}
			++LayerId;
		}
	}
	return Super::NativePaint(Args, Geometry, CullingRect, Elements, LayerId, WidgetStyle, bParentEnabled);
}
