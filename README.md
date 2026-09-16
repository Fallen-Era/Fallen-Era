# 🐾 Fallen-Era — Unreal Engine 5 Multiplayer Project

> Fallen-Era 멀티플레이어 게임 구현 프로젝트

---

## 📁 레포지토리 구조

```
Fallen-Era/                        ← 레포 루트
├── .github/
│   ├── ISSUE_TEMPLATE/
│   │   ├── feature.md           ← Feature 이슈 템플릿
│   │   └── bug.md               ← Bug 이슈 템플릿
│   └── pull_request_template.md ← PR 템플릿
├── CONTRIBUTING.md              ← Git 규칙 및 코딩 컨벤션
├── LICENSE
└── Fallen-Era/                    ← UE5 프로젝트 폴더
    ├── Config/
    ├── Content/
    │   ├── _Common/             ← 팀 공통 에셋 (완료된 작업만)
    │   └── _Private/
    │       ├── OBI/             ← 팀원 개인 작업 폴더 (이니셜)
    │       └── .../
    └── Source/
        ├── Fallen-Era/           ← 공통 소스 (완료된 작업만)
        └── OBI/                 ← 팀원 개인 소스 폴더 (이니셜)
            ├── public
            └── private 
.../
```

---

## 🚀 시작하기 전에 반드시 읽을 것

> **이 README를 끝까지 읽고 세팅을 완료한 뒤 작업을 시작해주세요.**
> 세팅 없이 바로 작업하면 팀 전체에 충돌이 발생할 수 있습니다.

---

## 1. 브랜치 전략

### 브랜치 구조

```
main
 └── develop
      ├── feature/combat        ← 특정 모듈 작업 브랜치
      ├── bugfix/fe-health-sync
      └── refactor/fe-character
```

### 브랜치 네이밍 규칙

브랜치는 **두 가지 방식** 중 작업 성격에 맞게 선택합니다.

| 방식 | 형식 | 언제 사용 |
|------|------|-----------|
| 이니셜 브랜치 | `feature/OBI` | 개인 작업 공간이 필요할 때 |
| 모듈 브랜치 | `feature/combat` | 특정 시스템을 집중 구현할 때 |

```bash
# 이니셜 브랜치 예시 — 개인 작업
git checkout -b feature/OBI

# 모듈 브랜치 예시 — 시스템 단위 작업
git checkout -b feature/fe-follow
git checkout -b feature/fe-combat
git checkout -b feature/fe-capture
git checkout -b bugfix/fe-health-replication
```

### 브랜치 규칙 요약

```
✅ 해야 할 것
- 모든 작업은 develop에서 브랜치를 따서 시작
- 작업 완료 후 PR을 통해 develop에 머지
- 머지 완료된 브랜치는 즉시 삭제

❌ 하지 말 것
- main, develop에 직접 Push 금지
- 다른 팀원의 이니셜 브랜치에 직접 Push 금지
```

---

## 2. Source / Content 폴더 작업 규칙

### 핵심 원칙

```
개인 폴더에서 작업 → 완료 후 공통 폴더에 복사 → PR 올리기
```

### Content 폴더 구조

```
Content/
├── _Common/                 ← Unreal 자산은 에셋 내부 레퍼런스 문제로 바로 분류 후 작업
│   ├── Characters/
│   ├── AI/
│   ├── UI/
│   └── VFX/

```

### Source 폴더 구조

```
Source/
├── Fallen-Era/               ← 팀 공통 소스 (완료된 코드만)
│   ├── Character/
│   ├── AI/
│   ├── Combat/
│   └── ...
└── OBI/                 ← 본인 이니셜 폴더에서만 작업
│   ├── public
│   └── private 
└── KSH/
│   ├── public
│   └── private 
└──.../
```

### 작업 흐름

```
1. 작업 시작
   Work/본인이니셜/ 폴더에서 작업

       ↓

2. 작업 완료 및 테스트
   PIE 2인 환경에서 직접 테스트
   멀티플레이어 동작 확인

       ↓

3. 공통 폴더로 복사
   Work/OBI/ → _Common/ (에셋)

       ↓

4. PR 제출
   PR 템플릿 작성 후 develop에 머지 요청
```

> ⚠️ **공통 폴더에는 완료되고 테스트까지 끝난 작업만 올립니다.**
> 작업 중인 파일을 공통 폴더에 올리면 다른 팀원의 작업에 영향을 줍니다.
> Source 파일명의 **prefix**는 PW로 "PW..."와 같이 저장합니다 

---

## 3. 프로젝트 세팅 및 로그 설정

> Issue에 등록된 세팅 기준 참고해주세요
> 세팅이 다르면 멀티플레이어 테스트 환경이 달라져 재현이 어렵습니다.

### 필수 프로젝트 세팅

에디터에서 `Edit > Project Settings` 진입 후 아래 항목 확인:

```
[Maps & Modes]
Default GameMode: 팀 합의된 GameMode 클래스로 설정

[Network]
Net Mode: Listen Server (개발 테스트 기준)
```

### PIE 멀티플레이어 테스트 세팅

에디터 상단 플레이 버튼 옆 `▼` 클릭:

```
Number of Players: 2 이상
Net Mode: Play As Listen Server
Run Under One Process: ✅ 체크
```

---

## 4. 템플릿 및 협업 문서 활용

### Issue 템플릿 사용

모든 작업은 **Issue 생성에서 시작**합니다.

```
GitHub → Issues → New Issue

✨ Feature Request   → 새 기능 구현 시 사용
🐛 Bug Report        → 버그 발견 시 사용
```

Issue 없이 바로 PR을 올리는 것은 금지합니다.

### PR 템플릿 사용

PR 본문의 **멀티플레이어 체크리스트**는 반드시 직접 확인 후 체크합니다.

```
- [ ] 서버/클라이언트 Authority 구분이 올바른가?
- [ ] PIE 2인 이상 환경에서 테스트했는가?
- [ ] Replicated 변수가 GetLifetimeReplicatedProps에 등록되었는가?
```

체크하지 않은 항목이 있으면 리뷰어가 머지를 보류할 수 있습니다.

### 협업 문서 협의 요청

아래 두 문서는 현재 기본 세팅 상태입니다.
팀 상황에 맞게 함께 내용을 수정하고 합의해주세요. (같이해요!)

| 문서 | 내용 | 협의 필요 항목 |
|------|------|----------------|
| `CONTRIBUTING.md` | Git 규칙, 커밋 컨벤션, 코딩 스타일 | 커밋 타입 추가/제거, 코딩 규칙 조정 |
| `PROJECT_TRACKING.md` | 스프린트 계획, 태스크 목록 | 담당자 배정, 우선순위 조정, 일정 확정 |


---

## 5. 커밋 메시지 규칙 (요약)

자세한 내용은 `CONTRIBUTING.md` 참고.

```
feat(fe-character): Add Health Replication
fix(fe-combat): Fix damage not applying on client
docs: Update README
```

---

## 6. 참고 문서

| 문서 | 설명 |
|------|------|
| [CONTRIBUTING.md](./CONTRIBUTING.md) | Git 규칙, 커밋 컨벤션, 코딩 컨벤션, Rider 세팅 |

---

## 7. 문의

작업 중 막히는 부분이나 공통 폴더 반영 전 리뷰가 필요한 경우 Issue를 먼저 생성하거나 팀 채널에 공유해주세요.
