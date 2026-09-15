#include "HT/Component/CombatComponent.h"

#include "AbilitySystem/FallenEraGameplayTags.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "HT/Effect/DamageGameplayEffect.h"
#include "GameplayEffect.h"

UFE_CombatComponent::UFE_CombatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	DamageEffectClass = UFE_DamageGameplayEffect::StaticClass();
	DamageSetByCallerTag = FallenEraGameplayTags::SetByCaller_Damage;
}

UAbilitySystemComponent* UFE_CombatComponent::FindAbilitySystemComponent(AActor* Actor)
{
	if (!Actor)
	{
		return nullptr;
	}

	if (const IAbilitySystemInterface* AbilitySystemInterface = Cast<IAbilitySystemInterface>(Actor))
	{
		return AbilitySystemInterface->GetAbilitySystemComponent();
	}

	return nullptr;
}

bool UFE_CombatComponent::ApplyDamage(AActor* TargetActor, float DamageAmount)
{
	const TSubclassOf<UGameplayEffect> EffectClass = DamageEffectClass.IsNull()
		? UFE_DamageGameplayEffect::StaticClass()
		: DamageEffectClass.LoadSynchronous();

	return ApplyDamageInternal(TargetActor, DamageAmount, EffectClass);
}

bool UFE_CombatComponent::ApplyDamageWithEffect(AActor* TargetActor, float DamageAmount, TSubclassOf<UGameplayEffect> DamageEffectClassOverride)
{
	return ApplyDamageInternal(TargetActor, DamageAmount, DamageEffectClassOverride);
}

bool UFE_CombatComponent::ApplyDamageInternal(AActor* TargetActor, float DamageAmount, TSubclassOf<UGameplayEffect> EffectClass)
{
	AActor* SourceActor = GetOwner();
	if (!SourceActor || !TargetActor || SourceActor == TargetActor || DamageAmount <= 0.0f || !FMath::IsFinite(DamageAmount) || !EffectClass)
	{
		return false;
	}

	// Damage is authoritative. Abilities may still predict their own effects separately when needed.
	if (!SourceActor->HasAuthority())
	{
		return false;
	}

	UAbilitySystemComponent* SourceAbilitySystem = FindAbilitySystemComponent(SourceActor);
	UAbilitySystemComponent* TargetAbilitySystem = FindAbilitySystemComponent(TargetActor);
	if (!SourceAbilitySystem || !TargetAbilitySystem)
	{
		return false;
	}

	FGameplayEffectContextHandle EffectContext = SourceAbilitySystem->MakeEffectContext();
	EffectContext.AddSourceObject(SourceActor);
	EffectContext.AddInstigator(SourceActor, SourceActor);

	const FGameplayEffectSpecHandle EffectSpec = SourceAbilitySystem->MakeOutgoingSpec(EffectClass, 1.0f, EffectContext);
	if (!EffectSpec.IsValid())
	{
		return false;
	}

	EffectSpec.Data->SetSetByCallerMagnitude(DamageSetByCallerTag, DamageAmount);
	TargetAbilitySystem->ApplyGameplayEffectSpecToSelf(*EffectSpec.Data.Get());
	return true;
}
