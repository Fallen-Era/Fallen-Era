#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "AbilitySystem/FallenEraAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/FallenEraAttributeSet.h"
#include "Combat/AI/FEAISettingsDataAsset.h"
#include "Combat/AI/FEEnemyAIController.h"
#include "Combat/Animation/FEEnemyAnimInstance.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimMontage.h"
#include "Combat/Component/FECombatComponent.h"
#include "Combat/Component/FEProceduralHitReactionComponent.h"
#include "Combat/Component/FEKnockdownComponent.h"
#include "Combat/Damage/FEHitZoneMappingData.h"
#include "Combat/FECombatGameplayTags.h"
#include "Combat/GameplayEffect/FEDamageGameplayEffect.h"
#include "Combat/Tests/FEEnemySettingsTestCharacter.h"
#include "Combat/Tests/FEReactionTestAttackAbility.h"
#include "AbilitySystem/FallenEraGameplayTags.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "HitReactProfile.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"
#include "TimerManager.h"

namespace
{
	struct FFE_ProceduralReactionFixture
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		AFE_EnemySettingsTestCharacter* Enemy = nullptr;
		UFE_ProceduralHitReactionComponent* Physical = nullptr;
		USkeletalMeshComponent* Mesh = nullptr;
		UFE_AISettingsDataAsset* Preset = nullptr;
		FFE_ProceduralReactionFixture()
		{
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			Preset = NewObject<UFE_AISettingsDataAsset>();
			Preset->AISettings.AttackAbilityClass.Reset();
			Preset->AISettings.HitZoneMappingData = LoadObject<UFE_HitZoneMappingData>(nullptr,
				TEXT("/Game/Combat/DataAssets/HitBoneMultiplier/DA_Enemy1_HitZone.DA_Enemy1_HitZone"));
			Preset->AISettings.PhysicalHitReaction.Profile.LoadSynchronous();
			Preset->AISettings.PhysicalHitReaction.bRequireRecentlyRendered = false;
			Preset->AISettings.PhysicalHitReaction.MaxDistance = 0.0f;
			Enemy = World->SpawnActor<AFE_EnemySettingsTestCharacter>();
			Enemy->ConfigurePreset(Preset);
			Mesh = Enemy->GetMesh();
			Mesh->SetSkeletalMeshAsset(LoadObject<USkeletalMesh>(nullptr,
				TEXT("/Game/Combat/Character/EnemyCharacter/SKM_HT_Enemy.SKM_HT_Enemy")));
			Mesh->SetPhysicsAsset(LoadObject<UPhysicsAsset>(nullptr,
				TEXT("/Game/Combat/Character/EnemyCharacter/Physics/PA_FE_Enemy.PA_FE_Enemy")));
			Mesh->SetCollisionProfileName(TEXT("EnemyMesh"));
			Mesh->bEnableUpdateRateOptimizations = true;
			Enemy->InitializeComponents();
			Enemy->DispatchBeginPlay();
			Physical = Enemy->FindComponentByClass<UFE_ProceduralHitReactionComponent>();
			FlushAsyncLoading();
			World->Tick(LEVELTICK_TimeOnly, 0.1f);
		}
		~FFE_ProceduralReactionFixture()
		{
			World->DestroyWorld(false);
			GEngine->DestroyWorldContext(World);
		}
		FFE_ProceduralHitReactionEvent Hit(FName Bone) const
		{
			FFE_ProceduralHitReactionEvent Event;
			Event.HitBone = Bone;
			Event.Direction = FVector::ForwardVector;
			return Event;
		}
	};

	class FFE_ProceduralSolverCommand : public IAutomationLatentCommand
	{
	public:
		explicit FFE_ProceduralSolverCommand(FAutomationTestBase* InTest, FName InHitBone = TEXT("lowerarm_l"))
			: Test(InTest), HitBone(InHitBone) {}
		virtual bool Update() override
		{
			if (Frame == 0)
			{
				Fixture.World->InitializeActorsForPlay(FURL());
				Fixture.World->BeginPlay();
				Fixture.Enemy->GetCharacterMovement()->DisableMovement();
				// InitializeActorsForPlay resets world time; advance past the profile's initial cooldown.
				Fixture.World->Tick(LEVELTICK_TimeOnly, 0.1f);
				InitialBodyPosition = Fixture.Mesh->GetBodyInstance(HitBone)->GetCOMPosition();
				InitialCapsulePosition = Fixture.Enemy->GetActorLocation();
				if (!Test->TestTrue(TEXT("Solver reaction starts"), Fixture.Physical->TryPlayReaction(Fixture.Hit(HitBone)))) { return true; }
				Test->TestTrue(TEXT("Cooldown repeat retains physical presentation"), Fixture.Physical->TryPlayReaction(Fixture.Hit(HitBone)));
				Fixture.Physical->TickComponent(0.0f, LEVELTICK_All, nullptr);
			}
			Fixture.World->Tick(LEVELTICK_All, 1.0f / 60.0f);
			const FBodyInstance* HitBody = Fixture.Mesh->GetBodyInstance(HitBone);
			MaxBodyMovement = FMath::Max(MaxBodyMovement, FVector::Distance(InitialBodyPosition, HitBody->GetCOMPosition()));
			MaxForwardVelocity = FMath::Max(MaxForwardVelocity, HitBody->GetUnrealWorldVelocity().X);
			if (++Frame < 150) { return false; }
			Test->TestTrue(TEXT("Chaos actually moves the struck limb"), MaxBodyMovement > 1.0f);
			Test->TestTrue(HitBone.ToString() + TEXT(" receives the deferred forward impulse after a solver step"), MaxForwardVelocity > 1.0f);
			Test->TestTrue(TEXT("Reaction naturally expires"), Fixture.Physical->GetPhysicsBlends().IsEmpty());
			Test->TestFalse(TEXT("Natural completion stops component tick"), Fixture.Physical->IsComponentTickEnabled());
			Test->TestTrue(TEXT("Natural completion restores URO"), Fixture.Mesh->bEnableUpdateRateOptimizations);
			Test->TestEqual(TEXT("Natural completion restores QueryOnly"), Fixture.Mesh->GetCollisionEnabled(), ECollisionEnabled::QueryOnly);
			Test->TestFalse(TEXT("Natural completion stops limb simulation"), Fixture.Mesh->GetBodyInstance(HitBone)->IsInstanceSimulatingPhysics());
			Test->TestFalse(TEXT("Pelvis never enters simulation"), Fixture.Mesh->GetBodyInstance(TEXT("pelvis"))->IsInstanceSimulatingPhysics());
			Test->TestTrue(TEXT("Cosmetic impulse does not move the capsule"),
				Fixture.Enemy->GetActorLocation().Equals(InitialCapsulePosition, 1.0f));
			return true;
		}
	private:
		FAutomationTestBase* Test;
		FName HitBone;
		FFE_ProceduralReactionFixture Fixture;
		int32 Frame = 0;
		FVector InitialBodyPosition = FVector::ZeroVector;
		FVector InitialCapsulePosition = FVector::ZeroVector;
		float MaxBodyMovement = 0.0f;
		float MaxForwardVelocity = 0.0f;
	};

	AActor* AddKnockdownTestBox(UWorld* World, const FVector& Location, const FVector& Extent)
	{
		AActor* Actor = World->SpawnActor<AActor>();
		UBoxComponent* Box = NewObject<UBoxComponent>(Actor);
		Actor->SetRootComponent(Box);
		Box->SetBoxExtent(Extent);
		Box->SetCollisionProfileName(TEXT("BlockAll"));
		Box->RegisterComponent();
		Actor->SetActorLocation(Location);
		return Actor;
	}

	void PrepareKnockdownTestWorld(FFE_ProceduralReactionFixture& Fixture)
	{
		Fixture.World->InitializeActorsForPlay(FURL());
		Fixture.World->BeginPlay();
		AddKnockdownTestBox(Fixture.World, FVector(0.0f, 0.0f, -20.0f), FVector(2000.0f, 2000.0f, 20.0f));
		const float HalfHeight = Fixture.Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		Fixture.Enemy->SetActorLocation(FVector(0.0f, 0.0f, HalfHeight + 2.0f));
		Fixture.Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, -HalfHeight));
		Fixture.World->Tick(LEVELTICK_All, 0.1f);
	}

	class FFE_KnockdownSolverCommand : public IAutomationLatentCommand
	{
	public:
		explicit FFE_KnockdownSolverCommand(FAutomationTestBase* InTest, bool bInTestSnapshot = false, float InMeshScale = 1.0f)
			: Test(InTest), bTestSnapshot(bInTestSnapshot), MeshScale(InMeshScale) {}
		virtual bool Update() override
		{
			if (Frame == 0)
			{
				Fixture.Mesh->SetRelativeScale3D(FVector(MeshScale));
				PrepareKnockdownTestWorld(Fixture);
				Knockdown = Fixture.Enemy->FindComponentByClass<UFE_KnockdownComponent>();
				FFE_KnockdownSettings Settings;
				Settings.DownDuration = 0.8f;
				Settings.FallbackGetUpDuration = 0.1f;
				if (bTestSnapshot)
				{
					const UAnimBlueprint* Blueprint = LoadObject<UAnimBlueprint>(nullptr,
						TEXT("/Game/Combat/Character/EnemyCharacter/Animations/ABP_HT_EnemyCharacter.ABP_HT_EnemyCharacter"));
					Fixture.Mesh->SetAnimInstanceClass(Blueprint->GeneratedClass);
					Settings.GetUpFaceUpMontage = LoadObject<UAnimMontage>(nullptr,
						TEXT("/Game/Combat/Character/EnemyCharacter/Animations/AM_Getup_Back.AM_Getup_Back"));
					Settings.GetUpFaceDownMontage = LoadObject<UAnimMontage>(nullptr,
						TEXT("/Game/Combat/Character/EnemyCharacter/Animations/AM_GetUp_Front.AM_GetUp_Front"));
					Controller = Fixture.World->SpawnActor<AFE_EnemyAIController>();
					Controller->Possess(Fixture.Enemy);
					Controller->SetManagedSimulationActive(true);
					const FTransform TargetTransform(FVector(1000.0f, 0.0f, 100.0f));
					Target = Fixture.World->SpawnActorDeferred<AFE_EnemySettingsTestCharacter>(
						AFE_EnemySettingsTestCharacter::StaticClass(), TargetTransform);
					Target->ConfigurePreset(Fixture.Preset);
					Target->FinishSpawning(TargetTransform);
					// UE 5.8 identifies player-controlled pawns by a non-bot PlayerState.
					Target->SetPlayerState(Fixture.World->SpawnActor<APlayerState>());
					Target->GetCharacterMovement()->DisableMovement();
				}
				Knockdown->Configure(Settings);
				InitialPelvis = Fixture.Mesh->GetBodyInstance(TEXT("pelvis"))->GetCOMPosition();
				if (!Test->TestTrue(TEXT("Full Chaos ragdoll starts"), Knockdown->TryStartFromHit(TEXT("calf_r"), FVector::ForwardVector))) { return true; }
			}
			const EFE_KnockdownPhase PreviousPhase = Knockdown->GetPhase();
			const FVector PreviousPelvis = Fixture.Mesh->GetSocketLocation(TEXT("pelvis"));
			Fixture.World->Tick(LEVELTICK_All, 1.0f / 60.0f);
			if (bTestSnapshot && PreviousPhase == EFE_KnockdownPhase::Ragdoll && Knockdown->GetPhase() == EFE_KnockdownPhase::GettingUp)
			{
				bSawSnapshotTransition = true;
				GetUpStartHeight = PreviousPelvis.Z;
				GetUpStartTime = Fixture.World->GetTimeSeconds();
				RecoveryYaw = Fixture.Enemy->GetActorRotation().Yaw;
				const FVector TargetOffset = FRotator(0.0f, RecoveryYaw + 90.0f, 0.0f).Vector() * 150.0f;
				Target->SetActorLocation(Fixture.Enemy->GetActorLocation() + TargetOffset);
				Test->TestTrue(TEXT("AI retains a nearby encounter target during get-up"), Controller->SetEncounterCombatTarget(Target));
				const UFE_EnemyAnimInstance* Anim = Cast<UFE_EnemyAnimInstance>(Fixture.Mesh->GetAnimInstance());
				Test->TestTrue(TEXT("First recovery frame uses the saved physical pose"), Anim->GetRagdollPoseAlpha() > 0.99f);
				const float Displacement = FVector::Distance(PreviousPelvis, Fixture.Mesh->GetSocketLocation(TEXT("pelvis")));
				Test->AddInfo(FString::Printf(TEXT("Normal-tick recovery pelvis displacement: %.3f cm (no forced animation/bone refresh)"), Displacement));
				Test->TestTrue(TEXT("No upright flash when the real timer recovers a fallen ragdoll"), Displacement < 1.0f);
				Test->TestTrue(TEXT("Get-up montage is active on the same frame"), Anim->IsAnyMontagePlaying());
			}
			if (bTestSnapshot && bSawSnapshotTransition && PreviousPhase == EFE_KnockdownPhase::GettingUp)
			{
				const auto* Anim = Cast<UFE_EnemyAnimInstance>(Fixture.Mesh->GetAnimInstance());
				if (Anim->IsAnyMontagePlaying() && !Knockdown->IsKnockedDown()) { bUnlockedBeforeMontageEnded = true; }
				if (Knockdown->GetPhase() == EFE_KnockdownPhase::GettingUp)
				{
					if (Fixture.World->GetTimeSeconds() - GetUpStartTime <= 0.2f)
					{
						MaxEarlyGetUpHeight = FMath::Max(MaxEarlyGetUpHeight, Fixture.Mesh->GetSocketLocation(TEXT("pelvis")).Z);
					}
					bRotatedBeforeMontageEnded |= FMath::Abs(FMath::FindDeltaAngleDegrees(
						RecoveryYaw, Fixture.Enemy->GetActorRotation().Yaw)) > 0.1f;
				}
			}
			if (Knockdown->GetPhase() == EFE_KnockdownPhase::Ragdoll)
			{
				bRootSimulated |= Fixture.Mesh->GetBodyInstance(TEXT("root"))->IsInstanceSimulatingPhysics();
				bMeshDetached |= Fixture.Mesh->GetAttachParent() != Fixture.Enemy->GetCapsuleComponent();
				const FVector Pelvis = Fixture.Mesh->GetBodyInstance(TEXT("pelvis"))->GetCOMPosition();
				MaxDisplacement = FMath::Max(MaxDisplacement, FVector::Distance(Pelvis, InitialPelvis));
				MinPelvisHeight = FMath::Min(MinPelvisHeight, Pelvis.Z);
			}
			if (++Frame < 180) { return false; }
			Test->TestTrue(TEXT("Physics actually moves the whole-body pelvis"), MaxDisplacement > 2.0f);
			Test->TestTrue(TEXT("Floor supports the full ragdoll"), MinPelvisHeight > -100.0f);
			Test->TestFalse(TEXT("Actual solver ragdoll recovers"), Knockdown->IsKnockedDown());
			Test->TestFalse(TEXT("Pelvis simulation ends after recovery"), Fixture.Mesh->GetBodyInstance(TEXT("pelvis"))->IsInstanceSimulatingPhysics());
			Test->TestEqual(TEXT("Movement resumes after physical recovery"), Fixture.Enemy->GetCharacterMovement()->MovementMode.GetValue(), MOVE_Walking);
			Test->TestFalse(TEXT("No idle knockdown ticking"), Knockdown->IsComponentTickEnabled());
			Test->TestFalse(TEXT("Root stays kinematic throughout Chaos simulation"), bRootSimulated);
			Test->TestFalse(TEXT("Mesh remains attached throughout ragdoll"), bMeshDetached);
			if (bTestSnapshot)
			{
				Test->TestTrue(TEXT("Real get-up transition was observed"), bSawSnapshotTransition);
				Test->AddInfo(FString::Printf(TEXT("Initial get-up pelvis height: %.3f -> peak %.3f cm"), GetUpStartHeight, MaxEarlyGetUpHeight));
				Test->TestTrue(TEXT("No root-rotation upward arc during the entire snapshot blend"), MaxEarlyGetUpHeight - GetUpStartHeight < 10.0f);
				const UFE_EnemyAnimInstance* Anim = Cast<UFE_EnemyAnimInstance>(Fixture.Mesh->GetAnimInstance());
				Test->TestEqual(TEXT("Recovery releases snapshot weight"), Anim->GetRagdollPoseAlpha(), 0.0f);
				Test->TestNull(TEXT("Recovery releases cached bone pose"), Anim->GetPoseSnapshot(UFE_EnemyAnimInstance::RagdollPoseSnapshotName));
				Test->TestFalse(TEXT("Movement/attacks remain blocked until the actual montage ends"), bUnlockedBeforeMontageEnded);
				Test->TestFalse(TEXT("AI does not rotate toward its target during the get-up montage"), bRotatedBeforeMontageEnded);
				const float TargetYaw = (Target->GetActorLocation() - Fixture.Enemy->GetActorLocation()).Rotation().Yaw;
				Test->TestTrue(TEXT("AI turns toward its target after the get-up montage ends"),
					FMath::Abs(FMath::FindDeltaAngleDegrees(Fixture.Enemy->GetActorRotation().Yaw, TargetYaw)) < 0.1f);
			}
			return true;
		}
	private:
		FAutomationTestBase* Test;
		FFE_ProceduralReactionFixture Fixture;
		bool bTestSnapshot = false;
		float MeshScale = 1.0f;
		bool bSawSnapshotTransition = false;
		bool bRootSimulated = false;
		bool bMeshDetached = false;
		float GetUpStartHeight = 0.0f;
		float GetUpStartTime = 0.0f;
		float MaxEarlyGetUpHeight = -BIG_NUMBER;
		bool bUnlockedBeforeMontageEnded = false;
		bool bRotatedBeforeMontageEnded = false;
		float RecoveryYaw = 0.0f;
		AFE_EnemyAIController* Controller = nullptr;
		AFE_EnemySettingsTestCharacter* Target = nullptr;
		UFE_KnockdownComponent* Knockdown = nullptr;
		int32 Frame = 0;
		FVector InitialPelvis = FVector::ZeroVector;
		float MaxDisplacement = 0.0f;
		float MinPelvisHeight = BIG_NUMBER;
	};

	/** TimerManager deliberately ticks once per engine frame, even when a test ticks its world repeatedly. */
	class FFE_NextReactionFrameCommand : public IAutomationLatentCommand
	{
	public:
		explicit FFE_NextReactionFrameCommand(TFunction<void()> InAction) : Action(MoveTemp(InAction)) {}
		virtual bool Update() override
		{
			if (!bWaited) { bWaited = true; return false; }
			Action();
			return true;
		}
	private:
		TFunction<void()> Action;
		bool bWaited = false;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_LegKnockdownSolverTest,
	"FallenEra.Combat.Reaction.LegKnockdownSolver", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFE_LegKnockdownSolverTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FFE_KnockdownSolverCommand(this));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_KnockdownNormalTickRecoveryTest,
	"FallenEra.Combat.Reaction.KnockdownNormalTickRecovery", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFE_KnockdownNormalTickRecoveryTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FFE_KnockdownSolverCommand(this, true));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_KnockdownScaledRecoveryTest,
	"FallenEra.Combat.Reaction.KnockdownScaledRecovery", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFE_KnockdownScaledRecoveryTest::RunTest(const FString& Parameters)
{
	// The heavy zombie uses a 1.5x mesh; snapshot alignment must preserve non-unit scale too.
	ADD_LATENT_AUTOMATION_COMMAND(FFE_KnockdownSolverCommand(this, true, 1.5f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_LegKnockdownLifecycleTest,
	"FallenEra.Combat.Reaction.LegKnockdownLifecycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFE_LegKnockdownLifecycleTest::RunTest(const FString& Parameters)
{
	const auto SharedFixture = MakeShared<FFE_ProceduralReactionFixture>();
	FFE_ProceduralReactionFixture& Fixture = *SharedFixture;
	PrepareKnockdownTestWorld(Fixture);
	UFE_KnockdownComponent* Knockdown = Fixture.Enemy->FindComponentByClass<UFE_KnockdownComponent>();
	if (!TestNotNull(TEXT("Enemy owns knockdown component"), Knockdown)) { return false; }
	FFE_KnockdownSettings Settings;
	Settings.DownDuration = 0.2f;
	Settings.FallbackGetUpDuration = 0.1f;
	Knockdown->Configure(Settings);
	const FTransform OriginalMeshTransform = Fixture.Mesh->GetRelativeTransform();
	const ECollisionResponse OriginalTraceResponse = Fixture.Mesh->GetCollisionResponseToChannel(ECC_GameTraceChannel6);
	const ECollisionEnabled::Type OriginalCapsuleCollision = Fixture.Enemy->GetCapsuleComponent()->GetCollisionEnabled();
	TestFalse(TEXT("Arm does not cause full ragdoll"), Knockdown->TryStartFromHit(TEXT("hand_l"), FVector::ForwardVector));
	TestFalse(TEXT("Unknown bone does not cause full ragdoll"), Knockdown->TryStartFromHit(TEXT("missing"), FVector::ForwardVector));
	auto* ASC = Fixture.Enemy->GetEnemyAbilitySystemComponent();
	ASC->AddLooseGameplayTag(FallenEraCombatGameplayTags::State_Immune_PhysicalReaction);
	TestFalse(TEXT("Physical immunity blocks knockdown"), Knockdown->TryStartFromHit(TEXT("calf_r"), FVector::ForwardVector));
	ASC->RemoveLooseGameplayTag(FallenEraCombatGameplayTags::State_Immune_PhysicalReaction);
	ASC->AddLooseGameplayTag(FallenEraCombatGameplayTags::State_Immune_Stun);
	TestTrue(TEXT("Leg hit causes knockdown independently of stun immunity"), Knockdown->TryStartFromHit(TEXT("calf_r"), FVector::ForwardVector));
	TestEqual(TEXT("Full ragdoll phase"), Knockdown->GetPhase(), EFE_KnockdownPhase::Ragdoll);
	TestTrue(TEXT("Pelvis simulates"), Fixture.Mesh->GetBodyInstance(TEXT("pelvis"))->IsInstanceSimulatingPhysics());
	TestTrue(TEXT("Dedicated GAS state is present"), ASC->HasMatchingGameplayTag(FallenEraCombatGameplayTags::State_Combat_KnockedDown));
	TestFalse(TEXT("Cannot attack while down"), UFE_CombatComponent::CanActorAttack(Fixture.Enemy));
	TestEqual(TEXT("Movement is disabled"), Fixture.Enemy->GetCharacterMovement()->MovementMode.GetValue(), MOVE_None);
	TestFalse(TEXT("Procedural limb physics cannot take ownership during full ragdoll"), Fixture.Physical->TryPlayReaction(Fixture.Hit(TEXT("hand_l"))));
	TestEqual(TEXT("Weapon trace response preserved"), Fixture.Mesh->GetCollisionResponseToChannel(ECC_GameTraceChannel6), OriginalTraceResponse);
	ADD_LATENT_AUTOMATION_COMMAND(FFE_NextReactionFrameCommand([this, SharedFixture, Knockdown]()
	{
		SharedFixture->World->Tick(LEVELTICK_TimeOnly, 0.11f);
		SharedFixture->World->GetTimerManager().Tick(0.11f);
		TestTrue(TEXT("Repeated hit is consumed without restart"), Knockdown->TryStartFromHit(TEXT("calf_l"), FVector::ForwardVector));
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FFE_NextReactionFrameCommand([this, SharedFixture, Knockdown, OriginalMeshTransform]()
	{
		auto& Fixture = *SharedFixture;
		Fixture.World->Tick(LEVELTICK_TimeOnly, 0.11f);
		Fixture.World->GetTimerManager().Tick(0.11f);
		Knockdown->TickComponent(0.0f, LEVELTICK_All, nullptr);
		TestEqual(TEXT("Original timer still begins getting up"), Knockdown->GetPhase(), EFE_KnockdownPhase::GettingUp);
		TestFalse(TEXT("Still cannot attack while getting up"), UFE_CombatComponent::CanActorAttack(Fixture.Enemy));
		TestTrue(TEXT("Mesh attachment and transform restored"), Fixture.Mesh->GetAttachParent() == Fixture.Enemy->GetCapsuleComponent() &&
			Fixture.Mesh->GetRelativeTransform().Equals(OriginalMeshTransform));
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FFE_NextReactionFrameCommand([this, SharedFixture, Knockdown, ASC, OriginalCapsuleCollision]()
	{
		auto& Fixture = *SharedFixture;
		Fixture.World->Tick(LEVELTICK_TimeOnly, 0.11f);
		Fixture.World->GetTimerManager().Tick(0.11f);
		Knockdown->TickComponent(0.0f, LEVELTICK_All, nullptr);
		TestFalse(TEXT("Knockdown completed"), Knockdown->IsKnockedDown());
		TestFalse(TEXT("Own GAS tag removed"), ASC->HasMatchingGameplayTag(FallenEraCombatGameplayTags::State_Combat_KnockedDown));
		TestTrue(TEXT("Unrelated stun immunity remains"), ASC->HasMatchingGameplayTag(FallenEraCombatGameplayTags::State_Immune_Stun));
		TestEqual(TEXT("Walking restored"), Fixture.Enemy->GetCharacterMovement()->MovementMode.GetValue(), MOVE_Walking);
		TestEqual(TEXT("Capsule collision restored"), Fixture.Enemy->GetCapsuleComponent()->GetCollisionEnabled(), OriginalCapsuleCollision);
		TestEqual(TEXT("Mesh collision restored"), Fixture.Mesh->GetCollisionEnabled(), ECollisionEnabled::QueryOnly);
		TestFalse(TEXT("Full simulation stopped"), Fixture.Mesh->GetBodyInstance(TEXT("pelvis"))->IsInstanceSimulatingPhysics());
		TestFalse(TEXT("Knockdown tick stops"), Knockdown->IsComponentTickEnabled());
		TestTrue(TEXT("Arm reactions work after recovery"), Fixture.Physical->TryPlayReaction(Fixture.Hit(TEXT("hand_l"))));
		TestTrue(TEXT("Can be knocked down again after recovery"), Knockdown->TryStartFromHit(TEXT("foot_l"), FVector::ForwardVector));
		Fixture.Enemy->GetCombatComponent()->HandleDeath();
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FFE_NextReactionFrameCommand([this, SharedFixture, Knockdown, ASC]()
	{
		auto& Fixture = *SharedFixture;
		Fixture.World->Tick(LEVELTICK_TimeOnly, 1.0f);
		Fixture.World->GetTimerManager().Tick(1.0f);
		TestEqual(TEXT("Death never restores movement"), Fixture.Enemy->GetCharacterMovement()->MovementMode.GetValue(), MOVE_None);
		TestEqual(TEXT("Death keeps NoCollision"), Fixture.Mesh->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
		TestFalse(TEXT("Death removes owned knockdown tag"), ASC->HasMatchingGameplayTag(FallenEraCombatGameplayTags::State_Combat_KnockedDown));
		TestFalse(TEXT("Cannot start after death cleanup"), Knockdown->TryStartFromHit(TEXT("calf_r"), FVector::ForwardVector));
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_KnockdownSnapshotPoseTest,
	"FallenEra.Combat.Reaction.KnockdownSnapshotPose", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFE_KnockdownSnapshotPoseTest::RunTest(const FString& Parameters)
{
	const auto SharedFixture = MakeShared<FFE_ProceduralReactionFixture>();
	auto& Fixture = *SharedFixture;
	PrepareKnockdownTestWorld(Fixture);
	const UAnimBlueprint* Blueprint = LoadObject<UAnimBlueprint>(nullptr,
		TEXT("/Game/Combat/Character/EnemyCharacter/Animations/ABP_HT_EnemyCharacter.ABP_HT_EnemyCharacter"));
	if (!TestNotNull(TEXT("Actual enemy AnimBP"), Blueprint)) { return false; }
	Fixture.Mesh->SetAnimInstanceClass(Blueprint->GeneratedClass);
	Fixture.Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	Fixture.Mesh->bEnableUpdateRateOptimizations = false;
	Fixture.World->Tick(LEVELTICK_All, 0.016f);
	UFE_EnemyAnimInstance* Anim = Cast<UFE_EnemyAnimInstance>(Fixture.Mesh->GetAnimInstance());
	if (!TestNotNull(TEXT("Snapshot animation instance"), Anim)) { return false; }
	TestTrue(TEXT("Snapshot is captured"), Anim->CaptureRagdollPose());
	const FTransform PelvisWorld = Fixture.Mesh->GetComponentSpaceTransforms()[1] * Fixture.Mesh->GetComponentTransform();
	Fixture.Enemy->SetActorLocationAndRotation(FVector(120.0f, 70.0f, 190.0f), FRotator(0.0f, 95.0f, 0.0f));
	Anim->BeginGetUpPoseBlend(0.2f, 0.0f);
	const FPoseSnapshot* After = Anim->GetPoseSnapshot(UFE_EnemyAnimInstance::RagdollPoseSnapshotName);
	if (!TestNotNull(TEXT("Named pose stored in the native animation proxy"), After)) { return false; }
	TestTrue(TEXT("Root remains in the animation reference frame, not the fallen frame"),
		After->LocalTransforms[0].Equals(Fixture.Mesh->GetSkeletalMeshAsset()->GetRefSkeleton().GetRefBonePose()[0]));
	TestTrue(TEXT("Physical pelvis world pose survives capsule repositioning/rotation"),
		(After->LocalTransforms[1] * After->LocalTransforms[0] * Fixture.Mesh->GetComponentTransform()).Equals(PelvisWorld, 0.001f));
	TestEqual(TEXT("First output is entirely the saved pose"), Anim->GetRagdollPoseAlpha(), 1.0f);
	Fixture.World->Tick(LEVELTICK_TimeOnly, 0.1f);
	Anim->NativeUpdateAnimation(0.1f);
	TestTrue(TEXT("Smooth intermediate blend"), FMath::IsNearlyEqual(Anim->GetRagdollPoseAlpha(), 0.5f, 0.001f));
	Fixture.World->Tick(LEVELTICK_TimeOnly, 0.15f);
	Anim->NativeUpdateAnimation(0.15f);
	TestEqual(TEXT("Snapshot cannot reappear at montage blend-out"), Anim->GetRagdollPoseAlpha(), 0.0f);
	Anim->ClearGetUpPoseBlend();
	TestNull(TEXT("Cleanup removes the cached bone pose"), Anim->GetPoseSnapshot(UFE_EnemyAnimInstance::RagdollPoseSnapshotName));

	UFE_KnockdownComponent* Knockdown = Fixture.Enemy->FindComponentByClass<UFE_KnockdownComponent>();
	FFE_KnockdownSettings Settings;
	Settings.DownDuration = 0.1f;
	Settings.GetUpPoseBlendDuration = 0.2f;
	Settings.GetUpFaceUpMontage = LoadObject<UAnimMontage>(nullptr,
		TEXT("/Game/Combat/Character/EnemyCharacter/Animations/AM_Getup_Back.AM_Getup_Back"));
	Settings.GetUpFaceDownMontage = LoadObject<UAnimMontage>(nullptr,
		TEXT("/Game/Combat/Character/EnemyCharacter/Animations/AM_GetUp_Front.AM_GetUp_Front"));
	if (!TestTrue(TEXT("Get-up montages loaded"), Settings.GetUpFaceUpMontage.IsValid() && Settings.GetUpFaceDownMontage.IsValid())) { return false; }
	Knockdown->Configure(Settings);
	// Return over the floor before starting the real physics-to-montage transition.
	Fixture.Enemy->SetActorLocation(FVector(0.0f, 0.0f, Fixture.Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.0f));
	TestTrue(TEXT("Physical transition starts"), Knockdown->TryStartFromHit(TEXT("calf_r"), FVector::ForwardVector));
	ADD_LATENT_AUTOMATION_COMMAND(FFE_NextReactionFrameCommand([this, SharedFixture, Knockdown]()
	{
		auto& Fixture = *SharedFixture;
		Fixture.World->Tick(LEVELTICK_TimeOnly, 0.11f);
		Fixture.World->GetTimerManager().Tick(0.11f);
		TestEqual(TEXT("Timer only requests recovery; physical pose remains until PrePhysics"), Knockdown->GetPhase(), EFE_KnockdownPhase::Ragdoll);
		// Lifecycle unit test explicitly runs the transition phase; the solver regression below uses only normal world ticks.
		Knockdown->TickComponent(0.0f, LEVELTICK_All, nullptr);
		auto* Anim = Cast<UFE_EnemyAnimInstance>(Fixture.Mesh->GetAnimInstance());
		TestEqual(TEXT("Getting up with the actual montage"), Knockdown->GetPhase(), EFE_KnockdownPhase::GettingUp);
		TestTrue(TEXT("Component captured before physics restoration"), Anim->GetRagdollPoseAlpha() > 0.0f);
		TestTrue(TEXT("Get-up montage is playing under the snapshot"), Anim->IsAnyMontagePlaying());
		TestFalse(TEXT("Physics is disabled after capture"), Fixture.Mesh->IsSimulatingPhysics(TEXT("pelvis")));
		const FPoseSnapshot* Snapshot = Anim->GetPoseSnapshot(UFE_EnemyAnimInstance::RagdollPoseSnapshotName);
		if (TestNotNull(TEXT("Recovery has a valid named snapshot"), Snapshot))
		{
			const FReferenceSkeleton& Skeleton = Fixture.Mesh->GetSkeletalMeshAsset()->GetRefSkeleton();
			const int32 HandIndex = Skeleton.FindBoneIndex(TEXT("hand_r"));
			FTransform ExpectedHand = Snapshot->LocalTransforms[HandIndex];
			for (int32 Parent = Skeleton.GetParentIndex(HandIndex); Parent != INDEX_NONE; Parent = Skeleton.GetParentIndex(Parent))
			{
				ExpectedHand *= Snapshot->LocalTransforms[Parent];
			}
			ExpectedHand *= Fixture.Mesh->GetComponentTransform();
			Fixture.Mesh->TickAnimation(0.0f, false);
			Fixture.Mesh->RefreshBoneTransforms();
			TestTrue(TEXT("Actual AnimGraph output keeps the captured hand pose, not an upright idle frame"),
				Fixture.Mesh->GetSocketTransform(TEXT("hand_r")).Equals(ExpectedHand, 0.05f));
		}
		Fixture.Enemy->GetCombatComponent()->HandleDeath();
		TestEqual(TEXT("Death cancels the snapshot bridge immediately"), Anim->GetRagdollPoseAlpha(), 0.0f);
		TestNull(TEXT("Death releases the saved bone pose"), Anim->GetPoseSnapshot(UFE_EnemyAnimInstance::RagdollPoseSnapshotName));
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_LegKnockdownDamageTest,
	"FallenEra.Combat.Reaction.LegKnockdownDamage", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFE_LegKnockdownDamageTest::RunTest(const FString& Parameters)
{
	FFE_ProceduralReactionFixture Fixture;
	UFE_KnockdownComponent* Knockdown = Fixture.Enemy->FindComponentByClass<UFE_KnockdownComponent>();
	ACharacter* Source = Fixture.World->SpawnActor<ACharacter>();
	UAbilitySystemComponent* SourceASC = NewObject<UAbilitySystemComponent>(Source);
	SourceASC->RegisterComponent();
	SourceASC->InitializeComponent();
	SourceASC->AddAttributeSetSubobject(NewObject<UFallenEraAttributeSet>(Source));
	SourceASC->InitAbilityActorInfo(Source, Source);
	SourceASC->SetNumericAttributeBase(UFallenEraAttributeSet::GetAttackPowerAttribute(), 10.0f);
	auto* TargetASC = Fixture.Enemy->GetEnemyAbilitySystemComponent();
	TargetASC->SetNumericAttributeBase(UFallenEraAttributeSet::GetHealthAttribute(), 100.0f);
	const FGameplayAbilitySpecHandle AttackHandle = TargetASC->GiveAbility(FGameplayAbilitySpec(UFE_ReactionTestAttackAbility::StaticClass(), 1));
	TargetASC->TryActivateAbility(AttackHandle);
	FFE_CombatDamageRequest Request;
	Request.SourceActor = Source;
	Request.TargetActor = Fixture.Enemy;
	Request.DamageEffectClass = UFE_DamageGameplayEffect::StaticClass();
	Request.bHasHitResult = true;
	Request.HitResult.BoneName = TEXT("calf_r");
	Request.HitResult.TraceEnd = FVector(100.0f, 0.0f, 0.0f);
	Request.HitType = EFE_DamageHitType::Area;
	Fixture.Enemy->GetCombatComponent()->ApplyGameplayEffectDamage(Request);
	TestFalse(TEXT("Area damage does not use a supplied leg bone"), Knockdown->IsKnockedDown());
	Request.HitType = EFE_DamageHitType::Direct;
	const FFE_CombatDamageResult Result = Fixture.Enemy->GetCombatComponent()->ApplyGameplayEffectDamage(Request);
	TestTrue(TEXT("Positive direct leg damage starts full ragdoll even without AttackData"), Result.AppliedDamage > 0.0f && Knockdown->IsKnockedDown());
	TestFalse(TEXT("Knockdown cancels active attack and trace"), TargetASC->FindAbilitySpecFromHandle(AttackHandle)->IsActive());
	const float HealthBefore = Fixture.Enemy->GetHealth();
	Fixture.Enemy->GetCombatComponent()->ApplyGameplayEffectDamage(Request);
	TestTrue(TEXT("Can continue damaging a downed enemy"), Fixture.Enemy->GetHealth() < HealthBefore);
	TestTrue(TEXT("Limb plugin never runs during knockdown"), Fixture.Physical->GetPhysicsBlends().IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_KnockdownRecoveryFacingTest,
	"FallenEra.Combat.Reaction.KnockdownRecoveryFacing", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFE_KnockdownRecoveryFacingTest::RunTest(const FString& Parameters)
{
	const auto SharedFixture = MakeShared<FFE_ProceduralReactionFixture>();
	auto& Fixture = *SharedFixture;
	PrepareKnockdownTestWorld(Fixture);
	Fixture.Enemy->SetActorRotation(FRotator(0.0f, 37.0f, 0.0f));
	const FQuat PelvisRotation = Fixture.Mesh->GetSocketQuaternion(TEXT("pelvis"));
	FFE_KnockdownSettings Settings;
	Settings.DownDuration = 0.1f;
	Settings.FallbackGetUpDuration = 0.1f;
	// Deterministically simulate a supine skeleton with its head pointing toward world +X.
	Settings.PelvisUpAxis = PelvisRotation.UnrotateVector(FVector::ForwardVector);
	Settings.PelvisFrontAxis = PelvisRotation.UnrotateVector(FVector::UpVector);
	Settings.GetUpYawOffset = 15.0f;
	const float ExpectedYaw = FRotator::NormalizeAxis(180.0f -
		Fixture.Mesh->GetRelativeTransform().TransformVectorNoScale(FVector::YAxisVector).Rotation().Yaw + Settings.GetUpYawOffset);
	auto* Knockdown = Fixture.Enemy->FindComponentByClass<UFE_KnockdownComponent>();
	Knockdown->Configure(Settings);
	TestTrue(TEXT("Facing test starts ragdoll"), Knockdown->TryStartFromHit(TEXT("calf_r"), FVector::ForwardVector));
	const auto EvolvedTransform = MakeShared<FTransform>();
	ADD_LATENT_AUTOMATION_COMMAND(FFE_NextReactionFrameCommand([this, SharedFixture, Knockdown, ExpectedYaw, EvolvedTransform]()
	{
		auto& Fixture = *SharedFixture;
		Fixture.World->Tick(LEVELTICK_TimeOnly, 0.11f);
		Fixture.World->GetTimerManager().Tick(0.11f);
		Knockdown->TickComponent(0.0f, LEVELTICK_All, nullptr);
		TestEqual(TEXT("Getting up"), Knockdown->GetPhase(), EFE_KnockdownPhase::GettingUp);
		TestTrue(TEXT("Recovery faces the fallen pelvis, not pre-ragdoll actor yaw"),
			FMath::Abs(FMath::FindDeltaAngleDegrees(Fixture.Enemy->GetActorRotation().Yaw, ExpectedYaw)) < 0.1f);
		// Standing floor adjustment/root motion must not be undone by replaying the start transform.
		Fixture.Enemy->AddActorWorldOffset(FVector(25.0f, 13.0f, 7.0f));
		*EvolvedTransform = Fixture.Enemy->GetActorTransform();
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FFE_NextReactionFrameCommand([this, SharedFixture, Knockdown, EvolvedTransform]()
	{
		auto& Fixture = *SharedFixture;
		Fixture.World->Tick(LEVELTICK_TimeOnly, 0.11f);
		Fixture.World->GetTimerManager().Tick(0.11f);
		Knockdown->TickComponent(0.0f, LEVELTICK_All, nullptr);
		TestFalse(TEXT("Fallback recovery completes"), Knockdown->IsKnockedDown());
		TestTrue(TEXT("Completion preserves horizontal motion instead of teleporting to recovery start"),
			FVector::DistSquared2D(Fixture.Enemy->GetActorLocation(), EvolvedTransform->GetLocation()) < 0.001f);
		TestTrue(TEXT("Completion preserves the recovered facing"), Fixture.Enemy->GetActorQuat().Equals(EvolvedTransform->GetRotation(), 0.001f));
		// Entering Walking may legitimately lower the capsule onto the floor.
		TestTrue(TEXT("Floor adjustment cannot create an upward recovery jump"), Fixture.Enemy->GetActorLocation().Z <= EvolvedTransform->GetLocation().Z + 2.0f);
		TestTrue(TEXT("Completion leaves no residual launch velocity"), Fixture.Enemy->GetCharacterMovement()->Velocity.IsNearlyZero());
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_KnockdownReplicatedTransitionTest,
	"FallenEra.Combat.Reaction.KnockdownReplicatedTransition", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFE_KnockdownReplicatedTransitionTest::RunTest(const FString& Parameters)
{
	const auto SharedFixture = MakeShared<FFE_ProceduralReactionFixture>();
	auto& Fixture = *SharedFixture;
	PrepareKnockdownTestWorld(Fixture);
	const UAnimBlueprint* Blueprint = LoadObject<UAnimBlueprint>(nullptr,
		TEXT("/Game/Combat/Character/EnemyCharacter/Animations/ABP_HT_EnemyCharacter.ABP_HT_EnemyCharacter"));
	Fixture.Mesh->SetAnimInstanceClass(Blueprint->GeneratedClass);
	UFE_KnockdownComponent* Knockdown = Fixture.Enemy->FindComponentByClass<UFE_KnockdownComponent>();
	Fixture.Enemy->SwapRoles();
	if (!TestFalse(TEXT("Exercise the simulated-proxy path, not authority"), Fixture.Enemy->HasAuthority())) { return false; }
	FStructProperty* Property = FindFProperty<FStructProperty>(Knockdown->GetClass(), TEXT("State"));
	FFE_KnockdownState* Replicated = Property->ContainerPtrToValuePtr<FFE_KnockdownState>(Knockdown);
	Replicated->Phase = EFE_KnockdownPhase::Ragdoll;
	Replicated->HitBone = TEXT("calf_r");
	Replicated->Direction = FVector::ForwardVector;
	Knockdown->ProcessEvent(Knockdown->FindFunction(TEXT("OnRep_State")), nullptr);
	TestFalse(TEXT("OnRep does not mutate physics immediately"), Fixture.Mesh->IsSimulatingPhysics(TEXT("pelvis")));
	ADD_LATENT_AUTOMATION_COMMAND(FFE_NextReactionFrameCommand([this, SharedFixture, Knockdown]()
	{
		SharedFixture->World->Tick(LEVELTICK_All, 0.016f);
		TestTrue(TEXT("Proxy enters ragdoll at the normal pre-physics phase"), SharedFixture->Mesh->IsSimulatingPhysics(TEXT("pelvis")));
		TestFalse(TEXT("Proxy does not run the server ragdoll-follow tick"), Knockdown->IsComponentTickEnabled());
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FFE_NextReactionFrameCommand([this, SharedFixture, Knockdown]()
	{
		auto& Fixture = *SharedFixture;
		FStructProperty* Property = FindFProperty<FStructProperty>(Knockdown->GetClass(), TEXT("State"));
		auto* Replicated = Property->ContainerPtrToValuePtr<FFE_KnockdownState>(Knockdown);
		Replicated->Phase = EFE_KnockdownPhase::GettingUp;
		Replicated->bHasRecoveryTransform = true;
		Replicated->RecoveryLocation = Fixture.Enemy->GetActorLocation();
		Replicated->RecoveryRotation = Fixture.Enemy->GetActorRotation();
		Replicated->GetUpStartTime = Fixture.World->GetTimeSeconds();
		Replicated->GetUpMontage = LoadObject<UAnimMontage>(nullptr,
			TEXT("/Game/Combat/Character/EnemyCharacter/Animations/AM_Getup_Back.AM_Getup_Back"));
		Knockdown->ProcessEvent(Knockdown->FindFunction(TEXT("OnRep_State")), nullptr);
		TestTrue(TEXT("OnRep preserves physics until the ordered transition"), Fixture.Mesh->IsSimulatingPhysics(TEXT("pelvis")));
		Fixture.World->Tick(LEVELTICK_All, 0.016f);
		TestFalse(TEXT("Normal proxy tick disables physics"), Fixture.Mesh->IsSimulatingPhysics(TEXT("pelvis")));
		const auto* Anim = Cast<UFE_EnemyAnimInstance>(Fixture.Mesh->GetAnimInstance());
		TestTrue(TEXT("Proxy begins snapshot recovery in the same normal evaluation"), Anim->GetRagdollPoseAlpha() > 0.9f);
		TestTrue(TEXT("Proxy montage plays"), Anim->IsAnyMontagePlaying());
		TestFalse(TEXT("Transition tick returns to idle on proxy"), Knockdown->IsComponentTickEnabled());
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FFE_NextReactionFrameCommand([this, SharedFixture, Knockdown]()
	{
		auto& Fixture = *SharedFixture;
		FStructProperty* Property = FindFProperty<FStructProperty>(Knockdown->GetClass(), TEXT("State"));
		auto* Replicated = Property->ContainerPtrToValuePtr<FFE_KnockdownState>(Knockdown);
		Replicated->Phase = EFE_KnockdownPhase::None;
		Knockdown->ProcessEvent(Knockdown->FindFunction(TEXT("OnRep_State")), nullptr);
		TestTrue(TEXT("Local recovery still blocks actions before queued completion"), Knockdown->IsKnockedDown());
		Fixture.World->Tick(LEVELTICK_All, 0.016f);
		TestFalse(TEXT("Proxy recovery completes"), Knockdown->IsKnockedDown());
		TestTrue(TEXT("Proxy restores original URO"), Fixture.Mesh->bEnableUpdateRateOptimizations);
		TestFalse(TEXT("No idle proxy ticking"), Knockdown->IsComponentTickEnabled());
		const auto* Anim = Cast<UFE_EnemyAnimInstance>(Fixture.Mesh->GetAnimInstance());
		TestNull(TEXT("Proxy clears snapshot cache"), Anim->GetPoseSnapshot(UFE_EnemyAnimInstance::RagdollPoseSnapshotName));
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_LegKnockdownRecoverySpaceTest,
	"FallenEra.Combat.Reaction.LegKnockdownRecoverySpace", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFE_LegKnockdownRecoverySpaceTest::RunTest(const FString& Parameters)
{
	const auto SharedFixture = MakeShared<FFE_ProceduralReactionFixture>();
	auto& Fixture = *SharedFixture;
	PrepareKnockdownTestWorld(Fixture);
	AActor* Obstruction = AddKnockdownTestBox(Fixture.World, FVector(0.0f, 0.0f, 150.0f), FVector(1000.0f, 1000.0f, 100.0f));
	UFE_KnockdownComponent* Knockdown = Fixture.Enemy->FindComponentByClass<UFE_KnockdownComponent>();
	FFE_KnockdownSettings Settings;
	Settings.DownDuration = 0.1f;
	Knockdown->Configure(Settings);
	TestTrue(TEXT("Starts while recovery space is obstructed"), Knockdown->TryStartFromHit(TEXT("calf_r"), FVector::ForwardVector));
	ADD_LATENT_AUTOMATION_COMMAND(FFE_NextReactionFrameCommand([this, SharedFixture, Knockdown, Obstruction]()
	{
		SharedFixture->World->Tick(LEVELTICK_TimeOnly, 0.11f);
		SharedFixture->World->GetTimerManager().Tick(0.11f);
		Knockdown->TickComponent(0.0f, LEVELTICK_All, nullptr);
		TestEqual(TEXT("Cannot recover inside solid geometry"), Knockdown->GetPhase(), EFE_KnockdownPhase::Ragdoll);
		Obstruction->Destroy();
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FFE_NextReactionFrameCommand([this, SharedFixture, Knockdown]()
	{
		SharedFixture->World->Tick(LEVELTICK_TimeOnly, 0.26f);
		SharedFixture->World->GetTimerManager().Tick(0.26f);
		Knockdown->TickComponent(0.0f, LEVELTICK_All, nullptr);
		TestEqual(TEXT("Recovery resumes when space is free"), Knockdown->GetPhase(), EFE_KnockdownPhase::GettingUp);
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_ProceduralReactionSolverTest,
	"FallenEra.Combat.Reaction.ProceduralSolver", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFE_ProceduralReactionSolverTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FFE_ProceduralSolverCommand(this));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_ProceduralRightLimbSolverTest,
	"FallenEra.Combat.Reaction.ProceduralRightLimbSolver", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFE_ProceduralRightLimbSolverTest::RunTest(const FString& Parameters)
{
	for (const FName Bone : { FName(TEXT("upperarm_r")), FName(TEXT("hand_r")),
		FName(TEXT("thigh_r")), FName(TEXT("calf_r")), FName(TEXT("foot_r")) })
	{
		ADD_LATENT_AUTOMATION_COMMAND(FFE_ProceduralSolverCommand(this, Bone));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_ProceduralReactionBodyOrderTest,
	"FallenEra.Combat.Reaction.ProceduralBodyOrder", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFE_ProceduralReactionBodyOrderTest::RunTest(const FString& Parameters)
{
	for (const FName HitBone : { FName(TEXT("upperarm_r")), FName(TEXT("hand_r")),
		FName(TEXT("thigh_r")), FName(TEXT("calf_r")), FName(TEXT("foot_r")) })
	{
		FFE_ProceduralReactionFixture Fixture;
		FName ImpulseBone;
		const FName RequestedRoot = Fixture.Physical->ResolveSimulationBone(HitBone, ImpulseBone);
		const FString Label = HitBone.ToString();
		TestFalse(Label + TEXT(" has an anatomical root"), RequestedRoot.IsNone());
		if (!TestTrue(Label + TEXT(" starts reaction"), Fixture.Physical->TryPlayReaction(Fixture.Hit(HitBone)))) { continue; }
		TestEqual(Label + TEXT(" chooses requested root, not first stored child"),
			Fixture.Physical->GetPhysicsBlends()[0].SimulatedBoneName, RequestedRoot);
		// A same-frame tick has zero blend weight: the impulse must wait, not warn or be lost.
		Fixture.Physical->TickComponent(0.0f, LEVELTICK_All, nullptr);
		Fixture.Physical->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
		const FBodyInstance* HitBody = Fixture.Mesh->GetBodyInstance(ImpulseBone);
		const FBodyInstance* RootBody = Fixture.Mesh->GetBodyInstance(RequestedRoot);
		TestTrue(Label + TEXT(" simulates the struck body before impulse"), HitBody && HitBody->IsInstanceSimulatingPhysics());
		TestTrue(Label + TEXT(" also simulates the requested parent"), RootBody && RootBody->IsInstanceSimulatingPhysics());
		TestFalse(Label + TEXT(" never simulates pelvis"), Fixture.Mesh->GetBodyInstance(TEXT("pelvis"))->IsInstanceSimulatingPhysics());
		Fixture.Physical->StopReaction();
		const FBodyInstance* StoppedBody = Fixture.Mesh->GetBodyInstance(ImpulseBone);
		TestFalse(Label + TEXT(" stops struck-body simulation on cleanup"), StoppedBody && StoppedBody->IsInstanceSimulatingPhysics());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_ProceduralReactionLifecycleTest,
	"FallenEra.Combat.Reaction.ProceduralLifecycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFE_ProceduralReactionLifecycleTest::RunTest(const FString& Parameters)
{
	FFE_ProceduralReactionFixture Fixture;
	if (!TestNotNull(TEXT("Enemy owns the plugin adapter"), Fixture.Physical)) { return false; }
	TestTrue(TEXT("Preset configures physical reactions"), Fixture.Physical->IsConfiguredForHits());
	TestTrue(TEXT("Preloaded profile initializes plugin"), Fixture.Physical->IsReadyForHits());
	FName ImpulseBone;
	TestEqual(TEXT("Hand resolves to left-arm chain"),
		Fixture.Physical->ResolveSimulationBone(TEXT("hand_l"), ImpulseBone), FName(TEXT("upperarm_l")));
	TestEqual(TEXT("Impulse stays at struck body"), ImpulseBone, FName(TEXT("hand_l")));
	TestEqual(TEXT("Head resolves within head zone"),
		Fixture.Physical->ResolveSimulationBone(TEXT("head"), ImpulseBone), FName(TEXT("neck_01")));
	TestTrue(TEXT("Torso uses montage fallback"),
		Fixture.Physical->ResolveSimulationBone(TEXT("spine_03"), ImpulseBone).IsNone());
	TestTrue(TEXT("Unknown bones do not simulate a different body"),
		Fixture.Physical->ResolveSimulationBone(TEXT("unknown"), ImpulseBone).IsNone());
	TestFalse(TEXT("Torso does not start physics"), Fixture.Physical->TryPlayReaction(Fixture.Hit(TEXT("spine_03"))));
	if (!TestTrue(TEXT("Limb starts plugin reaction"), Fixture.Physical->TryPlayReaction(Fixture.Hit(TEXT("lowerarm_l"))))) { return false; }
	TestEqual(TEXT("Reaction enables physics collision"), Fixture.Mesh->GetCollisionEnabled(), ECollisionEnabled::QueryAndPhysics);
	TestFalse(TEXT("URO disabled only during reaction"), Fixture.Mesh->bEnableUpdateRateOptimizations);
	Fixture.Physical->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Actual limb body simulates"), Fixture.Mesh->GetBodyInstance(TEXT("lowerarm_l"))->IsInstanceSimulatingPhysics());
	Fixture.Enemy->GetCombatComponent()->HandleDeath();
	TestTrue(TEXT("Death clears all blends"), Fixture.Physical->GetPhysicsBlends().IsEmpty());
	TestFalse(TEXT("Death stops plugin tick"), Fixture.Physical->IsComponentTickEnabled());
	TestTrue(TEXT("Death restores URO"), Fixture.Mesh->bEnableUpdateRateOptimizations);
	TestEqual(TEXT("Cleanup cannot undo death collision"), Fixture.Mesh->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	TestFalse(TEXT("Late hit cannot restart a dead enemy"), Fixture.Physical->TryPlayReaction(Fixture.Hit(TEXT("hand_l"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_ProceduralReactionDamageTest,
	"FallenEra.Combat.Reaction.ProceduralDamageRouting", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFE_ProceduralReactionDamageTest::RunTest(const FString& Parameters)
{
	FFE_ProceduralReactionFixture Fixture;
	ACharacter* Source = Fixture.World->SpawnActor<ACharacter>();
	UAbilitySystemComponent* SourceASC = NewObject<UAbilitySystemComponent>(Source);
	SourceASC->RegisterComponent();
	SourceASC->InitializeComponent();
	SourceASC->AddAttributeSetSubobject(NewObject<UFallenEraAttributeSet>(Source));
	SourceASC->InitAbilityActorInfo(Source, Source);
	SourceASC->SetNumericAttributeBase(UFallenEraAttributeSet::GetAttackPowerAttribute(), 10.0f);
	Fixture.Enemy->GetEnemyAbilitySystemComponent()->SetNumericAttributeBase(UFallenEraAttributeSet::GetHealthAttribute(), 100.0f);
	UFE_MeleeAttackData* Attack = NewObject<UFE_MeleeAttackData>(Source);
	Attack->AttackReactionData.StaggerStrength = EFE_StaggerStrength::Light;
	Fixture.Enemy->GetCombatComponent()->SetAttackStaggerProtection(Attack, EFE_StaggerStrength::Light);
	FFE_CombatDamageRequest Request;
	Request.SourceActor = Source;
	Request.TargetActor = Fixture.Enemy;
	Request.DamageEffectClass = UFE_DamageGameplayEffect::StaticClass();
	Request.AttackData = Attack;
	Request.bHasHitResult = true;
	Request.HitResult.BoneName = TEXT("lowerarm_l");
	Request.HitResult.TraceEnd = FVector(100.0f, 0.0f, 0.0f);
	const FFE_CombatDamageResult Result = Fixture.Enemy->GetCombatComponent()->ApplyGameplayEffectDamage(Request);
	TestTrue(TEXT("Existing GAS damage still applies"), Result.bDamageApplied && Result.AppliedDamage > 0.0f);
	TestFalse(TEXT("Light hit preserves attack protection"), Fixture.Enemy->GetCombatComponent()->ResolveDamageReaction(Attack->AttackReactionData).bInterruptAttack);
	TestFalse(TEXT("Resisted light hit still receives cosmetic physics"), Fixture.Physical->GetPhysicsBlends().IsEmpty());
	const int32 BlendCountBeforeArea = Fixture.Physical->GetPhysicsBlends().Num();
	Fixture.World->Tick(LEVELTICK_TimeOnly, 0.1f);
	Request.HitType = EFE_DamageHitType::Area;
	Fixture.Enemy->GetCombatComponent()->ApplyGameplayEffectDamage(Request);
	TestEqual(TEXT("Area damage never invents an anatomical hit"), Fixture.Physical->GetPhysicsBlends().Num(), BlendCountBeforeArea);
	UFallenEraAbilitySystemComponent* TargetASC = Fixture.Enemy->GetEnemyAbilitySystemComponent();
	TargetASC->AddLooseGameplayTag(FallenEraCombatGameplayTags::State_Immune_HitReaction);
	TargetASC->AddLooseGameplayTag(FallenEraCombatGameplayTags::State_Immune_Stun);
	TargetASC->AddLooseGameplayTag(FallenEraCombatGameplayTags::State_Immune_Knockback);
	TestTrue(TEXT("Existing gameplay/montage immunities permit physical presentation"), Fixture.Physical->CanHitReact());
	UFE_RangedWeaponItemData* DefendingWeapon = NewObject<UFE_RangedWeaponItemData>(Fixture.Enemy);
	DefendingWeapon->Template.AttackActions.Add(NewObject<UFE_MeleeAttackData>(DefendingWeapon));
	const FGameplayAbilitySpecHandle AttackHandle = TargetASC->GiveAbility(
		FGameplayAbilitySpec(UFE_ReactionTestAttackAbility::StaticClass(), 1, INDEX_NONE, DefendingWeapon));
	TestTrue(TEXT("Immune enemy can start its attack"), TargetASC->TryActivateAbility(AttackHandle));
	UFE_ReactionTestAttackAbility* DefendingAbility = Cast<UFE_ReactionTestAttackAbility>(
		TargetASC->FindAbilitySpecFromHandle(AttackHandle)->GetPrimaryInstance());
	if (!TestNotNull(TEXT("Defending attack instance"), DefendingAbility)) { return false; }
	Fixture.World->Tick(LEVELTICK_TimeOnly, 0.1f);
	Request.HitType = EFE_DamageHitType::Direct;
	Request.HitResult.BoneName = TEXT("hand_r");
	Attack->AttackReactionData.StaggerStrength = EFE_StaggerStrength::Heavy;
	Attack->AttackReactionData.StunDuration = 1.0f;
	FName ImpulseBone;
	const FName ExpectedRoot = Fixture.Physical->ResolveSimulationBone(Request.HitResult.BoneName, ImpulseBone);
	TestFalse(TEXT("Incoming right-arm bone has a valid simulation root"), ExpectedRoot.IsNone());
	const FFE_CombatDamageResult ImmuneResult = Fixture.Enemy->GetCombatComponent()->ApplyGameplayEffectDamage(Request);
	TestTrue(TEXT("Immune enemy still loses health"), ImmuneResult.bDamageApplied && ImmuneResult.AppliedDamage > 0.0f);
	TestTrue(TEXT("Stun immunity preserves the attack and trace"), DefendingAbility->IsActive() && DefendingAbility->IsTraceActive());
	TestFalse(TEXT("Stun immunity does not grant stunned state"), TargetASC->HasMatchingGameplayTag(FallenEraGameplayTags::State_Stunned));
	TestTrue(TEXT("Server still dispatches right-arm physics through both old immunity tags"),
		Fixture.Physical->GetPhysicsBlends().ContainsByPredicate([ExpectedRoot](const FHitReactPhysics& Blend)
		{
			return Blend.SimulatedBoneName == ExpectedRoot;
		}));
	TargetASC->AddLooseGameplayTag(FallenEraCombatGameplayTags::State_Immune_PhysicalReaction);
	TestFalse(TEXT("Dedicated physical-immunity tag blocks plugin"), Fixture.Physical->TryPlayReaction(Fixture.Hit(TEXT("hand_l"))));
	Fixture.Physical->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("New physical immunity also clears ongoing blends"), Fixture.Physical->GetPhysicsBlends().IsEmpty());
	Fixture.Enemy->GetCombatComponent()->ApplyGameplayEffectDamage(Request);
	TestTrue(TEXT("Server blocks physical events while dedicated immunity is present"), Fixture.Physical->GetPhysicsBlends().IsEmpty());
	TargetASC->RemoveLooseGameplayTag(FallenEraCombatGameplayTags::State_Immune_PhysicalReaction);
	Fixture.World->Tick(LEVELTICK_TimeOnly, 0.1f);
	TestTrue(TEXT("Removing physical immunity restores physics without removing stun immunity"),
		Fixture.Physical->TryPlayReaction(Fixture.Hit(TEXT("hand_l"))));
	return true;
}

#endif
