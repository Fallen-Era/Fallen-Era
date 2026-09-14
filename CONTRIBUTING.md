# Contributing Guide

> Fallen-Era 프로젝트 협업 규칙 및 코딩 컨벤션

---

## 목차

1. [Git 브랜치 전략](#1-git-브랜치-전략)
2. [Issue 규칙](#2-issue-규칙)
3. [커밋 규칙](#3-커밋-규칙)
4. [PR 규칙](#4-pr-규칙)
5. [Label 규칙](#5-label-규칙)
6. [UE5 C++ 코딩 컨벤션](#6-ue5-c-코딩-컨벤션)
7. [Rider 세팅 방법](#7-rider-세팅-방법)

---

## 1. Git 브랜치 전략

```
main
 └── dev
      ├── feature/
      ├── feature/
      ├── bugfix/
      └── refactor/
```

### 브랜치 종류

| 브랜치 | 용도 | 베이스 | 머지 대상 |
|--------|------|--------|-----------|
| `main` | 최종 배포용. 직접 Push 금지 | - | - |
| `develop` | 개발 통합 브랜치 | `main` | `main` |
| `feature/` | 새 기능 개발 | `develop` | `develop` |
| `bugfix/` | 버그 수정 | `develop` | `develop` |
| `refactor/` | 리팩토링 | `develop` | `develop` |
| `hotfix/` | 긴급 버그 수정 | `main` | `main` + `develop` |

### 브랜치 네이밍 규칙

```
feature/[시스템명]-[기능명]
bugfix/[시스템명]-[버그내용]
refactor/[시스템명]-[내용]

예시:
feature/fe-follow-system
feature/fe-capture-rpc
bugfix/fe-health-replication
refactor/fe-character-base
```

---

## 2. Issue 규칙

### Issue 생성 원칙

- **모든 작업은 Issue에서 시작한다**
- 작업을 시작하기 전에 반드시 Issue를 먼저 생성한다
- 하나의 Issue = 하나의 기능 또는 하나의 버그

### Issue 템플릿 종류

| 템플릿 | 용도 | 제목 prefix |
|--------|------|-------------|
| ✨ Feature Request | 새 기능 구현 | `[FEAT]` |
| 🐛 Bug Report | 버그 보고 | `[BUG]` |

### Issue 제목 예시

```
[FEAT] 서버 이동 로직 구현
[FEAT] Replication 설정
[BUG] 클라이언트에서 HP가 동기화되지 않는 문제
[BUG] PIE 2인 환경에서 AI가 서버에서만 동작하는 문제
```

---

## 3. 커밋 규칙

### 커밋 메시지 형식

```
<type>(<scope>): <subject>

<body> (선택)

<footer> (선택)
```

### Type 종류

| Type | 설명 | 예시 |
|------|------|------|
| `feat` | 새로운 기능 추가 | `feat(fe): Add Health Replication` |
| `fix` | 버그 수정 | `fix(fe): Fix Health not syncing on client` |
| `refactor` | 리팩토링 | `refactor(fe): Reorganize FECharacter header` |
| `docs` | 문서 수정 | `docs: Update CONTRIBUTING.md` |
| `style` | 코드 포맷 수정 | `style(fe): Apply coding convention` |
| `perf` | 성능 개선 | `perf(fe): Reduce replication frequency` |
| `test` | 테스트 추가 | `test(fe): Add PIE replication test` |
| `chore` | 빌드, 설정 변경 | `chore: Add .gitignore for UE5` |


### 커밋 메시지 예시

```
feat(character): Add Health variable with Replication

- UPROPERTY(Replicated) 설정
- GetLifetimeReplicatedProps 구현
- OnRep_Health 콜백 추가

Closes #12
```

```
fix(replication): Fix Health not updating on client

클라이언트에서 OnRep_Health가 호출되지 않던 문제 수정
GetLifetimeReplicatedProps에 DOREPLIFETIME 등록 누락이 원인

Closes #15
```

### 커밋 규칙 요약

```
✅ 해야 할 것
- 하나의 커밋 = 하나의 논리적 변경
- 영어로 작성 (type, scope, subject)
- subject는 동사 원형으로 시작 (Add, Fix, Remove, Update)
- 관련 Issue 번호를 footer에 명시

❌ 하지 말 것
- 여러 기능을 하나의 커밋에 묶지 않기
- "수정", "작업중", "aaaa" 같은 의미없는 커밋 메시지
- main, develop에 직접 Push
```

---

## 4. PR 규칙

### PR 생성 조건

- 기능 구현 또는 버그 수정이 완료된 경우
- PIE 환경에서 직접 테스트를 완료한 경우
- PR 체크리스트를 모두 확인한 경우

### PR 규칙

```
1. PR 제목은 관련 Issue 제목과 동일하게 작성
2. PR 본문은 템플릿을 반드시 사용
3. 최소 1명 이상의 팀원 리뷰 후 머지
4. Approve 없이 본인이 직접 머지 금지
5. 머지 후 feature 브랜치는 즉시 삭제
```

### 머지 전략

```
develop ← feature: Squash and Merge
  → 커밋 히스토리를 깔끔하게 유지

main ← develop: Merge Commit
  → 배포 시점을 명확하게 기록
```

---

## 5. Label 규칙

GitHub Labels 설정 목록:

| Label | 색상 | 설명 |
|-------|------|------|
| `feature` | `#0075ca` | 새 기능 |
| `bug` | `#d73a4a` | 버그 |
| `refactor` | `#e4e669` | 리팩토링 |
| `docs` | `#cfd3d7` | 문서 |
| `in progress` | `#f9a825` | 작업 중 |
| `review needed` | `#9c27b0` | 리뷰 요청 |
| `blocked` | `#b60205` | 블로킹 이슈 존재 |
| `fe-ai` | `#1d76db` | AI 시스템 관련 |
| `fe-replication` | `#0e8a16` | Replication/RPC 관련 |
| `fe-combat` | `#e11d48` | Combat 시스템 관련 |

---

## 6. UE5 C++ 코딩 컨벤션

> Unreal Engine 공식 컨벤션을 기반으로 팀 규칙을 추가한 버전

### 네이밍 규칙

```cpp
// 클래스
// U prefix: UObject 파생
// A prefix: AActor 파생
// F prefix: 일반 구조체
// E prefix: Enum
// I prefix: Interface

class AFECharacter : public ACharacter { };
class UFECombatComponent : public UActorComponent { };
struct FFEStatusData { };
enum class EFEState : uint8 { };
class IFEInteractable { };
```

```cpp
// 변수
// 멤버 변수: PascalCase, prefix 없음
// bool 변수: b prefix

float Health;
float MaxHealth;
bool bIsDead;
bool bIsFollowing;

// 포인터 변수
AFECharacter* OwnerCharacter;
UBehaviorTree* BehaviorTree;
```

```cpp
// 함수
// PascalCase, 동사로 시작

void TakeDamage(float DamageAmount);
float GetHealthPercent() const;
bool IsDead() const;

// RPC 함수 네이밍 규칙 (필수)
// Server RPC: Server prefix
// Client RPC: Client prefix  
// Multicast RPC: Multicast prefix

UFUNCTION(Server, Reliable)
void ServerRequestAttack(AActor* TargetActor);

UFUNCTION(NetMulticast, Reliable)
void MulticastPlayHitReaction();

UFUNCTION(Client, Reliable)
void ClientShowCaptureResult(bool bSuccess);
```

### UPROPERTY 순서 규칙

```cpp
// 헤더 파일에서 UPROPERTY 선언 순서
// 1. Replicated 변수
// 2. EditAnywhere / EditDefaultsOnly 변수
// 3. VisibleAnywhere 변수
// 4. BlueprintAssignable (Delegate)
// 5. private 변수

UPROPERTY(Replicated)
float Health;

UPROPERTY(ReplicatedUsing = OnRep_State)
EFEState CurrentState;

UPROPERTY(EditDefaultsOnly, Category = "FallenEra|Stats")
float MaxHealth = 100.f;

UPROPERTY(EditDefaultsOnly, Category = "FallenEra|AI")
UBehaviorTree* BehaviorTree;
```

### Category 네이밍 규칙

```cpp
// Category는 "시스템|세부항목" 형식 사용
UPROPERTY(EditDefaultsOnly, Category = "FallenEra|Stats")
UPROPERTY(EditDefaultsOnly, Category = "FallenEra|AI")
UPROPERTY(EditDefaultsOnly, Category = "FallenEra|Combat")
UPROPERTY(EditDefaultsOnly, Category = "FallenEra|Capture")
```

### 멀티플레이어 코딩 규칙

```cpp
// 규칙 1: 게임 로직은 반드시 Authority 체크 후 실행
void ACharacter::TakeDamage(float DamageAmount)
{
    if (!HasAuthority()) return; // 서버에서만 실행

    Health -= DamageAmount;

    if (Health <= 0.f)
    {
        Die();
    }
}

// 규칙 2: SpawnActor는 반드시 서버에서만
void AFESpawnSystem::SpawnCharacter(TSubclassOf<AFECharacter> CharacterClass)
{
    if (!HasAuthority()) return;

    GetWorld()->SpawnActor<AFECharacter>(CharacterClass, ...);
}

// 규칙 3: RPC Validate 함수는 반드시 구현
bool AFECharacter::ServerRequestAttack_Validate(AActor* TargetActor)
{
    return TargetActor != nullptr; // 기본 유효성 검사
}
```

### 파일 구조 규칙

```
Source/Fallen-Era/
├── Character/
│   ├── FECharacter.h
│   └── FECharacter.cpp
├── AI/
│   ├── FEAIController.h
│   ├── FEAIController.cpp
│   └── BehaviorTree/
├── Combat/
│   ├── FECombatComponent.h
│   └── FECombatComponent.cpp
├── Capture/
├── Spawn/
├── Skill/
└── Data/
    └── FEDataAsset.h
```

### 주석 규칙

```cpp
// 한 줄 주석: 코드 의도를 설명 (무엇을 하는지가 아니라 왜 하는지)

// ❌ 나쁜 주석
Health -= DamageAmount; // Health에서 DamageAmount를 뺀다

// ✅ 좋은 주석
Health -= DamageAmount; // 방어력 계산은 DamageComponent에서 이미 처리됨

// 멀티플레이어 관련 주석은 반드시 명시
// [Server Only] 서버에서만 호출되어야 하는 함수
// [Client Only] 클라이언트에서만 호출되어야 하는 함수
// [Server RPC] 클라이언트가 호출 → 서버에서 실행
// [Multicast RPC] 서버가 호출 → 모든 클라이언트에서 실행
```

---

## 7. Rider 세팅 방법

### 7-1. 필수 플러그인 설치

Rider 실행 후 `Plugins` 메뉴에서 아래 항목 설치:

```
- Unreal Engine Support (기본 내장)
- .editorconfig 지원 확인
```

### 7-2. 코드 포맷 설정

`File > Settings > Editor > Code Style > C++` 에서:

```
Indent: Spaces, Size = 4
Brace Style: Allman (여는 중괄호를 새 줄에)
```

### 7-3. 프로젝트 루트에 .editorconfig 파일 추가

아래 내용으로 `.editorconfig` 파일을 프로젝트 루트에 생성:

```ini
root = true

[*.{h,cpp}]
indent_style = space
indent_size = 4
end_of_line = crlf
charset = utf-8
trim_trailing_whitespace = true
insert_final_newline = true
```

### 7-4. Live Template 설정 (자주 쓰는 코드 스니펫)

`File > Settings > Editor > Live Templates` 에서 C++ 그룹에 추가:

**REPVAR — Replicated 변수 선언**
```
Abbreviation: repvar
Template:
UPROPERTY(Replicated)
$TYPE$ $NAME$;
```

**REPNOTIFY — ReplicatedUsing 변수 선언**
```
Abbreviation: repnotify
Template:
UPROPERTY(ReplicatedUsing = OnRep_$NAME$)
$TYPE$ $NAME$;

UFUNCTION()
void OnRep_$NAME$();
```

**SRPC — Server RPC 선언**
```
Abbreviation: srpc
Template:
UFUNCTION(Server, Reliable)
void Server$NAME$($PARAMS$);
bool Server$NAME$_Validate($PARAMS$);
```

**MRPC — Multicast RPC 선언**
```
Abbreviation: mrpc
Template:
UFUNCTION(NetMulticast, Reliable)
void Multicast$NAME$($PARAMS$);
```

### 7-5. 유용한 Rider 단축키 (UE5 개발)

| 단축키 | 기능 |
|--------|------|
| `Ctrl + Shift + B` | 빌드 |
| `Ctrl + Alt + F` | 전체 파일 포맷 |
| `Ctrl + B` | 선언으로 이동 |
| `Alt + F7` | 사용처 찾기 |
| `Shift + Shift` | 전체 검색 |
| `Ctrl + /` | 라인 주석 토글 |
