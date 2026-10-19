#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/FallenEraAttributeSet.h"
#include "AbilitySystem/FallenEraGameplayTags.h"
#include "Combat/AI/FEAISettings.h"
#include "Combat/Component/FECombatComponent.h"
#include "Combat/FECombatGameplayTags.h"
#include "Combat/GameplayEffect/FEDamageGameplayEffect.h"
#include "Combat/Tests/FEReactionTestAttackAbility.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameplayEffect.h"
#include "TimerManager.h"

namespace
{
	struct FFE_ReactionTestWorld
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		ACharacter* Character = World->SpawnActor<ACharacter>();
		UAbilitySystemComponent* ASC = NewObject<UAbilitySystemComponent>(Character);
		UFallenEraAttributeSet* Stats = NewObject<UFallenEraAttributeSet>(Character);
		UFE_CombatComponent* Combat = NewObject<UFE_CombatComponent>(Character);
		FFE_ReactionTestWorld()
		{
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			ASC->RegisterComponent();
			ASC->InitializeComponent();
			ASC->AddAttributeSetSubobject(Stats);
			ASC->InitAbilityActorInfo(Character, Character);
			Combat->RegisterComponent();
			Combat->BeginPlay();
		}
		~FFE_ReactionTestWorld()
		{
			Combat->EndPlay(EEndPlayReason::Destroyed);
			World->DestroyWorld(false);
			GEngine->DestroyWorldContext(World);
		}
		FGameplayAbilitySpecHandle GiveAttack()
		{
			UFE_RangedWeaponItemData* Weapon = NewObject<UFE_RangedWeaponItemData>(Character);
			UFE_MeleeAttackData* Attack = NewObject<UFE_MeleeAttackData>(Weapon);
			Attack->IgnoreStaggerUpTo = EFE_StaggerStrength::Light;
			Weapon->Template.AttackActions.Add(Attack);
			return ASC->GiveAbility(FGameplayAbilitySpec(UFE_ReactionTestAttackAbility::StaticClass(), 1, INDEX_NONE, Weapon));
		}
	};

	class FFE_StunExpiryCommand : public IAutomationLatentCommand
	{
	public:
		explicit FFE_StunExpiryCommand(FAutomationTestBase* InTest) : Test(InTest)
		{
			FFE_AttackReactionData Reaction;
			Reaction.StaggerStrength = EFE_StaggerStrength::Heavy;
			Reaction.StunDuration = 0.05f;
			Fixture.Combat->ApplyDamageReaction(nullptr, Reaction);
			Test->TestTrue(TEXT("Stun starts"), Fixture.ASC->HasMatchingGameplayTag(FallenEraGameplayTags::State_Stunned));
		}
		virtual bool Update() override
		{
			Fixture.World->Tick(LEVELTICK_TimeOnly, 0.1f);
			Fixture.World->GetTimerManager().Tick(0.1f);
			if (++Attempts < 5 && Fixture.ASC->HasMatchingGameplayTag(FallenEraGameplayTags::State_Stunned)) { return false; }
			Test->TestFalse(TEXT("Timed stun clears"), Fixture.ASC->HasMatchingGameplayTag(FallenEraGameplayTags::State_Stunned));
			Test->TestTrue(TEXT("Attack allowed after stun"), UFE_CombatComponent::CanActorAttack(Fixture.Character));
			return true;
		}
	private:
		FFE_ReactionTestWorld Fixture;
		FAutomationTestBase* Test;
		int32 Attempts = 0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_StaggerPolicyTest, "FallenEra.Combat.Reaction.StrengthAndImmunity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFE_StaggerPolicyTest::RunTest(const FString& Parameters)
{
	FFE_ReactionTestWorld Fixture;
	UObject* AttackSource = NewObject<UFE_MeleeAttackData>(Fixture.Character);
	FFE_AttackReactionData Reaction;
	Reaction.StunDuration = 1.0f;
	for (uint8 Protection = 0; Protection <= 2; ++Protection)
	{
		Fixture.Combat->SetAttackStaggerProtection(AttackSource, static_cast<EFE_StaggerStrength>(Protection));
		for (uint8 Incoming = 0; Incoming <= 2; ++Incoming)
		{
			Reaction.StaggerStrength = static_cast<EFE_StaggerStrength>(Incoming);
			const FFE_DamageReactionDecision Decision = Fixture.Combat->ResolveDamageReaction(Reaction);
			const bool bExpected = Incoming > Protection;
			TestEqual(TEXT("Only stronger hits interrupt"), Decision.bInterruptAttack, bExpected);
			TestEqual(TEXT("Only stronger hits play reaction"), Decision.bPlayHitReaction, bExpected);
			TestEqual(TEXT("Only stronger hits stun"), Decision.bApplyStun, bExpected);
		}
	}
	Fixture.Combat->SetAttackStaggerProtection(AttackSource, EFE_StaggerStrength::Light);
	UObject* SecondSource = NewObject<UFE_MeleeAttackData>(Fixture.Character);
	Fixture.Combat->SetAttackStaggerProtection(SecondSource, EFE_StaggerStrength::Heavy);
	Fixture.Combat->ClearAttackStaggerProtection(AttackSource);
	TestTrue(TEXT("Ending another attack cannot clear protection"), Fixture.Combat->GetIgnoredStaggerStrength() == EFE_StaggerStrength::Heavy);
	Fixture.Combat->ClearAttackStaggerProtection(SecondSource);
	TestTrue(TEXT("End clears protection"), Fixture.Combat->GetIgnoredStaggerStrength() == EFE_StaggerStrength::None);
	TestTrue(TEXT("AI defaults to light protection"), FSAIAttackMontageSettings().IgnoreStaggerUpTo == EFE_StaggerStrength::Light);

	Reaction.StaggerStrength = EFE_StaggerStrength::Heavy;
	Fixture.ASC->AddLooseGameplayTag(FallenEraCombatGameplayTags::State_Immune_HitReaction);
	const FFE_DamageReactionDecision VisualImmune = Fixture.Combat->ResolveDamageReaction(Reaction);
	TestFalse(TEXT("Visual immunity suppresses only montage"), VisualImmune.bPlayHitReaction);
	TestTrue(TEXT("Visual immunity does not grant stun immunity"), VisualImmune.bApplyStun && VisualImmune.bInterruptAttack);
	Fixture.ASC->AddLooseGameplayTag(FallenEraCombatGameplayTags::State_Immune_Stun);
	const FFE_DamageReactionDecision StunImmune = Fixture.Combat->ResolveDamageReaction(Reaction);
	TestFalse(TEXT("Stun immunity prevents montage/ability interruption"), StunImmune.bInterruptAttack || StunImmune.bPlayHitReaction || StunImmune.bApplyStun);
	TestEqual(TEXT("Stun immunity does not suppress knockback"), Fixture.Combat->CalculateReceivedKnockback(120.0f), 120.0f);
	Fixture.ASC->AddLooseGameplayTag(FallenEraCombatGameplayTags::State_Immune_Knockback);
	TestEqual(TEXT("Knockback immunity independent"), Fixture.Combat->CalculateReceivedKnockback(120.0f), 0.0f);
	TestTrue(TEXT("Existing montage/stun/knockback immunities allow cosmetic physics"),
		UFE_CombatComponent::CanActorPlayPhysicalHitReaction(Fixture.Character));
	Fixture.ASC->AddLooseGameplayTag(FallenEraCombatGameplayTags::State_Immune_PhysicalReaction);
	TestFalse(TEXT("Only dedicated physics immunity blocks cosmetic physics"),
		UFE_CombatComponent::CanActorPlayPhysicalHitReaction(Fixture.Character));
	Fixture.ASC->RemoveLooseGameplayTag(FallenEraCombatGameplayTags::State_Immune_Stun);
	Fixture.ASC->RemoveLooseGameplayTag(FallenEraCombatGameplayTags::State_Immune_HitReaction);
	const FFE_DamageReactionDecision PhysicsOnlyImmune = Fixture.Combat->ResolveDamageReaction(Reaction);
	TestTrue(TEXT("Physical immunity alone cannot prevent stun or montage interruption"),
		PhysicsOnlyImmune.bInterruptAttack && PhysicsOnlyImmune.bPlayHitReaction && PhysicsOnlyImmune.bApplyStun);
	ADD_LATENT_AUTOMATION_COMMAND(FFE_StunExpiryCommand(this));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_StaggerLifecycleTest, "FallenEra.Combat.Reaction.AbilityDamageAndDeath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFE_StaggerLifecycleTest::RunTest(const FString& Parameters)
{
	FFE_ReactionTestWorld Fixture;
	const FGameplayAbilitySpecHandle Handle = Fixture.GiveAttack();
	TestTrue(TEXT("Attack starts"), Fixture.ASC->TryActivateAbility(Handle));
	UFE_ReactionTestAttackAbility* Ability = Cast<UFE_ReactionTestAttackAbility>(Fixture.ASC->FindAbilitySpecFromHandle(Handle)->GetPrimaryInstance());
	if (!TestNotNull(TEXT("Attack instance"), Ability)) { return false; }
	TestTrue(TEXT("Attack registers protection"), Fixture.Combat->GetIgnoredStaggerStrength() == EFE_StaggerStrength::Light);
	TestTrue(TEXT("Trace starts"), Ability->IsTraceActive());
	FFE_AttackReactionData Reaction;
	Reaction.StaggerStrength = EFE_StaggerStrength::Light;
	Reaction.StunDuration = 1.0f;
	Fixture.Combat->ApplyDamageReaction(nullptr, Reaction);
	TestTrue(TEXT("Light hit preserves attack and trace"), Ability->IsActive() && Ability->IsTraceActive());
	TestFalse(TEXT("Resisted hit cannot add stun"), Fixture.ASC->HasMatchingGameplayTag(FallenEraGameplayTags::State_Stunned));
	Reaction.StaggerStrength = EFE_StaggerStrength::Heavy;
	Reaction.StunDuration = 0.0f;
	Fixture.Combat->ApplyDamageReaction(nullptr, Reaction);
	TestFalse(TEXT("Heavy hit cancels attack even with no montage or duration"), Ability->IsActive());
	TestFalse(TEXT("Cancellation ends trace task"), Ability->IsTraceActive());
	TestTrue(TEXT("Cancellation clears ability-owned protection"), Fixture.Combat->GetIgnoredStaggerStrength() == EFE_StaggerStrength::None);
	TestTrue(TEXT("Attack can restart"), Fixture.ASC->TryActivateAbility(Handle));
	Ability->EndAbility(Handle, Fixture.ASC->AbilityActorInfo.Get(), Ability->GetCurrentActivationInfo(), false, false);
	TestTrue(TEXT("Normal EndAbility clears protection"), Fixture.Combat->GetIgnoredStaggerStrength() == EFE_StaggerStrength::None);
	TestFalse(TEXT("Normal end stops trace"), Ability->IsTraceActive());

	// Runtime GE definition is a test fixture only; game assets use an Infinite/Duration BP GE.
	UGameplayEffect* Passive = NewObject<UGameplayEffect>(Fixture.Character);
	Passive->DurationPolicy = EGameplayEffectDurationType::Infinite;
	FGameplayEffectSpec PassiveSpec(Passive, Fixture.ASC->MakeEffectContext(), 1.0f);
	PassiveSpec.DynamicGrantedTags.AddTag(FallenEraCombatGameplayTags::State_Immune_Stun);
	PassiveSpec.DynamicGrantedTags.AddTag(FallenEraCombatGameplayTags::State_Immune_HitReaction);
	PassiveSpec.DynamicGrantedTags.AddTag(FallenEraCombatGameplayTags::State_Immune_Knockback);
	const FActiveGameplayEffectHandle PassiveHandle = Fixture.ASC->ApplyGameplayEffectSpecToSelf(PassiveSpec);
	TestTrue(TEXT("Passive GE grants immunity"), Fixture.ASC->HasMatchingGameplayTag(FallenEraCombatGameplayTags::State_Immune_Stun));
	TestTrue(TEXT("Immune actor starts attack"), Fixture.ASC->TryActivateAbility(Handle));
	Reaction.StunDuration = 1.0f;
	Fixture.Combat->ApplyDamageReaction(nullptr, Reaction);
	TestTrue(TEXT("Immune hit preserves attack/trace"), Ability->IsActive() && Ability->IsTraceActive());

	ACharacter* Attacker = Fixture.World->SpawnActor<ACharacter>();
	UAbilitySystemComponent* SourceASC = NewObject<UAbilitySystemComponent>(Attacker);
	SourceASC->RegisterComponent();
	SourceASC->InitAbilityActorInfo(Attacker, Attacker);
	UFallenEraAttributeSet* SourceStats = NewObject<UFallenEraAttributeSet>(Attacker);
	SourceASC->AddAttributeSetSubobject(SourceStats);
	SourceASC->SetNumericAttributeBase(UFallenEraAttributeSet::GetAttackPowerAttribute(), 20.0f);
	Fixture.ASC->SetNumericAttributeBase(UFallenEraAttributeSet::GetDefensePowerAttribute(), 5.0f);
	UFE_MeleeAttackData* Attack = NewObject<UFE_MeleeAttackData>(Attacker);
	Attack->AttackReactionData = Reaction;
	FFE_CombatDamageRequest Request;
	Request.SourceActor = Attacker;
	Request.TargetActor = Fixture.Character;
	Request.DamageEffectClass = UFE_DamageGameplayEffect::StaticClass();
	Request.AttackData = Attack;
	const float HealthBefore = Fixture.Stats->GetHealth();
	const FFE_CombatDamageResult Result = Fixture.Combat->ApplyGameplayEffectDamage(Request);
	TestTrue(TEXT("Reaction immunity does not block damage GE"), Result.bDamageApplied && Fixture.Stats->GetHealth() < HealthBefore);
	TestTrue(TEXT("Damage does not interrupt immune attack"), Ability->IsActive() && Ability->IsTraceActive());
	SourceASC->SetNumericAttributeBase(UFallenEraAttributeSet::GetAttackPowerAttribute(), 10000.0f);
	Fixture.Combat->ApplyGameplayEffectDamage(Request);
	TestEqual(TEXT("Lethal hit kills despite all reaction immunities"), Fixture.Stats->GetHealth(), 0.0f);
	TestFalse(TEXT("Death ends attack and trace"), Ability->IsActive() || Ability->IsTraceActive());
	TestFalse(TEXT("Death disables collision"), Fixture.Character->GetActorEnableCollision());
	Fixture.ASC->RemoveActiveGameplayEffect(PassiveHandle);
	TestFalse(TEXT("Removing GE clears immunity"), Fixture.ASC->HasMatchingGameplayTag(FallenEraCombatGameplayTags::State_Immune_Stun));
	return true;
}

#endif
