#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/Tests/FEEnemyObstacleTestActor.h"
#include "Combat/Tests/FEEnemySettingsTestCharacter.h"
#include "Combat/AI/FEAISettingsDataAsset.h"
#include "Combat/Ability/FEEnemyMeleeAttackAbility.h"
#include "Combat/Collision/FECollisionChannels.h"
#include "Combat/FECombatTeams.h"
#include "AbilitySystem/FallenEraAbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Animation/AnimMontage.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/WorldSettings.h"
#include "Navigation/PathFollowingComponent.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

namespace
{
	struct FFE_ObstacleFixture
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		AFE_EnemySettingsTestCharacter* Enemy;
		AFE_EnemyObstacleTestActor* Goal;
		AFE_EnemyObstacleTestActor* Wall;
		AFE_EnemyObstacleTestController* Controller;
		FFE_ObstacleFixture()
		{
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->GetWorldSettings()->MaxUndilatedFrameTime = 1.0f;
			UFE_AISettingsDataAsset* Preset = NewObject<UFE_AISettingsDataAsset>();
			Preset->AISettings.StartupEffects.AddDefaulted_GetRef().GameplayEffect = UFE_EnemySettingsTestEffect::StaticClass();
			FSAIAttackMontageSettings& Attack = Preset->AISettings.AttackMontages.AddDefaulted_GetRef();
			Attack.AttackMontage = NewObject<UAnimMontage>(Preset);
			FindFProperty<FFloatProperty>(UAnimMontage::StaticClass(), TEXT("SequenceLength"))
				->SetPropertyValue_InContainer(Attack.AttackMontage, 1.0f);
			Enemy = World->SpawnActor<AFE_EnemySettingsTestCharacter>();
			Enemy->ConfigurePreset(Preset);
			Enemy->InitializeComponents();
			Enemy->DispatchBeginPlay();
			Goal = World->SpawnActor<AFE_EnemyObstacleTestActor>(FVector(1000, 0, 0), FRotator::ZeroRotator);
			Goal->SetActorEnableCollision(false);
			Wall = World->SpawnActor<AFE_EnemyObstacleTestActor>(FVector(1000, 0, 0), FRotator::ZeroRotator);
			Wall->Box->SetRelativeLocation(FVector(-860, 0, 0)); // Near surface at x=115, but the actor pivot is 1000 away.
			World->GetTimerManager().Tick(0.01f);
			Controller = World->SpawnActor<AFE_EnemyObstacleTestController>();
			Controller->Possess(Enemy);
			Controller->SetManagedSimulationActive(true);
			Controller->SetEncounterCombatTarget(Goal);
		}
		void Advance(float Seconds = 0.3f)
		{
			World->Tick(LEVELTICK_TimeOnly, Seconds);
			World->GetTimerManager().Tick(Seconds); // TimeOnly advances world time but deliberately skips timers.
		}
		void ResetAttack()
		{
			const FGameplayTag Tag = Enemy->GetAttackMontageSettings()[0].AttackResetEventTag;
			FGameplayEventData Payload;
			Payload.EventTag = Tag;
			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Enemy, Tag, Payload);
		}
		~FFE_ObstacleFixture() { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); }
	};

	enum class EFE_ObstacleScenario { Stalled, Partial, Filters, Progress, Disabled };
	class FFE_ObstacleDecisionCommand : public IAutomationLatentCommand
	{
	public:
		FFE_ObstacleDecisionCommand(FAutomationTestBase* InTest, EFE_ObstacleScenario InScenario) : Test(InTest), Scenario(InScenario) {}
		virtual bool Update() override
		{
			if (!Fixture)
			{
				Fixture = MakeUnique<FFE_ObstacleFixture>();
				if (Scenario == EFE_ObstacleScenario::Partial) { Fixture->Controller->bProvidePartialPath = true; }
				if (Scenario == EFE_ObstacleScenario::Filters) { Fixture->Wall->Team = FECombatTeams::Enemy; }
				if (Scenario == EFE_ObstacleScenario::Disabled)
				{
					FindFProperty<FBoolProperty>(AFE_EnemyAIController::StaticClass(), TEXT("bAttackBlockingObstacles"))
						->SetPropertyValue_InContainer(Fixture->Controller, false);
				}
				return false;
			}
			++Step;
			if (Scenario == EFE_ObstacleScenario::Progress)
			{
				Fixture->Enemy->SetActorLocation(FVector(0, Step % 2 ? 30.0f : 0.0f, 0));
			}
			Fixture->Advance(Step >= 9 ? 0.6f : 0.3f);
			Test->TestTrue(TEXT("Obstacle handling preserves the original pursuit target"), Fixture->Controller->GetCurrentCombatTarget() == Fixture->Goal);
			if (Scenario == EFE_ObstacleScenario::Partial)
			{
				if (Step == 1)
				{
					Test->TestTrue(TEXT("Scripted partial path was accepted"), Fixture->Controller->GetPathFollowingComponent()->HasPartialPath());
					Test->TestFalse(TEXT("Having a partial path alone does not start an attack"), Fixture->Enemy->IsAttackInProgress());
					Fixture->Enemy->SetActorLocation(FVector(40, 0, 0));
					Fixture->Controller->GetPathFollowingComponent()->OnPathFinished(FPathFollowingResult(EPathFollowingResult::Success, FPathFollowingResultFlags::None));
					Test->TestFalse(TEXT("Engine remembers that partial completion did not reach the real goal"), Fixture->Controller->GetPathFollowingComponent()->DidMoveReachGoal());
					return false;
				}
				Test->TestTrue(TEXT("Partial-path arrival attacks before the two-second stall delay"), Fixture->Enemy->IsAttackInProgress());
				Test->TestTrue(TEXT("GA targets the blocker, not its remote pivot or the player"), Fixture->Enemy->GetPendingAttackTarget() == Fixture->Wall);
				return true;
			}
			if (Scenario == EFE_ObstacleScenario::Progress || Scenario == EFE_ObstacleScenario::Disabled)
			{
				Test->TestFalse(TEXT("Progressing or explicitly disabled AI cannot attack the nearby wall"), Fixture->Enemy->IsAttackInProgress());
				return Step == 12;
			}
			if (Step < 8)
			{
				Test->TestFalse(TEXT("Ordinary failed moves wait for the configured progress timeout"), Fixture->Enemy->IsAttackInProgress());
				return false;
			}
			if (Scenario == EFE_ObstacleScenario::Filters)
			{
				if (Step == 8)
				{
					Test->TestFalse(TEXT("Friendly blocker is not attacked"), Fixture->Enemy->IsAttackInProgress());
					Fixture->Wall->Team = FECombatTeams::Player;
					Fixture->Wall->Box->SetCollisionResponseToChannel(FECollisionChannels::EnemyTrace, ECR_Ignore);
					return false;
				}
				if (Step == 9)
				{
					Test->TestFalse(TEXT("Ignored blueprint/preview collision is not attacked"), Fixture->Enemy->IsAttackInProgress());
					Fixture->Wall->Box->SetCollisionResponseToChannel(FECollisionChannels::EnemyTrace, ECR_Block);
					NeutralWall = Fixture->World->SpawnActor<AFE_EnemyObstacleTestActor>(FVector(60, 0, 0), FRotator::ZeroRotator);
					NeutralWall->Team = FGenericTeamId::NoTeam;
					return false;
				}
				if (Step == 10)
				{
					Test->TestFalse(TEXT("Cannot select a damageable wall behind a non-target blocking wall"), Fixture->Enemy->IsAttackInProgress());
					NeutralWall->SetActorEnableCollision(false);
					return false;
				}
				Test->TestTrue(TEXT("Removing the front blocker exposes the eligible wall"), Fixture->Enemy->IsAttackInProgress());
				return true;
			}
			if (Step == 8)
			{
				Test->TestTrue(TEXT("Two seconds without progress starts the existing attack GA"), Fixture->Enemy->IsAttackInProgress());
				Test->TestTrue(TEXT("Stalled attack targets the actual blocking actor"), Fixture->Enemy->GetPendingAttackTarget() == Fixture->Wall);
				Test->TestEqual(TEXT("Decision/probe never directly applies damage"), Fixture->Wall->HitCount, 0);
				MovesBeforeClear = Fixture->Controller->MoveRequests;
				Fixture->Wall->SetActorEnableCollision(false);
				Fixture->ResetAttack();
				return false;
			}
			Test->TestFalse(TEXT("Cleared obstacle is no longer attacked"), Fixture->Enemy->IsAttackInProgress());
			Test->TestTrue(TEXT("Cleared obstacle resumes navigation to the preserved goal"), Fixture->Controller->MoveRequests > MovesBeforeClear);
			return true;
		}
	private:
		FAutomationTestBase* Test;
		EFE_ObstacleScenario Scenario;
		TUniquePtr<FFE_ObstacleFixture> Fixture;
		TWeakObjectPtr<AFE_EnemyObstacleTestActor> NeutralWall;
		int32 Step = 0, MovesBeforeClear = 0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_EnemyObstacleRangeTest, "FallenEra.Combat.AI.Obstacles.SurfaceRangeAndNotifyDamage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFE_EnemyObstacleRangeTest::RunTest(const FString& Parameters)
{
	FFE_ObstacleFixture F;
	F.World->InitializeActorsForPlay(FURL()); // Actor ProcessEvent only dispatches damage interfaces in initialized worlds.
	F.World->Tick(LEVELTICK_All, 0.01f); // Publish newly registered collision bodies to the scene query acceleration structure.
	TestTrue(TEXT("Large/offset world target uses the nearby collision surface"), F.Enemy->IsTargetWithinAttackRange(F.Wall));
	F.Wall->Box->SetRelativeLocation(FVector(-400, 0, 0));
	TestFalse(TEXT("Bounds outside melee reach do not grant unlimited range"), F.Enemy->IsTargetWithinAttackRange(F.Wall));
	F.Wall->Box->SetRelativeLocation(FVector(-860, 0, 0));
	F.Wall->Box->SetCollisionResponseToChannel(FECollisionChannels::EnemyTrace, ECR_Ignore);
	TestFalse(TEXT("Only attack-blocking query collision can establish world-target reach"), F.Enemy->IsTargetWithinAttackRange(F.Wall));
	F.Wall->Box->SetCollisionResponseToChannel(FECollisionChannels::EnemyTrace, ECR_Block);
	ACharacter* Pawn = F.World->SpawnActor<ACharacter>(FVector(200, 0, 0), FRotator::ZeroRotator);
	TestFalse(TEXT("Pawn range remains center-based"), F.Enemy->IsTargetWithinAttackRange(Pawn));
	if (!TestTrue(TEXT("Both character and real GA accept surface-based reach"), F.Enemy->TryStartAttack(F.Wall))) { return false; }
	TestEqual(TEXT("Starting the montage does not bypass notify timing"), F.Wall->HitCount, 0);
	const FGameplayTag StartTag = F.Enemy->GetAttackMontageSettings()[0].TraceStartEventTag;
	TestTrue(TEXT("Start notify tag exists"), StartTag.IsValid());
	FHitResult ProbeHit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(FE_ObstacleTest), false, F.Enemy);
	TestTrue(TEXT("Scene query resolves the blocking wall before testing the notify"),
		F.World->LineTraceSingleByChannel(ProbeHit, F.Enemy->GetActorLocation(), F.Enemy->GetActorLocation() + FVector(180, 0, 0),
			FECollisionChannels::EnemyTrace, QueryParams) && ProbeHit.GetActor() == F.Wall);
	FGameplayEventData Payload;
	Payload.EventTag = StartTag;
	F.Enemy->GetEnemyAbilitySystemComponent()->HandleGameplayEvent(StartTag, &Payload);
	FGameplayAbilitySpec* Spec = F.Enemy->GetEnemyAbilitySystemComponent()->FindAbilitySpecFromClass(UFE_EnemyMeleeAttackAbility::StaticClass());
	UGameplayAbility* Ability = Spec ? Spec->GetPrimaryInstance() : nullptr;
	TestTrue(TEXT("Real melee GA remains active for the notify"), Spec && Spec->IsActive());
	UFunction* Trace = Ability ? Ability->FindFunction(TEXT("ExecuteTrace")) : nullptr;
	if (!TestNotNull(TEXT("Real melee notify trace is callable for the fixture"), Trace)) { return false; }
	Ability->ProcessEvent(Trace, nullptr);
	TestEqual(TEXT("Notify trace reaches Damageable without any construction dependency"), F.Wall->HitCount, 1);
	TestTrue(TEXT("Damage uses the existing configured GE request"), F.Wall->LastRequest.DamageEffectClass == F.Enemy->GetCachedAttackDamageEffect());
	TestEqual(TEXT("Damage retains source ASC AttackPower"), F.Wall->LastRequest.SourceAttackPower, 7.0f);
	Ability->ProcessEvent(Trace, nullptr);
	TestEqual(TEXT("The same swing cannot damage the same wall twice"), F.Wall->HitCount, 1);
	return true;
}

#define FE_OBSTACLE_TEST(TestClass, Name, Scenario) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(TestClass, "FallenEra.Combat.AI.Obstacles." Name, EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter) \
	bool TestClass::RunTest(const FString& Parameters) \
	{ FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FFE_ObstacleDecisionCommand>(this, EFE_ObstacleScenario::Scenario)); return true; }

FE_OBSTACLE_TEST(FFE_EnemyObstacleStallTest, "StallAndResume", Stalled)
FE_OBSTACLE_TEST(FFE_EnemyObstaclePartialTest, "PartialArrival", Partial)
FE_OBSTACLE_TEST(FFE_EnemyObstacleFiltersTest, "CollisionAndTeamFilters", Filters)
FE_OBSTACLE_TEST(FFE_EnemyObstacleProgressTest, "ProgressDoesNotAttack", Progress)
FE_OBSTACLE_TEST(FFE_EnemyObstacleDisabledTest, "Disabled", Disabled)
#undef FE_OBSTACLE_TEST

#endif
