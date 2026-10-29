// Fill out your copyright notice in the Description page of Project Settings.

#include "FEBuildDefense.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/FallenEraAttributeSet.h"
#include "Combat/Component/FECombatComponent.h"
#include "Combat/GameplayEffect/FEDamageGameplayEffect.h"

AFEBuildDefense::AFEBuildDefense()
{
	Capacity = 0; // 함정은 칸 없음. 발리스타가 탄약 칸 수를 정한다

	AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));	
	// 공격 전용이라 복제하지 않는다 — 피해는 서버에서만 계산하고, 맞은 쪽의 피해 GE·GameplayCue 는 대상 ASC 로 복제된다.
	// 수가 많은 함정마다 ASC·AttributeSet 이 클라로 가지 않게. 발리스타는 생성자에서 다시 켠다 (공격자 ASC 의 명중 효과 대비)
	AbilitySystem->SetIsReplicated(false);
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	Attributes = CreateDefaultSubobject<UFallenEraAttributeSet>(TEXT("Attributes"));
	Combat = CreateDefaultSubobject<UFE_CombatComponent>(TEXT("Combat"));
}

void AFEBuildDefense::BeginPlay()
{
	Super::BeginPlay();

	// 서버·클라 모두 초기화한다 (클라 쪽은 발리스타 명중 효과용 — 함정은 ASC 를 복제하지 않아 클라에서 쓰이지 않지만 초기화는 무해).
	// 공격력은 서버 값만 쓴다 (피해는 서버에서만 계산)
	AbilitySystem->InitAbilityActorInfo(this, this);
	if (HasAuthority())
	{
		AbilitySystem->SetNumericAttributeBase(UFallenEraAttributeSet::GetAttackPowerAttribute(), AttackPower);
	}
}

bool AFEBuildDefense::CanInteractBuilt(AActor* InstigatorActor) const
{
	return false;
}

bool AFEBuildDefense::DealDamage(AActor* Target)
{
	// 피해 GE 는 C++ 클래스를 직접 넘긴다 — 컴포넌트 기본값(소프트 클래스)의 동기 로드를 피하고 BP 설정 누락도 없다
	return HasAuthority() && Target != nullptr
		&& Combat->ApplyDamageWithEffect(Target, UFE_DamageGameplayEffect::StaticClass());
}