#include "HT/Character/EnemyCharacter.h"

#include "AbilitySystem/FallenEraAbilitySet.h"
#include "AbilitySystem/FallenEraAbilitySystemComponent.h"
#include "AbilitySystem/FallenEraGameplayTags.h"
#include "AbilitySystem/Attributes/FallenEraAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "HT/Component/CombatComponent.h"
#include "HT/Component/EquipmentComponent.h"

AFE_EnemyCharacter::AFE_EnemyCharacter()
{
	AbilitySystemComponent = CreateDefaultSubobject<UFallenEraAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Full);

	AttributeSet = CreateDefaultSubobject<UFallenEraAttributeSet>(TEXT("AttributeSet"));
	CombatComponent = CreateDefaultSubobject<UFE_CombatComponent>(TEXT("CombatComponent"));
	EquipmentComponent = CreateDefaultSubobject<UFE_EquipmentComponent>(TEXT("EquipmentComponent"));

	bReplicates = true;
}

UAbilitySystemComponent* AFE_EnemyCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

FFE_CombatDamageResult AFE_EnemyCharacter::ReceiveCombatDamage_Implementation(
	const FFE_CombatDamageRequest& DamageRequest)
{
	return CombatComponent
		? CombatComponent->ApplyGameplayEffectDamage(DamageRequest)
		: FFE_CombatDamageResult();
}

void AFE_EnemyCharacter::BeginPlay()
{
	Super::BeginPlay();
	InitializeEnemyAbilitySystem();
	// Component BeginPlay may run before this character initializes its ASC.
	EquipmentComponent->RefreshEquipment();
}

void AFE_EnemyCharacter::InitializeEnemyAbilitySystem()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	AbilitySystemComponent->InitAbilityActorInfo(this, this);

	if (HasAuthority() && !AbilitySystemComponent->HasMatchingGameplayTag(FallenEraGameplayTags::State_AbilitySystem_Initialized))
	{
		for (const UFallenEraAbilitySet* AbilitySet : DefaultAbilitySets)
		{
			if (AbilitySet)
			{
				AbilitySet->GiveToAbilitySystem(AbilitySystemComponent, this);
			}
		}

		AbilitySystemComponent->AddLooseGameplayTag(
			FallenEraGameplayTags::State_AbilitySystem_Initialized,
			1,
			EGameplayTagReplicationState::TagAndCountToAll);
		AbilitySystemComponent->TryActivateAbilitiesOnSpawn();
	}
}

float AFE_EnemyCharacter::GetHealth() const
{
	return AttributeSet ? AttributeSet->GetHealth() : 0.0f;
}

float AFE_EnemyCharacter::GetMaxHealth() const
{
	return AttributeSet ? AttributeSet->GetMaxHealth() : 0.0f;
}

bool AFE_EnemyCharacter::IsDead() const
{
	return AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(FallenEraGameplayTags::State_Dead);
}
