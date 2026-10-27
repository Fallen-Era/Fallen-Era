#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/UI/FEAmmoRadialWidget.h"
#include "Combat/UI/FEAmmoSelectionViewModel.h"
#include "Combat/UI/FEAmmoOptionWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "ImageUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Slate/WidgetRenderer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_AmmoRadialRenderingTest, "FallenEra.Combat.Ammo.RadialRendering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFE_AmmoRadialRenderingTest::RunTest(const FString& Parameters)
{
	if (FParse::Param(FCommandLine::Get(), TEXT("NullRHI"))) { AddInfo(TEXT("Rendering requires RHI; run this test with -RenderOffscreen.")); return true; }
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	UClass* WidgetClass = LoadClass<UFE_AmmoRadialWidget>(nullptr, TEXT("/Game/Combat/UI/WBP/WBP_FE_AmmoRadial.WBP_FE_AmmoRadial_C"));
	if (!TestNotNull(TEXT("Designer radial WBP exists"), WidgetClass)) { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); return false; }
	UFE_AmmoRadialWidget* Defaults = WidgetClass->GetDefaultObject<UFE_AmmoRadialWidget>();
	TestNotNull(TEXT("Designer option WBP can preload"), Defaults->OptionWidgetClass.LoadSynchronous());
	UFE_AmmoRadialWidget* Widget = CreateWidget<UFE_AmmoRadialWidget>(World, WidgetClass);
	if (!TestNotNull(TEXT("Bound UMG widget constructs"), Widget)) { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); return false; }
	UFE_AmmoSelectionViewModel* Model = NewObject<UFE_AmmoSelectionViewModel>(Widget);
	for (int32 Index = 0; Index < 2; ++Index)
	{
		auto& Option = Model->Options.AddDefaulted_GetRef();
		Option.AmmoId = FGuid::NewGuid();
		Option.Name = Index == 0 ? NSLOCTEXT("FEAmmo", "PreviewStandard", "9mm 일반탄") : NSLOCTEXT("FEAmmo", "PreviewEnhanced", "9mm 강화탄");
		Option.Count = Index == 0 ? 90 : 30;
		Option.DamageMultiplier = Index == 0 ? 1.0f : 1.2f;
		Option.bLoaded = Index == 0;
	}
	Model->SelectIndex(1);
	TestTrue(TEXT("Required BindWidgets and option class are present"), Widget->SetViewModel(Model));
	TestFalse(TEXT("Wheel cannot steal R keyboard focus"), Widget->IsFocusable());
	UCanvasPanel* Options = Cast<UCanvasPanel>(Widget->GetWidgetFromName(TEXT("CanvasPanel_Options")));
	TestEqual(TEXT("One designer option per ammo stack"), Options ? Options->GetChildrenCount() : 0, 2);
	if (Options && Options->GetChildrenCount() > 1)
	{
		UUserWidget* Entry = Cast<UUserWidget>(Options->GetChildAt(1));
		const UTextBlock* Count = Entry ? Cast<UTextBlock>(Entry->GetWidgetFromName(TEXT("Text_Count"))) : nullptr;
		TestTrue(TEXT("Bound count label receives MVVM option data"), Count && Count->GetText().ToString().Contains(TEXT("30")));
	}
	const TSharedRef<SWidget> SlateWidget = Widget->TakeWidget();
	FWidgetRenderer Renderer(true);
	UTextureRenderTarget2D* Target = Renderer.DrawWidget(SlateWidget, FVector2D(1024, 768)); // Populate the WBP's cached layout before pointer hit testing.
	if (Options && Options->GetChildrenCount() > 1)
	{
		UUserWidget* Entry = Cast<UUserWidget>(Options->GetChildAt(1));
		UImage* Sector = Entry ? Cast<UImage>(Entry->GetWidgetFromName(TEXT("Image_Sector"))) : nullptr;
		UMaterialInstanceDynamic* Material = Sector ? Sector->GetDynamicMaterial() : nullptr;
		TestNotNull(TEXT("Sector has a configured UI material"), Material);
		if (Sector) { TestEqual(TEXT("Sector fills its designer canvas"), FVector2D(Sector->GetCachedGeometry().GetLocalSize()), FVector2D(500.0)); }
		if (Material) { TestEqual(TEXT("Second sector receives the correct material angle"), Material->K2_GetScalarParameterValue(TEXT("SectorStart")), float(UE_PI)); }
	}
	const FGeometry HitGeometry = FGeometry::MakeRoot(FVector2f(1024, 768), FSlateLayoutTransform());
	const FVector2D RightSector(670, 384);
	const FPointerEvent Move(0, RightSector, RightSector, TSet<FKey>(), FKey(), 0, FModifierKeysState());
	SlateWidget->OnMouseMove(HitGeometry, Move);
	TestEqual(TEXT("Real UMG mouse event forwards hover selection"), Model->SelectedIndex, 0);
	Model->SelectIndex(1);
	TestTrue(TEXT("Wheel participates in Slate pointer hit testing"), Widget->GetVisibility() == ESlateVisibility::Visible);
	TArray<FColor> Pixels;
	const bool bRead = Target && Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
	TestTrue(TEXT("Designer UMG paints to an actual render target"), bRead && Pixels.Num() == 1024 * 768);
	if (bRead)
	{
		// UI material shader resources are prepared lazily during Slate rendering; the real-frame PIE test checks sector pixels.
		TestTrue(TEXT("Bound designer text paints"), Pixels.ContainsByPredicate([](FColor Pixel) { return Pixel.R > 180 && Pixel.G > 180 && Pixel.B > 180; }));
		TArray64<uint8> Png;
		FImageUtils::PNGCompressImageArray(1024, 768, Pixels, Png);
		const FString Path = FPaths::ProjectSavedDir() / TEXT("Screenshots/AmmoSelection.png");
		IFileManager::Get().MakeDirectory(*(FPaths::ProjectSavedDir() / TEXT("Screenshots")), true);
		TestTrue(TEXT("Preview screenshot saved"), FFileHelper::SaveArrayToFile(Png, *Path));
	}
	Widget->SetViewModel(nullptr);
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

#endif
