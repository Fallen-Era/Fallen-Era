#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AbilitySystem/FallenEraAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/FallenEraAttributeSet.h"
#include "AbilitySystem/FallenEraGameplayTags.h"
#include "Combat/Ability/FEMeleeAttackAbility.h"
#include "Combat/Component/FEEquipmentComponent.h"
#include "Combat/Component/FEStaminaComponent.h"
#include "Combat/FECombatGameplayTags.h"
#include "Combat/GameplayEffect/FEStaminaChangeGameplayEffect.h"
#include "Combat/UI/FEPlayerStatusViewModel.h"
#include "Combat/UI/FEPlayerStatusWidget.h"
#include "Components/ProgressBar.h"
#include "Components/HorizontalBox.h"
#include "Combat/Weapon/FEWeaponItemData.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "FallenEraCharacter.h"
#include "FallenEraPlayerState.h"
#include "Combat/Component/FEPlayerMovementStatsComponent.h"
#include "UObject/UnrealType.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	struct FStaminaFixture
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		ACharacter* Character;
		UFallenEraAbilitySystemComponent* ASC;
		const UFallenEraAttributeSet* Stats;
		UFE_StaminaComponent* Stamina;
		UFE_EquipmentComponent* Equipment;
		FStaminaFixture()
		{
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			Character = World->SpawnActor<ACharacter>();
			ASC = NewObject<UFallenEraAbilitySystemComponent>(Character);
			ASC->RegisterComponent();
			ASC->InitAbilityActorInfo(Character, Character);
			Stats = ASC->AddAttributeSetSubobject(NewObject<UFallenEraAttributeSet>(Character));
			Equipment = NewObject<UFE_EquipmentComponent>(Character);
			Equipment->RegisterComponent();
			Stamina = NewObject<UFE_StaminaComponent>(Character);
			Stamina->RegisterComponent();
			Stamina->InitializeAbilitySystem();
		}
		void Change(float Amount)
		{
			FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UFE_StaminaChangeGameplayEffect::StaticClass(), 1, ASC->MakeEffectContext());
			Spec.Data->SetSetByCallerMagnitude(FallenEraCombatGameplayTags::SetByCaller_Stamina_Change, Amount);
			ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		}
		bool Exhausted() const { return ASC->HasMatchingGameplayTag(FallenEraCombatGameplayTags::State_Stamina_Exhausted); }
		~FStaminaFixture() { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); }
	};

	class FStaminaLifecycleCommand : public IAutomationLatentCommand
	{
	public:
		explicit FStaminaLifecycleCommand(FAutomationTestBase* InTest) : Test(InTest) {}
		virtual bool Update() override
		{
			if (Step == 0)
			{
				Fixture = MakeUnique<FStaminaFixture>();
				Fixture->World->InitializeActorsForPlay(FURL());
				Fixture->World->BeginPlay();
				Fixture->Character->GetCharacterMovement()->DisableMovement();
				Fixture->World->Tick(LEVELTICK_All, 0.01f);
				Fixture->Stamina->ConsumeJumpStamina();
				Test->TestEqual(TEXT("One successful jump consumes its resource transaction"), Fixture->Stats->GetStamina(), 80.0f);
				Test->TestFalse(TEXT("No component tick"), Fixture->Stamina->PrimaryComponentTick.bCanEverTick);
				++Step;
				return false;
			}
			Fixture->World->Tick(LEVELTICK_All, 0.1f);
			const float Value = Fixture->Stats->GetStamina();
			if (Step % 10 == 0)
			{
				Test->AddInfo(FString::Printf(TEXT("Stamina timer step=%d worldTime=%.3f stamina=%.3f"), Step, Fixture->World->GetTimeSeconds(), Value));
			}
			switch (Step)
			{
			case 8: Test->TestEqual(TEXT("No recovery during the first second"), Value, 80.0f); break;
			case 12:
				Test->TestTrue(TEXT("Recovery starts after delay without crediting the waiting second"), Value > 80.0f && Value < 85.0f);
				Fixture->Stamina->ConsumeJumpStamina();
				SavedStamina = Fixture->Stats->GetStamina();
				break;
			case 18: Test->TestEqual(TEXT("Another cost resets recovery delay"), Value, SavedStamina); break;
			case 25:
				Test->TestTrue(TEXT("Recovery resumes after the new delay"), Value > SavedStamina);
				Fixture->ASC->AddLooseGameplayTag(FallenEraCombatGameplayTags::State_Combat_Aiming);
				SavedStamina = Value;
				break;
			case 40:
				Test->TestEqual(TEXT("ADS neither consumes nor regenerates"), Value, SavedStamina);
				Fixture->ASC->SetNumericAttributeBase(UFallenEraAttributeSet::GetStaminaRecoveryRateAttribute(), 20.0f);
				Fixture->ASC->RemoveLooseGameplayTag(FallenEraCombatGameplayTags::State_Combat_Aiming);
				break;
			case 44:
				Test->TestTrue(TEXT("Recovery uses the live Attribute rate after ADS release"), Value > SavedStamina);
				Fixture->Stamina->SetSprintDraining(true);
				Test->TestEqual(TEXT("Starting sprint has no up-front cost"), Fixture->Stats->GetStamina(), Value);
				SavedStamina = Value;
				break;
			case 48:
				Test->TestTrue(TEXT("Sustained sprint drains by elapsed time"), FMath::IsNearlyEqual(Value, SavedStamina - 4.0f, 0.02f));
				Fixture->Stamina->SetSprintDraining(false);
				SavedStamina = Value;
				break;
			case 55: Test->TestEqual(TEXT("Stopping sprint still observes the final consumption delay"), Value, SavedStamina); break;
			case 60:
				Fixture->Change(-1000.0f);
				Test->TestEqual(TEXT("External effect clamps stamina at zero"), Fixture->Stats->GetStamina(), 0.0f);
				Test->TestTrue(TEXT("Zero stamina grants replicated exhaustion state"), Fixture->Exhausted());
				Test->TestFalse(TEXT("Exhaustion blocks jump"), Fixture->Stamina->CanJumpWithStamina());
				Test->TestFalse(TEXT("Exhaustion blocks sprint"), Fixture->Stamina->CanSprintWithStamina());
				break;
			case 79:
				Test->TestTrue(TEXT("Partial refill below 20 remains exhausted"), Value > 0.0f && Value < 20.0f && Fixture->Exhausted());
				Test->TestFalse(TEXT("Cheap consuming actions still blocked below threshold"), Fixture->Stamina->CanSpendStamina(1.0f));
				break;
			case 83:
				Test->TestTrue(TEXT("At 20 exhaustion is cleared and actions can resume"), Value >= 20.0f && !Fixture->Exhausted() && Fixture->Stamina->CanJumpWithStamina());
				Fixture->ASC->AddLooseGameplayTag(FallenEraGameplayTags::State_Dead);
				SavedStamina = Value;
				break;
			case 95:
				Test->TestEqual(TEXT("Dead players do not regenerate"), Value, SavedStamina);
				Fixture.Reset();
				return true;
			}
			++Step;
			return false;
		}
	private:
		FAutomationTestBase* Test;
		TUniquePtr<FStaminaFixture> Fixture;
		int32 Step = 0;
		float SavedStamina = 0;
	};

	class FStaminaFeedbackCommand : public IAutomationLatentCommand
	{
	public:
		explicit FStaminaFeedbackCommand(FAutomationTestBase* InTest) : Test(InTest) {}
		virtual bool Update() override
		{
			if (Step == 0)
			{
				Fixture = MakeUnique<FStaminaFixture>();
				APlayerController* PC = Fixture->World->SpawnActor<APlayerController>();
				PC->SetAsLocalPlayerController();
				PC->Possess(Fixture->Character);
				Fixture->World->InitializeActorsForPlay(FURL());
				Fixture->World->BeginPlay();
				Fixture->Character->GetCharacterMovement()->DisableMovement();
				Fixture->World->Tick(LEVELTICK_All, 0.01f);
				Fixture->ASC->SetNumericAttributeBase(UFallenEraAttributeSet::GetStaminaRecoveryRateAttribute(), 0.0f);
				Fixture->ASC->SetNumericAttributeBase(UFallenEraAttributeSet::GetStaminaAttribute(), 5.0f);
				Widget.Reset(CreateWidget<UFE_PlayerStatusWidget>(Fixture->World, UFE_PlayerStatusWidget::StaticClass()));
				if (!Test->TestNotNull(TEXT("World-owned status widget"), Widget.Get())) { Fixture.Reset(); return true; }
				Bar = NewObject<UProgressBar>(Widget.Get());
				Bar->SetFillColorAndOpacity(FLinearColor::Green);
				Bar->SetRenderOpacity(0.8f);
				FindFProperty<FObjectPropertyBase>(Widget->GetClass(), TEXT("ProgressBar_StaminaBar"))->SetObjectPropertyValue_InContainer(Widget.Get(), Bar.Get());
				FindFProperty<FObjectPropertyBase>(Widget->GetClass(), TEXT("HorizontalBox_AmmoStatusBox"))->SetObjectPropertyValue_InContainer(Widget.Get(), NewObject<UHorizontalBox>(Widget.Get()));
				Widget->NativeConstruct();
				Widget->StatusViewModel->Initialize(PC);
				Widget->StatusViewModel->Initialize(PC);
				FeedbackHandle = Widget->StatusViewModel->StaminaActionRejected.AddLambda([this]() { ++FeedbackCount; });
				Fixture->Stamina->BeginActionAttempt();
				Fixture->Stamina->TrySpendStamina(20.0f);
				Test->TestEqual(TEXT("One rejected action reaches the model exactly once"), FeedbackCount, 1);
				Test->TestNotNull(TEXT("Blink timer has a real widget world"), Widget->GetWorld());
				Test->AddInfo(FString::Printf(TEXT("Stamina warning render opacity: %.9f"), Bar->GetRenderOpacity()));
				Test->TestTrue(TEXT("Rejected action immediately dims the stamina bar"), FMath::IsNearlyEqual(Bar->GetRenderOpacity(), 0.16f, 0.0001f));
				for (int32 Index = 0; Index < 5; ++Index)
				{
					Fixture->Stamina->CanSpendStamina(20.0f);
					Fixture->Stamina->TrySpendStamina(20.0f);
				}
				Test->TestEqual(TEXT("Pure queries and held retries cannot spam feedback"), FeedbackCount, 1);
				Test->TestEqual(TEXT("Rejected action does not consume stamina"), Fixture->Stats->GetStamina(), 5.0f);
				++Step;
				return false;
			}
			Fixture->World->Tick(LEVELTICK_All, 0.06f);
			switch (Step)
			{
			case 3:
				Test->TestTrue(TEXT("Blink alternates back to the original opacity"), FMath::IsNearlyEqual(Bar->GetRenderOpacity(), 0.8f));
				break;
			case 4:
				Fixture->Stamina->BeginActionAttempt();
				Fixture->Stamina->TrySpendStamina(20.0f);
				Test->TestEqual(TEXT("A fresh action can warn again"), FeedbackCount, 2);
				break;
			case 13:
				Test->TestTrue(TEXT("Repeat input does not extend the finite blink timer"), FMath::IsNearlyEqual(Bar->GetRenderOpacity(), 0.8f));
				Test->TestTrue(TEXT("Blink never overwrites the authored fill color"), Bar->GetFillColorAndOpacity().Equals(FLinearColor::Green));
				Fixture->Change(-100.0f);
				Fixture->Stamina->BeginActionAttempt();
				Fixture->Stamina->TrySpendStamina(1.0f);
				Test->TestTrue(TEXT("Exhausted warnings blink while keeping gray fill"),
					FMath::IsNearlyEqual(Bar->GetRenderOpacity(), 0.16f, 0.0001f) && Bar->GetFillColorAndOpacity().Equals(FLinearColor(0.4f, 0.4f, 0.4f, 1)));
				break;
			case 14:
				Fixture->Stamina->BeginActionAttempt();
				Fixture->Stamina->TrySpendStamina(-1.0f);
				Test->TestEqual(TEXT("Invalid cost data is not a stamina warning"), FeedbackCount, 3);
				break;
			case 15:
				Widget->NativeDestruct();
				Test->TestTrue(TEXT("Closing the widget restores opacity and stops its timer"), FMath::IsNearlyEqual(Bar->GetRenderOpacity(), 0.8f));
				Fixture->Stamina->BeginActionAttempt();
				Fixture->Stamina->TrySpendStamina(1.0f);
				Test->TestEqual(TEXT("A closed widget/model no longer receives feedback"), FeedbackCount, 3);
				break;
			case 17:
				Widget->StatusViewModel->StaminaActionRejected.Remove(FeedbackHandle);
				Widget.Reset();
				Fixture.Reset();
				return true;
			}
			++Step;
			return false;
		}
	private:
		FAutomationTestBase* Test;
		TUniquePtr<FStaminaFixture> Fixture;
		TStrongObjectPtr<UFE_PlayerStatusWidget> Widget;
		TWeakObjectPtr<UProgressBar> Bar;
		FDelegateHandle FeedbackHandle;
		int32 Step = 0;
		int32 FeedbackCount = 0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_StaminaFeedbackTest, "FallenEra.Combat.Stamina.FailureFeedback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFE_StaminaFeedbackTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FStaminaFeedbackCommand(this));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_StaminaLifecycleTest, "FallenEra.Combat.Stamina.Lifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFE_StaminaLifecycleTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FStaminaLifecycleCommand(this));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_MeleeStaminaTest, "FallenEra.Combat.Stamina.PerSwing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFE_MeleeStaminaTest::RunTest(const FString& Parameters)
{
	FStaminaFixture Fixture;
	APlayerController* PC = Fixture.World->SpawnActor<APlayerController>();
	PC->SetAsLocalPlayerController();
	PC->Possess(Fixture.Character);
	int32 RejectedCount = 0;
	const FDelegateHandle FeedbackHandle = Fixture.Stamina->OnActionRejected.AddLambda([&RejectedCount]() { ++RejectedCount; });
	UFE_MeleeWeaponItemData* Weapon = NewObject<UFE_MeleeWeaponItemData>(Fixture.World);
	Weapon->Template.ItemTags.AddTag(FallenEraCombatGameplayTags::Ability_Input_Combat_LeftClick);
	Weapon->Template.AttachSocketName = NAME_None;
	Weapon->Instance.StaminaCost = 40.0f;
	UFE_MeleeAttackData* Attack = NewObject<UFE_MeleeAttackData>(Weapon);
	Attack->AbilityClass = UFE_MeleeAttackAbility::StaticClass();
	Attack->InputTag = FallenEraCombatGameplayTags::Ability_Input_Combat_LeftClick;
	Attack->AttackStartEventTag = FGameplayTag::RequestGameplayTag(TEXT("GameplayEvent.Attack.TraceStart"));
	Attack->AttackResetEventTag = FGameplayTag::RequestGameplayTag(TEXT("GameplayEvent.Attack.Reset"));
	Attack->bAutomatic = true;
	Weapon->Template.AttackActions.Add(Attack);
	*FindFProperty<FArrayProperty>(Fixture.Equipment->GetClass(), TEXT("TestItemTags"))->ContainerPtrToValuePtr<TArray<FGameplayTag>>(Fixture.Equipment) =
		{ FallenEraCombatGameplayTags::Ability_Input_Combat_LeftClick };
	*FindFProperty<FArrayProperty>(Fixture.Equipment->GetClass(), TEXT("WeaponDataAssets"))->ContainerPtrToValuePtr<TArray<TSoftObjectPtr<UFE_WeaponItemData>>>(Fixture.Equipment) = { Weapon };
	Fixture.Equipment->EquipWeaponByItemTag(FallenEraCombatGameplayTags::Ability_Input_Combat_LeftClick);
	Fixture.Equipment->RefreshEquipment();
	TestTrue(TEXT("Runtime instance supplies the stamina cost"), Fixture.Equipment->GetWeaponStaminaCost() == 40.0f);
	TestTrue(TEXT("Initial swing activates"), Fixture.ASC->TryActivateAbilityByClass(UFE_MeleeAttackAbility::StaticClass()));
	TestEqual(TEXT("First swing spends once, not twice at activation"), Fixture.Stats->GetStamina(), 60.0f);
	FGameplayEventData Event;
	Event.EventTag = Attack->AttackResetEventTag;
	Fixture.ASC->HandleGameplayEvent(Event.EventTag, &Event);
	TestEqual(TEXT("Automatic next swing pays its own cost"), Fixture.Stats->GetStamina(), 20.0f);
	Fixture.ASC->HandleGameplayEvent(Event.EventTag, &Event);
	TestEqual(TEXT("Insufficient resource cannot start the third swing"), Fixture.Stats->GetStamina(), 20.0f);
	TestEqual(TEXT("Automatic swing failure emits one stamina warning"), RejectedCount, 1);
	Fixture.ASC->SetNumericAttributeBase(UFallenEraAttributeSet::GetStaminaAttribute(), 40.0f);
	TestTrue(TEXT("A final exactly affordable swing is allowed"), Fixture.ASC->TryActivateAbilityByClass(UFE_MeleeAttackAbility::StaticClass()));
	TestTrue(TEXT("Last affordable swing reaches zero and exhaustion"), Fixture.Exhausted() && Fixture.Stats->GetStamina() == 0.0f);
	Fixture.ASC->HandleGameplayEvent(Event.EventTag, &Event);
	TestFalse(TEXT("Another activation is blocked during exhaustion"), Fixture.ASC->TryActivateAbilityByClass(UFE_MeleeAttackAbility::StaticClass()));
	Fixture.Stamina->BeginActionAttempt();
	TestFalse(TEXT("Fresh input is still rejected during exhaustion"), Fixture.ASC->TryActivateAbilityByClass(UFE_MeleeAttackAbility::StaticClass()));
	TestEqual(TEXT("GAS activation failure carries the stamina reason to local feedback"), RejectedCount, 2);
	Fixture.Stamina->OnActionRejected.Remove(FeedbackHandle);
	TestEqual(TEXT("Runtime spending never edits the DA seed"), Weapon->Instance.StaminaCost, 40.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_StaminaStatusModelTest, "FallenEra.Combat.Stamina.StatusModel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFE_StaminaStatusModelTest::RunTest(const FString& Parameters)
{
	FStaminaFixture Fixture;
	APlayerController* PC = Fixture.World->SpawnActor<APlayerController>();
	PC->SetAsLocalPlayerController();
	PC->Possess(Fixture.Character);
	UFE_PlayerStatusViewModel* Model = NewObject<UFE_PlayerStatusViewModel>();
	Model->Initialize(PC);
	Model->Initialize(PC);
	Fixture.Change(-100.0f);
	TestTrue(TEXT("Tag event drives exhausted status without widget polling"), Model->bStaminaExhausted);
	Fixture.Change(19.0f);
	TestTrue(TEXT("Partial refill must keep gray UI state"), Model->bStaminaExhausted);
	Fixture.Change(1.0f);
	TestFalse(TEXT("Threshold tag event restores normal UI state"), Model->bStaminaExhausted);
	Model->Shutdown();
	Fixture.Change(-100.0f);
	TestFalse(TEXT("Shutdown unsubscribes the stamina state source"), Model->bStaminaExhausted);
	UFE_PlayerStatusWidget* Widget = NewObject<UFE_PlayerStatusWidget>(PC);
	UProgressBar* Bar = NewObject<UProgressBar>(Widget);
	const FLinearColor OriginalColor(0.15f, 0.8f, 0.45f, 0.75f);
	Bar->SetFillColorAndOpacity(OriginalColor);
	FindFProperty<FObjectPropertyBase>(Widget->GetClass(), TEXT("ProgressBar_StaminaBar"))->SetObjectPropertyValue_InContainer(Widget, Bar);
	FindFProperty<FObjectPropertyBase>(Widget->GetClass(), TEXT("HorizontalBox_AmmoStatusBox"))->SetObjectPropertyValue_InContainer(Widget, NewObject<UHorizontalBox>(Widget));
	Widget->NativeConstruct();
	Widget->StatusViewModel->Initialize(PC);
	TestTrue(TEXT("Exhausted widget uses gray fill but preserves alpha"), Bar->GetFillColorAndOpacity().Equals(FLinearColor(0.4f, 0.4f, 0.4f, OriginalColor.A)));
	Widget->NativeDestruct();
	Widget->NativeConstruct();
	Widget->StatusViewModel->Initialize(PC);
	Fixture.Change(20.0f);
	TestTrue(TEXT("Original authored color survives reconstructing the widget while exhausted"), Bar->GetFillColorAndOpacity().Equals(OriginalColor));
	Widget->NativeDestruct();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_StaminaMovementTest, "FallenEra.Combat.Stamina.PlayerMovement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFE_StaminaMovementTest::RunTest(const FString& Parameters)
{
	FStaminaFixture Fixture;
	UClass* PlayerClass = LoadClass<AFallenEraCharacter>(nullptr,
		TEXT("/Game/Combat/Character/PlayerCharacter/BP_HT_PlayerCharacter.BP_HT_PlayerCharacter_C"));
	if (!TestNotNull(TEXT("Concrete player BP"), PlayerClass)) { return false; }
	AFallenEraCharacter* Player = Fixture.World->SpawnActor<AFallenEraCharacter>(PlayerClass);
	APlayerController* PC = Fixture.World->SpawnActor<APlayerController>();
	PC->SetAsLocalPlayerController();
	PC->PlayerState = Fixture.World->SpawnActor<AFallenEraPlayerState>();
	Fixture.World->InitializeActorsForPlay(FURL());
	PC->Possess(Player);
	UFallenEraAbilitySystemComponent* ASC = Player->GetFallenEraAbilitySystemComponent();
	const UFallenEraAttributeSet* Stats = ASC->GetSet<UFallenEraAttributeSet>();
	Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	Player->Jump();
	Player->CheckJumpInput(0.016f);
	TestEqual(TEXT("Actual jump take-off spends once"), Stats->GetStamina(), 80.0f);
	Player->CheckJumpInput(0.016f);
	TestEqual(TEXT("Held jump cannot consume another take-off cost"), Stats->GetStamina(), 80.0f);
	Player->StopJumping();
	Player->ResetJumpState();
	Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	UFE_PlayerMovementStatsComponent* Movement = Player->GetPlayerMovementStatsComponent();
	Movement->SetSprinting(true);
	TestTrue(TEXT("Sprint starts without a resource transaction"), Movement->IsSprinting() && Stats->GetStamina() == 80.0f);
	ASC->SetNumericAttributeBase(UFallenEraAttributeSet::GetStaminaAttribute(), 0.0f);
	TestFalse(TEXT("Exhaustion immediately stops sprint"), Movement->IsSprinting());
	TestEqual(TEXT("Exhaustion restores walking speed"), Player->GetCharacterMovement()->MaxWalkSpeed, 300.0f);
	ASC->SetNumericAttributeBase(UFallenEraAttributeSet::GetStaminaAttribute(), 19.0f);
	int32 RejectedActions = 0;
	const FDelegateHandle FeedbackHandle = Player->FindComponentByClass<UFE_StaminaComponent>()->OnActionRejected.AddLambda([&RejectedActions]() { ++RejectedActions; });
	Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	Player->ProcessEvent(Player->FindFunctionChecked(TEXT("DoJumpStart")), nullptr);
	Player->CheckJumpInput(0.016f);
	TestEqual(TEXT("Exhausted jump input cannot spend stamina"), Stats->GetStamina(), 19.0f);
	TestEqual(TEXT("Exhausted jump cannot take off"), Player->JumpCurrentCount, 0);
	TestEqual(TEXT("Fresh failed jump input reports stamina shortage once"), RejectedActions, 1);
	Movement->SetSprinting(true);
	TestFalse(TEXT("Sprint cannot restart at 19"), Movement->IsSprinting());
	TestEqual(TEXT("Fresh rejected sprint input reports stamina shortage"), RejectedActions, 2);
	ASC->SetNumericAttributeBase(UFallenEraAttributeSet::GetStaminaAttribute(), 20.0f);
	Movement->SetSprinting(true);
	TestTrue(TEXT("Fresh sprint input can resume at 20"), Movement->IsSprinting());
	Player->FindComponentByClass<UFE_StaminaComponent>()->OnActionRejected.Remove(FeedbackHandle);
	return true;
}

#endif
