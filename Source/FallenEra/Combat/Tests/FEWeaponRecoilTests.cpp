#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/Component/FECombatComponent.h"
#include "Combat/Weapon/FEWeaponItemData.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"

namespace
{
	struct FFE_RecoilFixture
	{
		FFE_RecoilFixture()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			Character = World->SpawnActor<ACharacter>();
			Controller = World->SpawnActor<APlayerController>();
			Controller->SetAsLocalPlayerController();
			Controller->Possess(Character);
			Combat = NewObject<UFE_CombatComponent>(Character);
			Combat->RegisterComponent();
			Controller->SetControlRotation(FRotator(10, 20, 0));
		}
		~FFE_RecoilFixture()
		{
			World->DestroyWorld(false);
			GEngine->DestroyWorldContext(World);
		}
		void Advance(float DeltaTime, int32 Frames)
		{
			for (int32 Index = 0; Index < Frames; ++Index)
			{
				Combat->TickComponent(DeltaTime, LEVELTICK_All, nullptr);
			}
		}
		UWorld* World = nullptr;
		ACharacter* Character = nullptr;
		APlayerController* Controller = nullptr;
		UFE_CombatComponent* Combat = nullptr;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_WeaponRecoilTest, "FallenEra.Combat.Presentation.ManualRecoil",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFE_WeaponRecoilTest::RunTest(const FString& Parameters)
{
	FFE_RecoilSettings Settings;
	Settings.MinPitch = Settings.MaxPitch = 4.0f;
	Settings.MinYaw = Settings.MaxYaw = 2.0f;
	for (const int32 FrameRate : {30, 60, 144})
	{
		FFE_RecoilFixture Fixture;
		const float DeltaTime = 1.0f / FrameRate;
		Fixture.Combat->ApplyLocalWeaponRecoil(Settings, 0.5f);
		TestTrue(TEXT("Shot queues kick without an instant camera step"),
			Fixture.Controller->GetControlRotation().Equals(FRotator(10, 20, 0), 0.001f));
		Fixture.Advance(DeltaTime, 1);
		const FRotator FirstStep = Fixture.Controller->GetControlRotation();
		TestTrue(TEXT("First frame applies only a smooth fraction"), FirstStep.Pitch > 10.0f && FirstStep.Pitch < 12.0f);
		Fixture.Combat->ApplyLocalWeaponRecoil(Settings, 1.0f);
		TestTrue(TEXT("Perfect control adds no extra kick"), Fixture.Controller->GetControlRotation().Equals(FirstStep));
		Fixture.Combat->ApplyLocalWeaponRecoil(Settings, 0.5f);
		TestTrue(TEXT("Another shot does not snap or reset ongoing interpolation"),
			Fixture.Controller->GetControlRotation().Equals(FirstStep));
		// Add mouse compensation during interpolation; pending recoil must never restore an old absolute view.
		Fixture.Controller->SetControlRotation(FirstStep + FRotator(-1, -0.5f, 0));
		Fixture.Advance(DeltaTime, FrameRate);
		TestEqual(FString::Printf(TEXT("Full recoil preserves manual input at %d FPS"), FrameRate),
			Fixture.Controller->GetControlRotation(), FRotator(13, 21.5f, 0), 0.002f);
		TestFalse(TEXT("Settled recoil disables component ticking"), Fixture.Combat->IsComponentTickEnabled());
		Fixture.Controller->SetControlRotation(FRotator(11, 23, 0));
		Fixture.Advance(DeltaTime, FrameRate);
		TestTrue(TEXT("No automatic return after recoil settles"),
			Fixture.Controller->GetControlRotation().Equals(FRotator(11, 23, 0), 0.001f));
	}
	FFE_RecoilFixture Fixture;
	Settings.SmoothingTime = 0.0f;
	Fixture.Combat->ApplyLocalWeaponRecoil(Settings, 0.5f);
	TestTrue(TEXT("Zero smoothing retains the explicit instant-kick option"),
		Fixture.Controller->GetControlRotation().Equals(FRotator(12, 21, 0), 0.001f));
	TestFalse(TEXT("Instant recoil does not enable ticking"), Fixture.Combat->IsComponentTickEnabled());
	Settings.SmoothingTime = 0.04f;
	Fixture.Combat->ApplyLocalWeaponRecoil(Settings, 0.5f);
	Fixture.Controller->UnPossess();
	Fixture.Advance(0.1f, 1);
	TestFalse(TEXT("Unpossessing cancels unapplied recoil"), Fixture.Combat->IsComponentTickEnabled());
	TestTrue(TEXT("Unpossessed controller receives no stale kick"),
		Fixture.Controller->GetControlRotation().Equals(FRotator(12, 21, 0), 0.001f));
	return true;
}

#endif
