#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AbilitySystem/FallenEraAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/FallenEraAttributeSet.h"
#include "Combat/Ability/FEAmmoSelectionAbility.h"
#include "Combat/Component/FEEquipmentComponent.h"
#include "Combat/FECombatGameplayTags.h"
#include "Combat/UI/FEAmmoRadialWidget.h"
#include "Combat/Weapon/FEWeaponItemData.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "InputKeyEventArgs.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/CommandLine.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "Widgets/SViewport.h"
#include "UObject/UnrealType.h"

namespace
{
	/** Run in an isolated -RenderOffscreen editor; exercises real PIE viewport focus and key state. */
	class FFE_AmmoViewportCommand : public IAutomationLatentCommand
	{
	public:
		explicit FFE_AmmoViewportCommand(FAutomationTestBase* InTest) : Test(InTest), Deadline(FPlatformTime::Seconds() + 30.0) {}
		virtual bool Update() override
		{
			const double Now = FPlatformTime::Seconds();
			if (Now > Deadline) { Test->AddError(TEXT("Timed out waiting for PIE ammo wheel")); return true; }
			if (!Controller.IsValid())
			{
				UWorld* World = GEditor->PlayWorld;
				APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
				if (!PC || !PC->GetPawn() || !World->GetGameViewport() || !World->GetGameViewport()->GetGameViewportWidget()) { return false; }
				Controller = PC;
				APawn* Pawn = PC->GetPawn();
				ASC = NewObject<UFallenEraAbilitySystemComponent>(Pawn);
				ASC->RegisterComponent();
				ASC->AddAttributeSetSubobject(NewObject<UFallenEraAttributeSet>(Pawn));
				ASC->InitAbilityActorInfo(Pawn, Pawn);
				Equipment = NewObject<UFE_EquipmentComponent>(Pawn);
				Equipment->AmmoSelectionWidgetClass = LoadClass<UFE_AmmoRadialWidget>(nullptr, TEXT("/Game/Combat/UI/WBP/WBP_FE_AmmoRadial.WBP_FE_AmmoRadial_C"));
				UFE_RangedWeaponItemData* Weapon = NewObject<UFE_RangedWeaponItemData>(Equipment.Get());
				Weapon->Template.ItemTags.AddTag(InputTag());
				Weapon->Template.AmmoSettings.bUsesAmmo = true;
				Weapon->Template.AmmoSettings.AmmoType = FallenEraCombatGameplayTags::Item_Ammo_Rifle;
				Weapon->Template.AmmoSettings.MagazineCapacity = 3;
				Weapon->Instance.LoadedAmmo = 3;
				*FindFProperty<FArrayProperty>(Equipment->GetClass(), TEXT("TestItemTags"))
					->ContainerPtrToValuePtr<TArray<FGameplayTag>>(Equipment.Get()) = { InputTag() };
				*FindFProperty<FArrayProperty>(Equipment->GetClass(), TEXT("WeaponDataAssets"))
					->ContainerPtrToValuePtr<TArray<TSoftObjectPtr<UFE_WeaponItemData>>>(Equipment.Get()) = { Weapon };
				Equipment->RegisterComponent();
				Equipment->EquipWeaponByItemTag(InputTag());
				Equipment->SetReserveAmmo(FallenEraCombatGameplayTags::Item_Ammo_Rifle, 20);
				PC->SetInputMode(FInputModeGameOnly());
				PC->bShowMouseCursor = false;
				Press();
				StageTime = Now;
				return false;
			}
			if (!bOpened)
			{
				if (!ASC->HasMatchingGameplayTag(FallenEraCombatGameplayTags::State_Combat_SelectingAmmo)) { return false; }
				bOpened = true;
				StageTime = Now;
			}
			APlayerController* PC = Controller.Get();
			const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromClass(UFE_AmmoSelectionAbility::StaticClass());
			if (!Spec || !Spec->IsActive() || !PC->IsInputKeyDown(EKeys::R) || !PC->bShowMouseCursor)
			{
				Test->AddError(TEXT("Holding R must preserve key state, active GA and cursor without reopening"));
				return true;
			}
			const auto Viewport = PC->GetWorld()->GetGameViewport()->GetGameViewportWidget();
			Test->TestTrue(TEXT("Keyboard focus stays on the actual PIE viewport"), FSlateApplication::Get().GetUserFocusedWidget(0) == Viewport);
			if (!bClicked)
			{
				const FVector2D Position = Viewport->GetCachedGeometry().GetAbsolutePosition() + Viewport->GetCachedGeometry().GetAbsoluteSize() * 0.5;
				const FPointerEvent Click(0, Position, Position, {EKeys::LeftMouseButton}, EKeys::LeftMouseButton, 0, FModifierKeysState());
				Test->TestTrue(TEXT("Real Slate shot click is consumed by the wheel"), FSlateApplication::Get().ProcessMouseButtonDownEvent(nullptr, Click));
				Test->TestFalse(TEXT("Click cannot become a held gameplay attack"), PC->IsInputKeyDown(EKeys::LeftMouseButton));
				bClicked = true;
			}
			if (Now - StageTime < 1.0) { return false; }
			if (Cycle == 0)
			{
				TArray<FColor> Pixels;
				FIntVector Size;
				if (FSlateApplication::Get().TakeScreenshot(Viewport.ToSharedRef(), Pixels, Size))
				{
					const int32 ColoredPixels = Pixels.FilterByPredicate([](FColor Pixel) { return Pixel.B > Pixel.R + 10 && Pixel.B > 50; }).Num();
					Test->TestTrue(TEXT("Designer radial sectors are visible in real PIE"), ColoredPixels > 10000);
					TArray64<uint8> Png;
					FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, Png);
					FFileHelper::SaveArrayToFile(Png, *(FPaths::ProjectSavedDir() / TEXT("Screenshots/AmmoSelectionPIE.png")));
				}
				else { Test->AddError(TEXT("Could not capture the real PIE ammo menu")); }
			}
			PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::R, IE_Released, 0.0f));
			ASC->AbilityInputTagReleased(InputTag());
			ASC->ProcessAbilityInput(0.016f, false);
			Test->TestFalse(TEXT("R release ends the selection GA after a shot click"), Spec->IsActive());
			Test->TestFalse(TEXT("R release restores the gameplay cursor"), PC->bShowMouseCursor);
			Test->TestFalse(TEXT("R release restores camera look"), PC->IsLookInputIgnored());
			if (++Cycle == 2) { return true; }
			bOpened = bClicked = false;
			Press();
			return false;
		}
	private:
		static FGameplayTag InputTag() { return FallenEraCombatGameplayTags::Ability_Input_Combat_Reload; }
		void Press()
		{
			Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::R, IE_Pressed, 1.0f));
			ASC->AbilityInputTagPressed(InputTag());
			ASC->ProcessAbilityInput(0.016f, false);
		}
		FAutomationTestBase* Test;
		TWeakObjectPtr<APlayerController> Controller;
		TWeakObjectPtr<UFallenEraAbilitySystemComponent> ASC;
		TWeakObjectPtr<UFE_EquipmentComponent> Equipment;
		double Deadline, StageTime = 0;
		int32 Cycle = 0;
		bool bOpened = false, bClicked = false;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_AmmoViewportTest, "FallenEra.Integration.AmmoViewportHold",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFE_AmmoViewportTest::RunTest(const FString& Parameters)
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("FEAmmoViewportTest")))
	{
		AddInfo(TEXT("Opt-in test replaces the isolated editor map: use -FEAmmoViewportTest -RenderOffscreen."));
		return true;
	}
	if (GEditor->PlayWorld) { AddError(TEXT("Close an existing PIE session before running this test.")); return false; }
	UWorld* Map = FAutomationEditorCommonUtils::CreateNewMap();
	Map->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FFE_AmmoViewportCommand>(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
