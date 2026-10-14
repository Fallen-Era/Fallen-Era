#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/Tests/FEEnemySettingsTestCharacter.h"
#include "Combat/AI/FEAISettingsDataAsset.h"
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
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_EnemyPresetConfigurationTest, "FallenEra.Combat.AI.PresetConfiguration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFE_EnemyPresetConfigurationTest::RunTest(const FString& Parameters)
{
	FFE_EnemySettingsTestWorld Fixture;
	UFE_AISettingsDataAsset* Preset = NewObject<UFE_AISettingsDataAsset>();
	FSAIStartupEffect& StartupEffect = Preset->AISettings.StartupEffects.AddDefaulted_GetRef();
	StartupEffect.GameplayEffect = UFE_EnemySettingsTestEffect::StaticClass();
	UFE_ArmorItemData* Armor = NewObject<UFE_ArmorItemData>();
	Armor->EquipmentSlotTag = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Body"));
	Armor->ArmorStat.Defense = 5.0f;
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
	TestTrue(TEXT("Preset armor cached"), Enemy->GetEquipmentComponent()->GetEquippedArmor(Armor->EquipmentSlotTag) == Armor);
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
	OldArmor->EquipmentSlotTag = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Body"));
	OldArmor->ArmorStat.Defense = 50.0f;
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
