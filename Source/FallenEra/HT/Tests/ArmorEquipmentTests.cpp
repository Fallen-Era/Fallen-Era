#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/FallenEraAttributeSet.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "HT/Armor/ArmorGameplayTags.h"
#include "HT/Armor/ArmorItemData.h"
#include "HT/Component/EquipmentComponent.h"
#include "HT/Component/CombatComponent.h"
#include "HT/Effect/DamageGameplayEffect.h"
#include "HT/Interface/Damageable.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_ArmorEquipmentTest, "FallenEra.HT.Armor.EquipmentAndDamage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFE_ArmorEquipmentTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Test world"), World))
	{
		return false;
	}
	ACharacter* Character = World->SpawnActor<ACharacter>();
	UAbilitySystemComponent* ASC = NewObject<UAbilitySystemComponent>(Character);
	ASC->RegisterComponent();
	ASC->InitializeComponent();
	UFallenEraAttributeSet* Stats = NewObject<UFallenEraAttributeSet>(Character);
	ASC->AddAttributeSetSubobject(Stats);
	ASC->InitAbilityActorInfo(Character, Character);
	TestTrue(TEXT("Character authority"), Character->HasAuthority());
	TestTrue(TEXT("ASC avatar matches"), ASC->GetAvatarActor() == Character);
	TestTrue(TEXT("Defense set registered"), ASC->HasAttributeSetForAttribute(UFallenEraAttributeSet::GetDefensePowerAttribute()));
	TestTrue(TEXT("ASC component discoverable"), UFE_CombatComponent::FindAbilitySystemComponent(Character) == ASC);
	Stats->InitDefensePower(2.0f);
	Stats->InitAttackPower(10.0f);
	UFE_EquipmentComponent* Equipment = NewObject<UFE_EquipmentComponent>(Character);
	Equipment->RegisterComponent();
	TestTrue(TEXT("Equipment owner matches"), Equipment->GetOwner() == Character);
	Equipment->RefreshEquipment();
	TestTrue(TEXT("Authority initialized"), ASC->IsOwnerActorAuthoritative());
	TestTrue(TEXT("Armor initial setup ready"), Equipment->bInitialArmorApplied);
	UFE_CombatComponent* Combat = NewObject<UFE_CombatComponent>(Character);
	Combat->RegisterComponent();
	ACharacter* Attacker = World->SpawnActor<ACharacter>(FVector(1000.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
	UAbilitySystemComponent* AttackASC = NewObject<UAbilitySystemComponent>(Attacker);
	AttackASC->RegisterComponent();
	AttackASC->InitAbilityActorInfo(Attacker, Attacker);
	UFallenEraAttributeSet* AttackStats = NewObject<UFallenEraAttributeSet>(Attacker);
	AttackASC->AddAttributeSetSubobject(AttackStats);
	AttackStats->InitAttackPower(10.0f);
	auto ApplyGasDamageToCharacter = [&]()
	{
		FFE_CombatDamageRequest Request;
		Request.SourceActor = Attacker;
		Request.TargetActor = Character;
		Request.DamageEffectClass = UFE_DamageGameplayEffect::StaticClass();
		return Combat->ApplyGameplayEffectDamage(Request).bDamageApplied;
	};

	const FGameplayTag Head = FGameplayTag::RequestGameplayTag(TEXT("HT.Equipment.Slot.Head"));
	const FGameplayTag Body = FGameplayTag::RequestGameplayTag(TEXT("HT.Equipment.Slot.Body"));
	UFE_ArmorItemData* Helmet = NewObject<UFE_ArmorItemData>();
	Helmet->EquipmentSlotTag = Head;
	Helmet->ArmorStat.Defense = 3.0f;
	Helmet->ArmorStat.KnockbackResistance = 100.0f;
	UFE_ArmorItemData* Vest = NewObject<UFE_ArmorItemData>();
	Vest->EquipmentSlotTag = Body;
	Vest->ArmorStat.Defense = 4.0f;
	Vest->ArmorStat.KnockbackResistance = 200.0f;

	TestTrue(TEXT("Equip helmet"), Equipment->EquipArmor(Helmet));
	TestEqual(TEXT("Base + helmet defense"), Stats->GetDefensePower(), 5.0f);
	TestTrue(TEXT("Accepted instant damage"), ApplyGasDamageToCharacter());
	TestEqual(TEXT("10 attack - 5 defense = 5 damage"), Stats->GetHealth(), 95.0f);
	TestTrue(TEXT("Equip independent second slot"), Equipment->EquipArmor(Vest));
	TestEqual(TEXT("Two armor slots add"), Stats->GetDefensePower(), 9.0f);
	TestEqual(TEXT("Knockback reduced by both slots"), Combat->CalculateReceivedKnockback(600.0f), 300.0f);
	TestEqual(TEXT("Knockback cannot be negative"), Combat->CalculateReceivedKnockback(50.0f), 0.0f);
	Equipment->RefreshEquipment();
	TestEqual(TEXT("Refresh does not duplicate armor"), Stats->GetDefensePower(), 9.0f);
	Equipment->EquipArmor(Helmet);
	TestEqual(TEXT("Re-equipping same slot does not stack"), Stats->GetDefensePower(), 9.0f);
	TestTrue(TEXT("Remove only head slot"), Equipment->UnequipArmor(Head));
	TestEqual(TEXT("Other slot and base survive removal"), Stats->GetDefensePower(), 6.0f);
	TestEqual(TEXT("Only vest resistance remains"), Stats->GetKnockbackResistance(), 200.0f);
	ASC->AddLooseGameplayTag(FE_ArmorGameplayTags::State_Immune_Knockback);
	TestEqual(TEXT("Immunity bypasses magnitude"), Combat->CalculateReceivedKnockback(600.0f), 0.0f);
	ASC->RemoveLooseGameplayTag(FE_ArmorGameplayTags::State_Immune_Knockback);
	TestEqual(TEXT("Resistance resumes after immunity ends"), Combat->CalculateReceivedKnockback(600.0f), 400.0f);
	AttackASC->SetNumericAttributeBase(UFallenEraAttributeSet::GetAttackPowerAttribute(), 1.0f);
	TestTrue(TEXT("Zero damage is still an accepted hit"), ApplyGasDamageToCharacter());
	TestEqual(TEXT("Defense blocks all health damage"), Stats->GetHealth(), 95.0f);
	TestEqual(TEXT("Zero damage does not imply knockback immunity"), Combat->CalculateReceivedKnockback(600.0f), 400.0f);
	Equipment->UnequipArmor(Body);
	TestEqual(TEXT("Original defense restored"), Stats->GetDefensePower(), 2.0f);
	TestEqual(TEXT("Original resistance restored"), Stats->GetKnockbackResistance(), 0.0f);
	World->DestroyWorld(false);
	return true;
}

#endif
