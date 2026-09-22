# Runtime Message System 사용 가이드 (초안)

`GameplayMessageRouter` · `UGameplayMessageSubsystem` · **채널당 하나의 Payload 타입**

## 1. 어디에 사용하는가

```mermaid
flowchart LR
    UI["UI 작업 요청"] --> REQ["요청 채널"]
    REQ --> OWNER["담당 시스템<br/>실행 담당은 하나로 지정"]

    EVENT["처치 · 획득<br/>작업 완료 · 실패"] --> NOTICE["알림 채널"]
    NOTICE --> QUEST["퀘스트"]
    NOTICE --> VIEW["UI / ViewModel"]
    NOTICE --> LOG["로그"]

    STATE["OnRep / GAS 변경 콜백"] --> LOCAL["로컬 변경 알림"]
    LOCAL --> VIEW
```

현재 값 조회·반환값이 필요한 호출은 **함수 / 인터페이스**, 이미 참조 중인 특정 객체의 알림은 **Delegate**를 사용한다.

## 2. 현재 프로젝트 연결

**채널:** `Event.World.CreateRequested` · **Payload:** `FWorldCreateRequest { DisplayName, WorldSize }`

```mermaid
sequenceDiagram
    participant UI as NewGameWidget
    participant Router as GameplayMessageSubsystem
    participant World as WorldRegistrySubsystem

    World->>Router: Initialize → RegisterListener
    Note over Router,World: 발신 전에 구독
    UI->>Router: 버튼 클릭 → BroadcastMessage(Channel, Request)
    Router->>World: CreateWorld(Channel, Request)
    World-->>Router: 콜백 반환
    Router-->>UI: BroadcastMessage 반환
    Note over UI,World: 같은 GameInstance 안에서 즉시 호출 / 작업 성공 반환값 없음
    World->>Router: Deinitialize → Handle.Unregister()
```

`CreateWorld()`의 생성 처리는 구현 중이다. **요청 수신 ≠ 생성 완료.**

<details>
<summary>C++ 최소 예제 펼치기</summary>

공통 include: `GameFramework/GameplayMessageSubsystem.h`, `AbilitySystem/FallenEraGameplayTags.h`, `WorldGenerator/Data/Payload.h`. 현재 모듈에는 `GameplayMessageRuntime` 의존성이 등록되어 있다.

```cpp
// 수신자 멤버
FGameplayMessageListenerHandle WorldCreateRequestHandle;
void CreateWorld(FGameplayTag Channel, const FWorldCreateRequest& Request);

// UWorldRegistrySubsystem::Initialize 내부, Super 호출 후
auto* Router = Collection.InitializeDependency<UGameplayMessageSubsystem>();
WorldCreateRequestHandle = Router->RegisterListener<FWorldCreateRequest>(
    FallenEraGameplayTags::TAG_Event_World_CreateRequested,
    this, &ThisClass::CreateWorld);

// 발신 객체의 멤버 함수 내부
FWorldCreateRequest Request;
Request.DisplayName = TEXT("New World");
Request.WorldSize = FIntPoint(1024, 1024);
UGameplayMessageSubsystem::Get(this).BroadcastMessage(
    FallenEraGameplayTags::TAG_Event_World_CreateRequested, Request);

// UWorldRegistrySubsystem::Deinitialize 내부, Super 호출 전
WorldCreateRequestHandle.Unregister();
```

원본: [발신](../Source/FallenEra/WorldGenerator/WorldSystemWidget/NewGameWidget.cpp) · [수신](../Source/FallenEra/WorldGenerator/WorldRegistrySubsystem.cpp) · [Payload](../Source/FallenEra/WorldGenerator/Data/Payload.h) · [채널](../Source/FallenEra/AbilitySystem/FallenEraGameplayTags.cpp)

</details>

## 3. Blueprint 연결

같은 **Channel + Payload 타입**이면 C++ ↔ BP도 연결된다. 아래는 위 월드 생성 요청의 BP 표현이다.

```mermaid
flowchart LR
    CLICK["버튼 클릭"] --> SEND["Broadcast Message<br/>Channel: Event.World.CreateRequested"]
    SUB["Get Game Instance Subsystem<br/>GameplayMessageSubsystem"] -->|"Target"| SEND
    MAKE["Make WorldCreateRequest<br/>DisplayName / WorldSize"] -->|"Message"| SEND
```

```mermaid
flowchart TD
    START["구독 시작"] --> LISTEN["Listen for<br/>Gameplay Messages<br/>위 Channel / Payload Type<br/>Exact Match"]
    LISTEN -->|"일반 실행 출력"| SAVE["Async Action<br/>변수로 보관"]
    LISTEN -->|"On Message Received"| PAYLOAD["Payload 분해<br/>Actual Channel 확인"]
    PAYLOAD --> FILTER{"내가 처리할 대상?"}
    FILTER -->|"예"| HANDLE["처리"]
    FILTER -->|"아니오"| SKIP["무시"]
    HANDLE -. "한 번만 받을 경우" .-> CANCEL["Async Action → Cancel"]
    STOP["구독 종료"] --> CANCEL
    SAVE -. "취소할 객체" .-> CANCEL
```

**기존 C++ 수신자와 BP 수신자는 둘 다 호출된다.** 월드 생성 실행을 중복 연결하지 않는다.

