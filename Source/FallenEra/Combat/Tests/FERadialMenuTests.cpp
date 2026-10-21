#include "Combat/Tests/FERadialMenuTestReceiver.h"
#include "Combat/UI/Radial/FERadialMenuViewModel.h"

void UFE_RadialMenuTestReceiver::ExecuteItem(FName ItemId)
{
	++ExecutionCount;
	LastItem = ItemId;
	if (Menu.IsValid()) { bReentered |= Menu->ConfirmSelection(); }
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/UI/Radial/FERadialMenuWidget.h"
#include "Combat/UI/Radial/FERadialMenuOptionWidget.h"
#include "Combat/UI/FEAmmoSelectionViewModel.h"
#include "Components/CanvasPanel.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_RadialMenuModelTest, "FallenEra.Combat.RadialMenu.ModelAndActions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFE_RadialMenuModelTest::RunTest(const FString& Parameters)
{
	UFE_RadialMenuViewModel* Model = NewObject<UFE_RadialMenuViewModel>();
	UFE_RadialMenuTestReceiver* Receiver = NewObject<UFE_RadialMenuTestReceiver>();
	Receiver->Menu = Model;
	TArray<FFE_RadialMenuItem> Items;
	for (int32 Index = 0; Index < 5; ++Index)
	{
		auto& Item = Items.AddDefaulted_GetRef();
		Item.Id = FName(*FString::Printf(TEXT("Action%d"), Index));
		Item.Label = FText::FromName(Item.Id);
		Item.OnExecute.BindDynamic(Receiver, &UFE_RadialMenuTestReceiver::ExecuteItem);
	}
	Items[1].bEnabled = false;
	TestTrue(TEXT("Accept five arbitrary, non-ammo commands"), Model->SetItems(Items));
	Model->SelectIndex(1);
	TestFalse(TEXT("Disabled command cannot confirm"), Model->ConfirmSelection());
	Model->SelectIndex(3);
	TestEqual(TEXT("Hover never executes"), Receiver->ExecutionCount, 0);
	TestTrue(TEXT("Confirmation invokes the selected command"), Model->ConfirmSelection());
	TestEqual(TEXT("Correct command receives its stable ID"), Receiver->LastItem, Items[3].Id);
	TestFalse(TEXT("Reentrant command execution is rejected"), Receiver->bReentered);
	TestFalse(TEXT("Repeated release/confirm cannot execute twice"), Model->ConfirmSelection());
	Model->BeginInteraction();
	TestEqual(TEXT("A new interaction clears the previous hover"), Model->SelectedIndex, INDEX_NONE);
	Model->SelectIndex(3);
	Items.Swap(3, 4);
	TestTrue(TEXT("Runtime item reorder"), Model->SetItems(Items));
	TestEqual(TEXT("Selection follows ID rather than stale index"), Model->SelectedIndex, 4);
	TestTrue(TEXT("New interaction can confirm again"), Model->ConfirmSelection());
	TestEqual(TEXT("One execution per interaction"), Receiver->ExecutionCount, 2);
	Items[0].Id = Items[1].Id;
	TestFalse(TEXT("Duplicate command IDs rejected without corrupting the menu"), Model->SetItems(Items));
	for (const int32 Count : {1, 2, 3, 5, 8, 12})
	{
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const double Angle = UE_TWO_PI * (Index + 0.5) / Count;
			TestEqual(TEXT("Sector center maps to its item for arbitrary menu counts"),
				Model->FindSector(FVector2D(FMath::Sin(Angle), -FMath::Cos(Angle)) * 150.0, Count, 85, 225), Index);
		}
	}
	TestEqual(TEXT("Center cancels"), Model->FindSector(FVector2D::ZeroVector, 5, 85, 225), INDEX_NONE);
	TestEqual(TEXT("Empty menu cancels"), Model->FindSector(FVector2D(150, 0), 0, 85, 225), INDEX_NONE);
	Model->Shutdown();
	TestTrue(TEXT("Shutdown drops command bindings"), Model->Items.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_RadialMenuWidgetTest, "FallenEra.Combat.RadialMenu.DesignerBindings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFE_RadialMenuWidgetTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	UClass* Class = LoadClass<UFE_RadialMenuWidget>(nullptr, TEXT("/Game/Combat/UI/WBP/WBP_FE_RadialMenu.WBP_FE_RadialMenu_C"));
	if (!TestNotNull(TEXT("Reusable designer WBP exists"), Class)) { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); return false; }
	Class->GetDefaultObject<UFE_RadialMenuWidget>()->OptionWidgetClass.LoadSynchronous();
	UFE_RadialMenuWidget* Widget = CreateWidget<UFE_RadialMenuWidget>(World, Class);
	const TSharedRef<SWidget> SlateWidget = Widget->TakeWidget();
	UFE_RadialMenuViewModel* Model = NewObject<UFE_RadialMenuViewModel>(Widget);
	UFE_RadialMenuTestReceiver* Receiver = NewObject<UFE_RadialMenuTestReceiver>(Widget);
	Model->SetMenuText(FText::FromString(TEXT("Commands")), FText::FromString(TEXT("Confirm")), FText::FromString(TEXT("Cancel")), FText::FromString(TEXT("Empty")));
	for (const int32 Count : {0, 1, 3, 8, 2})
	{
		TArray<FFE_RadialMenuItem> Items;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			auto& Item = Items.AddDefaulted_GetRef();
			Item.Id = FName(*FString::FromInt(Index)); Item.Label = FText::FromName(Item.Id);
			Item.OnExecute.BindDynamic(Receiver, &UFE_RadialMenuTestReceiver::ExecuteItem);
		}
		Model->SetItems(Items);
		TestTrue(TEXT("Bind generic model"), Widget->SetMenuModel(Model));
		UCanvasPanel* Panel = Cast<UCanvasPanel>(Widget->GetWidgetFromName(TEXT("CanvasPanel_Options")));
		TestEqual(TEXT("Sector count follows menu count at runtime"), Panel ? Panel->GetChildrenCount() : -1, Count);
		if (Count > 0 && Panel && Panel->GetChildrenCount() == Count)
		{
			UUserWidget* Entry = Cast<UUserWidget>(Panel->GetChildAt(Count - 1));
			UImage* Sector = Cast<UImage>(Entry->GetWidgetFromName(TEXT("Image_Sector")));
			UMaterialInstanceDynamic* Material = Sector->GetDynamicMaterial();
			TestNotNull(TEXT("Designer sector material assigned"), Material);
			if (Material) { TestTrue(TEXT("Sector angle tracks item count"), FMath::IsNearlyEqual(Material->K2_GetScalarParameterValue(TEXT("SectorAngle")), float(UE_TWO_PI / Count))); }
			const UTextBlock* Label = Cast<UTextBlock>(Entry->GetWidgetFromName(TEXT("Text_Label")));
			TestTrue(TEXT("Generic item content bound"), Label && Label->GetText().EqualTo(Items.Last().Label));
		}
	}
	Model->BeginInteraction();
	Model->SelectIndex(1);
	TestTrue(TEXT("BP-facing ConfirmMenu invokes the selected item"), Widget->ConfirmMenu());
	TestEqual(TEXT("Widget executes exactly one item callback"), Receiver->ExecutionCount, 1);
	TestFalse(TEXT("Widget ignores duplicate close/confirm"), Widget->ConfirmMenu());
	TestFalse(TEXT("Ownerless test widget cannot capture a player's input"), Widget->CaptureMenuInput());
	Widget->SetMenuModel(nullptr);
	Model->Shutdown();
	World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
	return true;
}
#endif
