// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FEBuildPiece.h"
#include "FEBuildTrap.generated.h"

class UAbilitySystemComponent;
class UPrimitiveComponent;
class UFallenEraAttributeSet;
class UFE_CombatComponent;

/** 함정을 밟고 있는 대상 1명. 감속 태그를 건 ASC 를 같이 기억한다 —
 *  플레이어 ASC 는 PlayerState 소유라 폰보다 오래 살아서, 폰이 사라져도 그 ASC 에서 태그를 떼야 한다 */
struct FFEBuildTrapVictim
{
	TWeakObjectPtr<AActor> Actor;
	TWeakObjectPtr<UAbilitySystemComponent> TaggedASC;
};

/**
 * 스파이크·철조망. 밟고 있는 적·플레이어에게 DamageInterval 마다 피해를 주고, 피해를 줄 때마다 자기 내구도가 깎인다.
 * 피해는 HT 전투 경로를 그대로 쓴다: 자체 ASC + AttributeSet(AttackPower) + UFE_CombatComponent
 * → 피해량 = AttackPower − 대상 DefensePower, GameplayEffect·사망·피격 반응·AI 피해 인지까지 무기와 같다 (HT 코드 수정 없음).
 * ASC 는 공격 전용이라 복제하지 않는다. 함정 자신의 체력은 다른 피스와 같은 AFEBuildPiece::Health (구조물 HP 는 GAS 예외, HT 합의).
 * 완성되면 메시가 BuildTrap 콜리전(적·플레이어 Overlap, 공격 트레이스 Ignore)으로 바뀌고 내비 영향을 끈다 —
 * 좀비가 피해 가지 않고 밟고 지나가며, 좀비·플레이어의 공격이 함정에 막히지 않는다.
 * 팀이 없어서(NoTeam) 적·플레이어 모두 피해를 받고, 좀비는 함정을 공격 목표로 삼지 않는다(HT IsValidTarget 은 플레이어 팀만).
 * 철조망은 BP 기본값 SlowTag 로 밟은 대상의 ASC 에 감속 태그를 건다. 실제 감속은 HT 가 태그를 읽어 처리한다.
 */
UCLASS()
class FALLENERA_API AFEBuildTrap : public AFEBuildPiece
{
	GENERATED_BODY()

public:
	AFEBuildTrap();
	
	// IGenericTeamAgentInterface — 팀 없음: 적과 플레이어 모두 피해 대상
	virtual FGenericTeamId GetGenericTeamId() const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnApplyState(EFEBuildPieceState NewState) override;
	
	/** 1회 피해의 공격력. 대상 방어력을 뺀 만큼 들어간다. 등급별 값은 BP 자식에서 (스파이크 25 · 철조망 10) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallenEra|Building|Defense", meta = (ClampMin = 0))
	float AttackPower = 20.f;

	/** 피해 간격(초). 밟고 있는 동안 이 간격마다 1회 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallenEra|Building|Defense", meta = (ClampMin = 0.1))
	float DamageInterval = 1.f;

	/** 피해를 1회 줄 때마다 자기 체력이 깎이는 양 (내구도). 0 이면 닳지 않는다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallenEra|Building|Defense", meta = (ClampMin = 0))
	float SelfDamagePerHit = 10.f;

	/** 밟은 대상 ASC 에 거는 태그 (철조망 = State.Slowed). 비우면 걸지 않는다 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FallenEra|Building|Defense")
	FGameplayTag SlowTag;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FallenEra|Building|Defense")
	TObjectPtr<UAbilitySystemComponent> AbilitySystem;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FallenEra|Building|Defense")
	TObjectPtr<UFE_CombatComponent> Combat;

	/** AttackPower 를 담는 AttributeSet. ASC 가 소유자 하위 객체에서 자동으로 찾아 등록한다 */
	UPROPERTY()
	TObjectPtr<UFallenEraAttributeSet> Attributes;

private:
	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	/** [Server Only] DamageInterval 마다: 사라졌거나 죽은 대상 정리 → 남은 대상마다 피해 + 자기 피해. 대상이 없으면 타이머를 끈다 */
	void TickDamage();

	/** [Server Only] 대상 하나를 목록에서 빼고, 걸어 둔 감속 태그를 뗀다 */
	void RemoveVictimAt(int32 Index);

	TArray<FFEBuildTrapVictim> Victims;
	FTimerHandle DamageTimer;
};
