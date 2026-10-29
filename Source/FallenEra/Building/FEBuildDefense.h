// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FEBuildStorage.h"
#include "FEBuildDefense.generated.h"

class UAbilitySystemComponent;
class UFallenEraAttributeSet;
class UFE_CombatComponent;

/**
 * 방어시설 공통 부모 (스파이크·철조망·발리스타).
 * 적·플레이어에게 피해를 줄 때 HT 전투 경로를 그대로 쓴다: 자체 ASC + AttributeSet(AttackPower) + UFE_CombatComponent.
 * → 피해량 = AttackPower − 대상 DefensePower, GameplayEffect·사망·피격 반응·AI 피해 인지까지 무기와 같다 (HT 코드 수정 없음).
 * 방어시설 자신의 체력은 다른 피스와 같은 AFEBuildPiece::Health (구조물 HP 는 GAS 예외, HT 합의). ASC 는 공격 전용이다.
 * 보관함을 상속하는 이유: 발리스타 탄약 칸의 서버 로직(넣기·빼기 검증·리플리케이트·저장)을 그대로 쓴다. UI 는 발리스타 전용 위젯. 함정은 칸 0개라 보관함 기능이 꺼져 있다.
 */
UCLASS(Abstract)
class FALLENERA_API AFEBuildDefense : public AFEBuildStorage
{
	GENERATED_BODY()

public:
	AFEBuildDefense();

protected:
	virtual void BeginPlay() override;
	
	/** 기본은 상호작용 없음 (함정). 발리스타가 탑승으로 오버라이드한다 */
	virtual bool CanInteractBuilt(AActor* InstigatorActor) const override;

	/** [Server Only] Target 에게 피해 1회. 피해 경로(같은 팀 검사 포함)를 탄다. 실제로 들어갔으면 true */
	bool DealDamage(AActor* Target);

	/** 1회 피해의 공격력. 대상 방어력을 뺀 만큼 들어간다. 등급별 값은 BP 자식에서 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallenEra|Building|Defense", meta = (ClampMin = 0))
	float AttackPower = 20.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FallenEra|Building|Defense")
	TObjectPtr<UAbilitySystemComponent> AbilitySystem;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FallenEra|Building|Defense")
	TObjectPtr<UFE_CombatComponent> Combat;

	/** AttackPower 를 담는 AttributeSet. ASC 가 소유자 하위 객체에서 자동으로 찾아 등록한다 */
	UPROPERTY()
	TObjectPtr<UFallenEraAttributeSet> Attributes;
};
