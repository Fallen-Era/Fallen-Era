#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/Tests/FEEnemySettingsTestCharacter.h"
#include "Combat/AI/FEAISettingsDataAsset.h"
#include "Combat/AI/FEEnemyAIController.h"
#include "Combat/Ability/FEEnemyMeleeAttackAbility.h"
#include "Combat/Armor/FEArmorItemData.h"
#include "Combat/Component/FEEquipmentComponent.h"
#include "Combat/Component/FECombatComponent.h"
#include "Combat/GameplayEffect/FEDamageGameplayEffect.h"
#include "Combat/Damage/FEHitZoneMappingData.h"
#include "Combat/Damage/FEHitZoneMultiplierData.h"
#include "Combat/FECombatGameplayTags.h"
#include "AbilitySystem/FallenEraAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/FallenEraAttributeSet.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

namespace
{
	struct FFE_EnemySettingsTestWorld
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		FFE_EnemySettingsTestWorld()
		{
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		}
		~FFE_EnemySettingsTestWorld()
		{
			World->DestroyWorld(false);
			GEngine->DestroyWorldContext(World);
		}
		AFE_EnemySettingsTestCharacter* Spawn(UFE_AISettingsDataAsset* Preset)
		{
			AFE_EnemySettingsTestCharacter* Enemy = World->SpawnActor<AFE_EnemySettingsTestCharacter>();
			Enemy->ConfigurePreset(Preset);
			// This lightweight world skips InitializeActorsForPlay; register default AttributeSets first.
			Enemy->InitializeComponents();
			Enemy->DispatchBeginPlay();
			return Enemy;
		}
	};

	class FFE_EnemySlowedMovementCommand : public IAutomationLatentCommand
	{
	public:
		explicit FFE_EnemySlowedMovementCommand(FAutomationTestBase* InTest) : Test(InTest) {}
		virtual bool Update() override
		{
			if (!Fixture)
			{
				Fixture = MakeUnique<FFE_EnemySettingsTestWorld>();
				UFE_AISettingsDataAsset* Preset = NewObject<UFE_AISettingsDataAsset>();
				Preset->AISettings.PatrolMovementSpeed = 180.0f;
				Enemy = Fixture->Spawn(Preset);
				Fixture->World->GetTimerManager().Tick(0.01f);
				AFE_EnemyAIController* Controller = Fixture->World->SpawnActor<AFE_EnemyAIController>();
				Controller->Possess(Enemy.Get());
				Controller->SetManagedSimulationActive(true); // Avoid needing a nearby player or NavMesh in this fixture.
				return false;
			}
			Fixture->World->GetTimerManager().Tick(0.3f); // Run the real low-frequency AI decision on a new engine frame.
			UFallenEraAbilitySystemComponent* ASC = Enemy->GetEnemyAbilitySystemComponent();
			const float Speed = Enemy->GetCharacterMovement()->MaxWalkSpeed;
			switch (Stage++)
			{
			case 0:
				Test->TestEqual(TEXT("Patrol uses its configured base speed"), Speed, 180.0f);
				ASC->AddLooseGameplayTag(FallenEraCombatGameplayTags::State_Slowed);
				break;
			case 1:
				Test->TestEqual(TEXT("Slowed halves movement on the next decision"), Speed, 90.0f);
				break;
			case 2:
				Test->TestEqual(TEXT("Repeated decisions cannot compound the slow"), Speed, 90.0f);
				ASC->AddLooseGameplayTag(FallenEraCombatGameplayTags::State_Slowed);
				break;
			case 3:
				Test->TestEqual(TEXT("Two overlapping sources still apply one half-speed factor"), Speed, 90.0f);
				ASC->RemoveLooseGameplayTag(FallenEraCombatGameplayTags::State_Slowed);
				break;
			case 4:
				Test->TestEqual(TEXT("Leaving one source preserves the remaining slow"), Speed, 90.0f);
				ASC->RemoveLooseGameplayTag(FallenEraCombatGameplayTags::State_Slowed);
				break;
			default:
				Test->TestEqual(TEXT("Leaving all sources restores the base speed"), Speed, 180.0f);
				return true;
			}
			return false;
		}
	private:
		FAutomationTestBase* Test;
		TUniquePtr<FFE_EnemySettingsTestWorld> Fixture;
		TWeakObjectPtr<AFE_EnemySettingsTestCharacter> Enemy;
		int32 Stage = 0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_EnemySlowedMovementTest, "FallenEra.Combat.AI.SlowedMovement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFE_EnemySlowedMovementTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("State.Slowed is registered as a native gameplay tag"),
		FGameplayTag::RequestGameplayTag(TEXT("State.Slowed"), false).IsValid());
	FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FFE_EnemySlowedMovementCommand>(this));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_EnemyPresetConfigurationTest, "FallenEra.Combat.AI.PresetConfiguration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFE_EnemyPresetConfigurationTest::RunTest(const FString& Parameters)
{
	FFE_EnemySettingsTestWorld Fixture;
	UFE_AISettingsDataAsset* Preset = NewObject<UFE_AISettingsDataAsset>();
	FFE_AIStartupEffect& StartupEffect = Preset->AISettings.StartupEffects.AddDefaulted_GetRef();
	StartupEffect.GameplayEffect = UFE_EnemySettingsTestEffect::StaticClass();
	UFE_ArmorItemData* Armor = NewObject<UFE_ArmorItemData>();
	Armor->Template.EquipmentSlotTag = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Body"));
	Armor->Instance.ItemData.Defense = 5.0f;
	Preset->AISettings.DefaultArmorDataAssets.Add(Armor);
	UFE_HitZoneMappingData* Mapping = NewObject<UFE_HitZoneMappingData>();
	UFE_HitZoneMultiplierData* Multipliers = NewObject<UFE_HitZoneMultiplierData>();
	Preset->AISettings.HitZoneMappingData = Mapping;
	Preset->AISettings.HitZoneMultiplierData = Multipliers;
	AFE_EnemySettingsTestCharacter* Enemy = Fixture.Spawn(Preset);
	UFallenEraAbilitySystemComponent* ASC = Enemy->GetEnemyAbilitySystemComponent();
	TestTrue(TEXT("Preset becomes ready"), Enemy->IsAISettingsCached());
	TestNotNull(TEXT("Preset GA granted"), ASC->FindAbilitySpecFromClass(UFE_EnemyMeleeAttackAbility::StaticClass()));
	TestEqual(TEXT("Preset startup GE initializes attributes"), Enemy->GetEnemyAttributeSet()->GetAttackPower(), 7.0f);
	TestEqual(TEXT("Preset armor applies after startup GE"), Enemy->GetEnemyAttributeSet()->GetDefensePower(), 5.0f);
	TestTrue(TEXT("Preset armor cached"), Enemy->GetEquipmentComponent()->GetEquippedArmor(Armor->Template.EquipmentSlotTag) == Armor);
	TestTrue(TEXT("Preset mapping cached"), Enemy->GetCombatComponent()->GetLoadedHitZoneMappingData() == Mapping);
	TestTrue(TEXT("Preset multipliers cached"), Enemy->GetCombatComponent()->GetLoadedHitZoneMultiplierData() == Multipliers);
	TArray<FSoftObjectPath> PreloadPaths;
	TestTrue(TEXT("Spawn dependency collection succeeds"), Enemy->GatherAISettingsAssetPaths(PreloadPaths, true));
	TestTrue(TEXT("Prewarming includes startup GE"), PreloadPaths.Contains(StartupEffect.GameplayEffect.ToSoftObjectPath()));
	TestTrue(TEXT("Prewarming includes attack GA"), PreloadPaths.Contains(Preset->AISettings.AttackAbilityClass.ToSoftObjectPath()));
	TestTrue(TEXT("Prewarming includes armor"), PreloadPaths.Contains(FSoftObjectPath(Armor)));
	TestTrue(TEXT("Prewarming includes hit zones"), PreloadPaths.Contains(FSoftObjectPath(Mapping)));
	// Simulate a repeated selection notification. Startup GEs must not be applied a second time.
	UFunction* RepNotify = Enemy->FindFunction(TEXT("OnRep_SelectedAISettingsIndex"));
	Enemy->ProcessEvent(RepNotify, nullptr);
	TestEqual(TEXT("Repeated cache does not grant startup GE twice"), Enemy->GetEnemyAttributeSet()->GetAttackPower(), 7.0f);
	TestEqual(TEXT("Repeated cache does not stack armor"), Enemy->GetEnemyAttributeSet()->GetDefensePower(), 5.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_EnemyPresetOnlyTest, "FallenEra.Combat.AI.PresetOnlyConfiguration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFE_EnemyPresetOnlyTest::RunTest(const FString& Parameters)
{
	FFE_EnemySettingsTestWorld Fixture;
	UFE_AISettingsDataAsset* Preset = NewObject<UFE_AISettingsDataAsset>();
	TestNull(TEXT("Character ability-set defaults removed"), FindFProperty<FProperty>(AFE_EnemyCharacter::StaticClass(), TEXT("DefaultAbilitySets")));
	TestNull(TEXT("Character attack GA default removed"), FindFProperty<FProperty>(AFE_EnemyCharacter::StaticClass(), TEXT("AttackAbilityClass")));
	TestNull(TEXT("Migration switch removed"), FindFProperty<FProperty>(FSAISettings::StaticStruct(), TEXT("bUsePresetCombatConfiguration")));
	Preset->AISettings.AttackAbilityClass.Reset();
	AFE_EnemySettingsTestCharacter* Enemy = Fixture.World->SpawnActor<AFE_EnemySettingsTestCharacter>();
	Enemy->ConfigurePreset(Preset);
	UFE_ArmorItemData* OldArmor = NewObject<UFE_ArmorItemData>();
	OldArmor->Template.EquipmentSlotTag = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Body"));
	OldArmor->Instance.ItemData.Defense = 50.0f;
	Enemy->GetEquipmentComponent()->DefaultArmorDataAssets.Add(OldArmor);
	UFE_HitZoneMappingData* OldMapping = NewObject<UFE_HitZoneMappingData>();
	Enemy->GetCombatComponent()->HitZoneMappingData = OldMapping;
	Enemy->GetCombatComponent()->HitZoneMultiplierData = NewObject<UFE_HitZoneMultiplierData>();
	FProperty* OldDamageProperty = FindFProperty<FProperty>(UFE_CombatComponent::StaticClass(), TEXT("DamageEffectClass"));
	FProperty* OldDeathProperty = FindFProperty<FProperty>(UFE_CombatComponent::StaticClass(), TEXT("DeathMontage"));
	*OldDamageProperty->ContainerPtrToValuePtr<TSoftClassPtr<UGameplayEffect>>(Enemy->GetCombatComponent()) = UFE_EnemySettingsTestEffect::StaticClass();
	*OldDeathProperty->ContainerPtrToValuePtr<TSoftObjectPtr<UAnimMontage>>(Enemy->GetCombatComponent()) = NewObject<UAnimMontage>();
	Enemy->InitializeComponents();
	Enemy->DispatchBeginPlay();
	TestTrue(TEXT("Explicit empty preset is ready"), Enemy->IsAISettingsCached());
	TestNull(TEXT("Empty preset GA does not auto-grant an attack"), Enemy->GetEnemyAbilitySystemComponent()->FindAbilitySpecFromClass(UFE_EnemyMeleeAttackAbility::StaticClass()));
	TestEqual(TEXT("Empty preset does not apply old armor"), Enemy->GetEnemyAttributeSet()->GetDefensePower(), 0.0f);
	TestNull(TEXT("Empty preset removes component hit-zone mapping"), Enemy->GetCombatComponent()->GetLoadedHitZoneMappingData());
	TestNull(TEXT("Empty preset removes component hit-zone multipliers"), Enemy->GetCombatComponent()->GetLoadedHitZoneMultiplierData());
	TestTrue(TEXT("Empty damage GE uses native default, not component overrides"), Enemy->GetCachedAttackDamageEffect() == UFE_DamageGameplayEffect::StaticClass());
	TestTrue(TEXT("Old component damage default cleared"), OldDamageProperty->ContainerPtrToValuePtr<TSoftClassPtr<UGameplayEffect>>(Enemy->GetCombatComponent())->IsNull());
	TestTrue(TEXT("Empty preset clears old death montage"), OldDeathProperty->ContainerPtrToValuePtr<TSoftObjectPtr<UAnimMontage>>(Enemy->GetCombatComponent())->IsNull());
	TArray<FSoftObjectPath> Paths;
	Enemy->GatherAISettingsAssetPaths(Paths, true);
	TestFalse(TEXT("Old component rules are not prewarmed"), Paths.Contains(FSoftObjectPath(OldMapping)));
	AddExpectedMessage(TEXT("has no valid AI settings"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	AFE_EnemySettingsTestCharacter* MissingPresetEnemy = Fixture.Spawn(nullptr);
	TestFalse(TEXT("Missing preset cannot start AI"), MissingPresetEnemy->IsAISettingsCached());
	TestFalse(TEXT("Missing preset cannot be marked spawn-ready"), MissingPresetEnemy->GatherAISettingsAssetPaths(Paths, false));
	TestNull(TEXT("Missing preset does not auto-grant an attack"), MissingPresetEnemy->GetEnemyAbilitySystemComponent()->FindAbilitySpecFromClass(UFE_EnemyMeleeAttackAbility::StaticClass()));
	return true;
}

#endif
