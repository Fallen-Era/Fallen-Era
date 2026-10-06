# 공격력 기반 피해 변동

## 선택한 방식

기본 공격력의 ±10% 범위에서 평균에 가까운 값이 자주 나오는 **연속형 삼각분포**를 사용한다.
상용 게임의 공식을 그대로 복제한 것이 아니라, 아래 사례와 현재 GAS 구조를 비교해 선택한 프로젝트 설계다.

| 후보 | 장점 | 적용 여부 |
| --- | --- | --- |
| AttackPower + 2d6 - 7 | 공격력 중심, 평균 근처가 자주 나옴 | 미적용: 결과가 11가지뿐이고 ±5 고정이라 성장에 따른 변동 비율이 달라짐 |
| 공격력 ±비율의 균등분포 | 단순하고 성장에 비례함 | 미적용: 중앙과 극단 구간이 같은 빈도로 등장 |
| 공격력 ±비율의 삼각분포 | 성장에 비례하면서 극단 피해를 줄임 | 적용: 난수 두 번, 고정 비용 계산 |
| 이전 피해 제외/셔플백/엔트로피 | 특정 반복이나 불운 연속을 제한할 수 있음 | 미적용: 대상/공격자별 이력 관리와 별도 확률 규칙이 필요함 |

PoE 개발자는 회피에서 엔트로피를 사용해 연속 실패를 제한한다고 설명한다.
이는 명중/회피라는 이진 결과의 연속 불운을 줄이는 장치이며, 일반 피해량에 그대로 적용할 근거는 아니다.
이번 구현에서는 이력을 추가하지 않고 제한된 연속형 변동을 선택했다.
[PoE 개발자 설명](https://www.pathofexile.com/forum/view-thread/11707/filter-account-type/staff/page/10)

Riot의 TFT 14.2 Karthus 변경은 첫 공격의 랜덤 치명타 의존도를 줄인 사례다.
무조건 큰 랜덤 피해를 추가하기보다 작은 변동 범위로 전투 예측 가능성을 유지하는 편이 이번 게임에 적합하다고 판단했다.
[Riot 공식 패치 노트](https://teamfighttactics.leagueoflegends.com/en-au/news/game-updates/teamfight-tactics-patch-14-2-notes/)

UE의 RandomStream은 시드 기반 재현과 [0, 1) 실수 난수를 제공한다.
자동화 통계 검사는 독립적인 고정 시드 스트림을 사용하고, 실제 공격은 매 타격 시 글로벌 난수를 진행시킨다.
타격마다 같은 시드로 초기화하지 않는다.
[Epic FRandomStream 문서](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Core/FRandomStream)

## 공식과 순서

```text
U1, U2          = 독립적인 균등 실수 난수 [0, 1)
CenteredRoll    = U1 + U2 - 1
RolledAttack    = max(AttackPower, 0) * (1 + DamageVarianceRatio * CenteredRoll)
FinalDamage     = max(RolledAttack - max(DefensePower, 0), 0) * HitRegionMultiplier
```

기본 DamageVarianceRatio = 0.1. 이론적인 공격력 범위는 90~110%이며 평균은 100%다.
피해를 정수로 반올림하지 않고 기존 float Health에 적용한다.

- 공격력 20, 방어력 5: 피해 13~17, 평균 15.
- 같은 조건, 머리 배율 2: 피해 26~34, 평균 30.
- 공격력 100, 방어력 20: 피해 70~90, 평균 80.
- 공격력 0: 피해 0. 랜덤 보너스만으로 피해가 생기지 않는다.
- 방어력이 최대 변동 공격력 이상: 모든 피해 0.
- 방어력이 기본 공격력과 같아도 높은 난수에서는 일부 피해가 생길 수 있다.

평균 설명은 0 피해 제한이 작동하지 않을 때에만 성립한다.
방어력 근처에서는 음수 피해를 0으로 제한하므로 평균 최종 피해는 단순 AttackPower - DefensePower보다 높아질 수 있다.
남은 HP보다 큰 피해는 HP가 0에서 제한되며 디버그는 기존처럼 실제 HP 감소량을 표시한다.

## 책임과 적용 범위

- FE_DamageExecutionCalculation에서 서버 권한을 확인한 후 타격당 두 난수를 생성한다.
- 플레이어/적의 근접, 히트스캔, 투사체, 수류탄 등 해당 Execution을 사용하는 GE에 적용된다.
- 근접은 기존 최초 HitResult 부위 규칙을 유지한다.
- 수류탄 Area 피해는 부위 배율 1을 유지하며 대상별 GE 실행에서 난수를 뽑는다.
- AttackPower Attribute/장비 GE는 변경하지 않는다. 임시 변동 공격력으로만 계산한다.
- DamageableInterface의 ASC 없는 대상은 자체 수신 구현을 그대로 사용한다. 이 Execution을 거치지 않는 자체 HP 계산까지 자동 변경되지는 않는다.
- 별도 Health 감소 Modifier/다른 Execution/출혈 HealthLoss GE에는 자동으로 적용되지 않는다.
- 클라이언트는 기존 Health 복제 결과를 사용한다. 추가 Attribute/Replicated 변수/RPC/Tick/대상별 이력 저장은 없다.
- 난수 두 번과 산술 연산만 추가되는 O(1) 계산이다. 실제 프레임 성능을 벤치마크한 결과는 아니다.

## 에디터 설정

기존 FE_DamageExecutionCalculation을 사용하는 데미지 GE는 별도 설정 없이 기본 ±10%를 사용한다.

변동 폭을 바꾸려면:

1. FE_DamageExecutionCalculation을 부모로 BP Execution 클래스를 생성한다.
2. Class Defaults의 FallenEra > Combat > Damage > Damage Variance Ratio를 설정한다.
3. 데미지 GE의 Executions에서 기존 Calculation Class 항목을 해당 BP 클래스로 교체한다. 추가 항목으로 중복 등록하지 않는다.
4. Health 감소 Modifier를 중복 추가하지 않는다.

Ratio 0은 이전 고정 피해 공식, 0.05는 ±5%, 0.1은 ±10%, 0.2는 ±20%이다.
FE_DamageGameplayEffect의 로드 보정은 BP 자식 Execution도 인식하며 마지막으로 설정된 피해 Execution 하나만 유지한다.
플레이 중 공유 Execution 기본 객체의 값을 변경하지 말고 에디터 기본값으로 설정한다.

## 같은 피해가 다시 나올 수 있는가?

그럴 수 있다. 순수 난수는 동일 값 재등장을 금지하지 않는다. float 정밀도, UI 반올림,
완전 방어로 인한 0 피해, 남은 HP 제한으로 인한 실제 감소량 반복도 존재한다.
2d6는 가능한 합이 11개이고 연속 두 번 합이 같을 확률이 146/1296(약 11.27%)이다.
연속형 샘플로 값의 가짓수를 늘렸지만 "표시되는 숫자가 무조건 매번 달라진다"는 기능은 아니다.
특히 낮은 공격력에서 정수 UI를 사용하면 같은 숫자가 자주 표시될 수 있다.
이 경우 작은 차이를 감추지 않으려면 피해 UI/디버그에 소수점을 표시한다.

## 검증

- FallenEra.Combat.Damage.Variance: 최소/중앙/최대, 방어력 순서, 0 제한, 비정상 입력, 설정 범위,
  고정 시드 20,000 샘플의 평균/범위/중앙 집중도를 검사한다.
- FallenEra.Combat.Damage.HitRegions: 기존 부위 테스트를 변동 피해 범위 검사로 전환하고 실제 GE 반복 타격을 검사한다.
- FallenEra.Combat.Armor.EquipmentAndDamage: 장비 방어력과 변동 피해, 완전 방어를 검사한다.
- 서버/클라이언트 및 실제 에셋 설정은 PIE에서 별도로 검증해야 한다.

2026-10-06 검증 상태: FallenEraEditor Win64 Development -NoLink 컴파일 및 UHT 생성 코드 확인 완료.
변경 소스의 git diff --check 통과. 실행 중인 에디터를 유지했으므로 새 DLL 링크 및 위 자동화 테스트 실행은 미검증이다.
에디터 저장/종료 후 전체 DLL 빌드를 완료한 다음 FallenEra.Combat 자동화 테스트를 실행한다.
