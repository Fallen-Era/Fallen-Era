#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/FallenEraAttributeSet.h"
#include "AbilitySystem/FallenEraGameplayTags.h"
#include "Combat/FECombatGameplayTags.h"
#include "Combat/Collision/FECollisionChannels.h"
#include "Combat/Component/FECharacterStatusComponent.h"
#include "Combat/Component/FECombatComponent.h"
#include "Combat/ObjectPool/FEProjectilePoolSubsystem.h"
#include "Combat/Projectile/FEArrowProjectile.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "UObject/UnrealType.h"
#include "TimerManager.h"

namespace
{
	struct FFE_ReviewTestWorld
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		ACharacter* Character = World->SpawnActor<ACharacter>();
		UAbilitySystemComponent* ASC = NewObject<UAbilitySystemComponent>(Character);
		UFallenEraAttributeSet* Stats = NewObject<UFallenEraAttributeSet>(Character);
		FFE_ReviewTestWorld()
		{
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			ASC->RegisterComponent();
			ASC->InitializeComponent();
			ASC->AddAttributeSetSubobject(Stats);
			ASC->InitAbilityActorInfo(Character, Character);
		}
		~FFE_ReviewTestWorld()
		{
			World->DestroyWorld(false);
			GEngine->DestroyWorldContext(World);
		}
	};

	/** Advance the test world's timers on separate automation frames, just like normal play. */
	class FFE_NaturalConditionExpiryCommand : public IAutomationLatentCommand
	{
	public:
		explicit FFE_NaturalConditionExpiryCommand(FAutomationTestBase* InTest) : Test(InTest)
		{
			Status = NewObject<UFE_CharacterStatusComponent>(TestWorld.Character);
			Status->RegisterComponent();
			FStructProperty* Property = FindFProperty<FStructProperty>(Status->GetClass(), TEXT("InfectionCondition"));
			Property->ContainerPtrToValuePtr<FFE_CharacterConditionDefinition>(Status)->Duration = 0.05f;
			Test->TestTrue(TEXT("Untreated infection starts"), Status->ApplyCondition(FallenEraCombatGameplayTags::State_Condition_Infection));
		}
		virtual bool Update() override
		{
			TestWorld.World->Tick(LEVELTICK_TimeOnly, 0.1f);
			TestWorld.World->GetTimerManager().Tick(0.1f);
			if (++Attempts < 5 && Status->HasCondition(FallenEraCombatGameplayTags::State_Condition_Infection)) { return false; }
			Test->TestEqual(TEXT("Natural infection expiry kills"), TestWorld.Stats->GetHealth(), 0.0f);
			Test->TestFalse(TEXT("Natural expiry removes component state"), Status->HasCondition(FallenEraCombatGameplayTags::State_Condition_Infection));
			return true;
		}
	private:
		FFE_ReviewTestWorld TestWorld;
		UFE_CharacterStatusComponent* Status = nullptr;
		FAutomationTestBase* Test;
		int32 Attempts = 0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_ConditionReviewTest, "FallenEra.Combat.Review.ConditionLifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFE_ConditionReviewTest::RunTest(const FString& Parameters)
{
	FFE_ReviewTestWorld TestWorld;
	UFE_CharacterStatusComponent* Status = NewObject<UFE_CharacterStatusComponent>(TestWorld.Character);
	Status->RegisterComponent();
	Status->BeginPlay();
	const FGameplayTag Bleeding = FallenEraCombatGameplayTags::State_Condition_Bleeding;
	const FGameplayTag Infection = FallenEraCombatGameplayTags::State_Condition_Infection;
	TestTrue(TEXT("Bleeding GE applied"), Status->ApplyCondition(Bleeding));
	TestTrue(TEXT("GAS grants condition tag"), TestWorld.ASC->HasMatchingGameplayTag(Bleeding));
	TestEqual(TEXT("One UI condition"), Status->GetActiveConditionsView().Num(), 1);
	TestTrue(TEXT("Reapply refreshes rather than duplicates"), Status->ApplyCondition(Bleeding));
	TestEqual(TEXT("One UI condition after refresh"), Status->GetActiveConditionsView().Num(), 1);
	TestEqual(TEXT("One GAS tag after refresh"), TestWorld.ASC->GetTagCount(Bleeding), 1);
	const auto Handles = TestWorld.ASC->GetActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(Bleeding)));
	if (TestEqual(TEXT("One lifetime effect"), Handles.Num(), 1))
	{
		const FActiveGameplayEffect* Effect = TestWorld.ASC->GetActiveGameplayEffect(Handles[0]);
		TestEqual(TEXT("GAS periodic interval"), Effect->Spec.GetPeriod(), 2.0f);
		TestEqual(TEXT("GAS duration"), Effect->Spec.GetDuration(), 10.0f);
	}
	TestWorld.ASC->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(Bleeding));
	TestFalse(TEXT("External cleanse updates component"), Status->HasCondition(Bleeding));
	TestEqual(TEXT("External cleanse removes UI"), Status->GetActiveConditionsView().Num(), 0);
	TestTrue(TEXT("Infection starts"), Status->ApplyCondition(Infection));
	TestWorld.ASC->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(Infection));
	TestFalse(TEXT("Cleansed infection removed"), Status->HasCondition(Infection));
	TestEqual(TEXT("Cleansing infection must not kill"), TestWorld.Stats->GetHealth(), 100.0f);

	FGameplayEffectApplicationQuery Immunity;
	Immunity.BindLambda([](const FActiveGameplayEffectsContainer&, const FGameplayEffectSpec&) { return false; });
	TestWorld.ASC->GameplayEffectApplicationQueries.Add(Immunity);
	TestFalse(TEXT("Immune condition rejected"), Status->ApplyCondition(Bleeding));
	TestFalse(TEXT("No immune ghost tag"), TestWorld.ASC->HasMatchingGameplayTag(Bleeding));
	TestEqual(TEXT("No immune ghost UI"), Status->GetActiveConditionsView().Num(), 0);
	TestWorld.ASC->GameplayEffectApplicationQueries.Reset();
	TestTrue(TEXT("Condition allowed after immunity ends"), Status->ApplyCondition(Bleeding));
	Status->EndPlay(EEndPlayReason::Destroyed);
	TestFalse(TEXT("Pawn teardown cleans surviving ASC"), TestWorld.ASC->HasMatchingGameplayTag(Bleeding));
	ADD_LATENT_AUTOMATION_COMMAND(FFE_NaturalConditionExpiryCommand(this));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_DeathReviewTest, "FallenEra.Combat.Review.DeathState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFE_DeathReviewTest::RunTest(const FString& Parameters)
{
	FFE_ReviewTestWorld TestWorld;
	UFE_CombatComponent* Combat = NewObject<UFE_CombatComponent>(TestWorld.Character);
	Combat->RegisterComponent();
	Combat->BeginPlay();
	Combat->RefreshAbilitySystem();
	TestTrue(TEXT("Living actor can attack"), UFE_CombatComponent::CanActorAttack(TestWorld.Character));
	TestWorld.ASC->AddLooseGameplayTag(FallenEraGameplayTags::State_Stunned);
	TestFalse(TEXT("Stun blocks firing and equipment changes"), UFE_CombatComponent::CanActorAttack(TestWorld.Character));
	TestWorld.ASC->RemoveLooseGameplayTag(FallenEraGameplayTags::State_Stunned);
	TestTrue(TEXT("Stun removal permits attacks"), UFE_CombatComponent::CanActorAttack(TestWorld.Character));
	TestWorld.ASC->AddLooseGameplayTag(FallenEraGameplayTags::State_Dead);
	TestFalse(TEXT("Death event disables collision"), TestWorld.Character->GetActorEnableCollision());
	TestFalse(TEXT("Dead actors cannot fire"), UFE_CombatComponent::CanActorAttack(TestWorld.Character));
	TestFalse(TEXT("Dead actor cannot damage through default path"), Combat->ApplyDamage(nullptr));
	TestEqual(TEXT("Dead actor charge reticle reset"), Combat->GetBowChargeAlpha(), 0.0f);
	Combat->EndPlay(EEndPlayReason::Destroyed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_CollisionPoolReviewTest, "FallenEra.Combat.Review.CollisionAndPool",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFE_CollisionPoolReviewTest::RunTest(const FString& Parameters)
{
	const UCollisionProfile* Profiles = UCollisionProfile::Get();
	TestEqual(TEXT("Preserve building channel 2"), Profiles->ReturnChannelNameFromContainerIndex(FECollisionChannels::BuildPiece), FName("BuildPiece"));
	TestEqual(TEXT("Hitscan channel agrees with ini"), Profiles->ReturnChannelNameFromContainerIndex(FECollisionChannels::PlayerHitscanTrace), FName("PlayerHitscanTrace"));
	TestEqual(TEXT("Enemy trace agrees with ini"), Profiles->ReturnChannelNameFromContainerIndex(FECollisionChannels::EnemyTrace), FName("EnemyTrace"));
	const FMapProperty* Pools = FindFProperty<FMapProperty>(UFE_ProjectilePoolSubsystem::StaticClass(), TEXT("Pools"));
	TestNotNull(TEXT("Projectile class keys visible to GC"), Pools);
	FFE_ReviewTestWorld TestWorld;
	UFE_ProjectilePoolSubsystem* Pool = TestWorld.World->GetSubsystem<UFE_ProjectilePoolSubsystem>();
	AFE_CombatProjectile* Preview = Pool->AcquireProjectile(AFE_ArrowProjectile::StaticClass(), FTransform::Identity,
		TestWorld.Character, TestWorld.Character, true);
	if (TestNotNull(TEXT("Pooled preview"), Preview))
	{
		Preview->ReturnToPool();
		AFE_CombatProjectile* Reused = Pool->AcquireProjectile(AFE_ArrowProjectile::StaticClass(), FTransform::Identity,
			TestWorld.Character, TestWorld.Character, true);
		TestTrue(TEXT("Preview instance reused"), Reused == Preview);
		if (Reused) { Reused->ReturnToPool(); }
	}
	return true;
}

#endif