## 4. 구독 수명

```mermaid
flowchart LR
    subgraph BEGIN["등록 시점"]
        GI["Subsystem<br/>Initialize"]
        ACTOR["Actor / Component<br/>BeginPlay"]
        WIDGET["Widget<br/>Construct / 활성화"]
    end
    GI --> REGISTER["RegisterListener / Listen<br/>Handle / Async Action 보관"]
    ACTOR --> REGISTER
    WIDGET --> REGISTER
    REGISTER --> ACTIVE["구독 유지<br/>메시지마다 콜백 실행"]
    ACTIVE --> RELEASE["Unregister / Cancel"]
    subgraph FINISH["해제 시점"]
        GEND["Subsystem<br/>Deinitialize"]
        AEND["Actor / Component<br/>EndPlay"]
        WEND["Widget<br/>Destruct / 비활성화"]
    end
    GEND --> RELEASE
    AEND --> RELEASE
    WEND --> RELEASE
```

**재등록 전 기존 구독 해제.** Handle 변수 소멸만으로는 해제되지 않는다. 늦게 구독한 화면의 초기값은 원본 시스템에서 조회한다.

## 5. 채널 선택과 수신 조건

```mermaid
flowchart TD
    SEND["Broadcast: Event.World.CreateRequested"] --> MATCH{"구독 채널 / Match Type"}
    MATCH -->|"Event.World.CreateRequested + Exact"| TYPE{"동일한 Payload 타입?"}
    MATCH -->|"Event.World + Partial"| TYPE
    MATCH -->|"Event.World + Exact"| IGNORE["수신 안 함"]
    TYPE -->|"예"| TARGET["Payload의 Actor / ID 확인<br/>내 대상이면 처리"]
    TYPE -->|"아니오"| ERROR["타입 불일치 / 콜백 미실행"]
```

**Exact가 기본. Partial은 자신 + 하위 채널.** 필드가 같아도 다른 구조체는 다른 타입이다. 여러 요청의 결과를 구분해야 하면 요청·결과에 같은 Request ID를 넣는다.

<details>
<summary>C++에서 Partial Match 지정</summary>

기존 구독을 해제한 뒤 아래 방식으로 등록한다. 해당 하위 채널은 모두 같은 Payload 타입을 사용해야 한다.

```cpp
FGameplayMessageListenerParams<FWorldCreateRequest> Params;
Params.MatchType = EGameplayMessageMatch::PartialMatch;
Params.SetMessageReceivedCallback(this, &ThisClass::CreateWorld);
WorldCreateRequestHandle = UGameplayMessageSubsystem::Get(this)
    .RegisterListener<FWorldCreateRequest>(
        FallenEraGameplayTags::TAG_Event_World_CreateRequested, Params);
```

</details>

## 6. 멀티플레이 전달 범위

```mermaid
flowchart TD
    subgraph SERVER["서버 GameInstance"]
        CHANGE["서버 상태 변경"] --> SB["서버 Broadcast"]
        SB --> SL["서버 구독자"]
    end
    subgraph CLIENT["클라이언트 GameInstance"]
        REP["OnRep / GAS 변경 콜백"] --> CB["클라이언트 Broadcast"]
        RPC["RPC 수신<br/>개별 사건 전달 시"] --> CB
        CB --> UI["로컬 UI / ViewModel"]
    end
    CHANGE -->|"Replication"| REP
    CHANGE -. "필요한 사건은 별도 RPC" .-> RPC
```

**메시지 자체는 네트워크로 전달되지 않는다.** Listen Server 호스트 UI도 로컬 발신 경로가 필요하며, 같은 변경은 한 번만 알린다.

## 7. 동작과 문제 확인

| 동작 | 적용 |
| --- | --- |
| 즉시 콜백 · 호출 순서 보장 없음 | 무거운 작업·수신자 간 실행 순서 의존 분리 |
| 과거 메시지 저장 없음 | 구독 전 사건은 재수신 불가 |
| Payload 참조는 콜백 동안 사용 | 지연 처리할 값은 수신 시 복사 |

```mermaid
flowchart LR
    ISSUE{"증상"}
    ISSUE -->|"수신 없음"| CHECK["발신 전 등록?<br/>같은 GameInstance?<br/>채널 · Match · 타입 일치?"]
    ISSUE -->|"여러 번 실행"| DUP["재등록 시 기존 구독 해제?<br/>C++ / BP 실행 담당 중복?"]
    ISSUE -->|"클라이언트만 미수신"| NET["Replication / RPC 수신 후<br/>클라이언트에서 Broadcast?"]
```

발신 로그: `GameplayMessageSubsystem.LogMessages 1` · 끄기: `0` · 로그가 있어도 수신 성공을 뜻하지는 않는다.

구현: [라우터](../Plugins/GameplayMessageRouter/Source/GameplayMessageRuntime/Public/GameFramework/GameplayMessageSubsystem.h) · [BP 수신 Action](../Plugins/GameplayMessageRouter/Source/GameplayMessageRuntime/Private/GameFramework/AsyncAction_ListenForGameplayMessage.cpp) · 포맷: [GitHub Mermaid](https://docs.github.com/en/get-started/writing-on-github/working-with-advanced-formatting/creating-diagrams)
