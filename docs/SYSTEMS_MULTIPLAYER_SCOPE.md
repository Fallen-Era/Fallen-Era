# Systems & Multiplayer Scope

This document defines the LMK-owned systems scope for the Fallen-Era project.
The first implementation pass focuses on field systems, monster spawning, and multiplayer sessions.

## 1. Ownership

| Area | Responsibility |
|---|---|
| World | Field state, time, field events |
| Monster Spawner | Spawn points, spawn zones, server-authoritative spawning, respawn rules |
| Session | Create, find, join, and destroy multiplayer sessions |
| Replication (Iris) | Synchronize world state and spawned field actors utilizing Iris Replication System |
| Combat & Abilities (GAS) | Gameplay Ability System (GAS) handles all attributes, effects, and ability logic |
| Save | Persist world state that must survive level reloads or game restarts |

## 2. Out of Scope

The following systems are excluded from the first LMK implementation pass:

- Inventory
- Item
- Crafting
- Building
- Base

Do not add placeholder implementations for these systems unless another task explicitly requires them.

## 3. Source Layout

All LMK C++ work must stay under the LMK folder:

```text
Fallen-Era/Source/Fallen-Era/LMK/
├── Public/
└── Private/
```

Use the existing project module, `Fallen-Era`. Do not create a new module unless the team agrees to split modules.

## 4. Naming Rules

- Prefix newly created project-owned code identifiers with `FE_`.
- Continue to use Unreal Engine type prefixes where required, such as `U`, `A`, `F`, and `E`.
- Prefer names that describe gameplay responsibility instead of generic names like `Manager` unless the class truly coordinates multiple objects.

Examples:

```cpp
UCLASS()
class FALLENERA_API UFE_SessionSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
};

UCLASS()
class FALLENERA_API AFE_MonsterSpawner : public AActor
{
    GENERATED_BODY()
};

```

## 5. Multiplayer Authority Rules

- Listen Server is the primary development target.
- World state changes must be made by the server.
- Monster spawning and despawning must happen on the server.
- Clients may request actions through Server RPCs, but the server must validate and apply the result.
- Clients should observe replicated state instead of calculating authoritative field state locally.

## 6. SOLID Design Rules

- Keep each class focused on one reason to change.
- Use interfaces for communication between systems that may be replaced or owned by another teammate.
- Use delegates for event notification instead of direct cross-system calls when the receiver is optional.
- Keep replicated state separate from local presentation logic.
- Keep session, world state, spawning, and save responsibilities in separate classes.
- Do not introduce abstractions without a concrete caller or near-term extension point.

Recommended interface boundaries:

| Interface | Purpose |
|---|---|
| `IFE_WorldStateProvider` | Read current day, hour, and future world state |
| `IFE_SpawnableActor` | Allow spawned actors to expose spawn lifecycle callbacks without depending on a monster class |
| `IFE_SpawnRuleProvider` | Provide spawn permission checks such as time or area state |
| `IFE_SaveSerializable` | Let world-owned objects provide save/load data without hard references |

Recommended delegates:

| Delegate | Purpose |
|---|---|
| `FFE_OnSessionOperationCompleted` | Notify create, find, join, and destroy session result |
| `FFE_OnWorldTimeChanged` | Notify local systems when replicated time changes |
| `FFE_OnMonsterSpawned` | Notify optional listeners when a spawner creates an actor |
| `FFE_OnMonsterDespawned` | Notify optional listeners when a spawned actor is removed |

## 7. Planned Systems

### Session System

Responsible for basic multiplayer flow:

- Create session
- Find sessions
- Join session
- Destroy session

Recommended owner:

- `UFE_SessionSubsystem`

Implementation notes:

- Prefer `UGameInstanceSubsystem` for lifetime.
- Keep the first pass minimal and focused on Listen Server sessions.
- Expose Blueprint-callable functions only when UI or menu work needs them.
- Report async operation results through delegates.
- Do not make UI classes directly depend on OnlineSubsystem implementation details.

### World State

Responsible for replicated field state:

- Current day
- Current hour
- Optional world difficulty if needed by gameplay

Recommended owner:

- `AFE_WorldGameState`

Implementation notes:

- Use `ReplicatedUsing` for client-facing state changes.
- Register replicated properties in `GetLifetimeReplicatedProps`.
- Only the server should advance time.
- Broadcast time changes through delegates from `OnRep` handlers.
- Expose read-only access through a world state provider interface when another system needs world state.

### Monster Spawner

Responsible for field monster lifecycle:

- Spawn monster classes at configured points or zones
- Limit active monster count
- Respawn after a delay
- Avoid spawning when the server-side rule says the area is inactive

Recommended owner:

- `AFE_MonsterSpawner`

Implementation notes:

- Spawn actors only on the server.
- Keep data simple in the first pass with editable properties on the spawner actor.
- Move to DataAssets only when multiple spawner types need shared data.
- Do not save every spawned monster by default. Add persistent monster state only when gameplay requires it.
- Use spawn rule interfaces when spawn conditions depend on world state.
- Use delegates to notify listeners about spawn and despawn events.
- Depend on a spawnable interface for lifecycle callbacks instead of depending on a concrete monster class.

### Save

Responsible for persistent world data:

- Time
- Any future field state that must persist

Implementation notes:

- Do not persist transient spawn timers in the first pass.
- Keep save data independent from A/B team implementation details.
- Collect save data through interfaces where objects own their own serialization details.

## 8. Implementation Order

1. Implement `UFE_SessionSubsystem`.
2. Define minimal interfaces and delegates needed by the first caller.
3. Add replicated world state through `AFE_WorldGameState`.
4. Implement server-authoritative `AFE_MonsterSpawner`.
5. Add simple editable spawn settings.
6. Add minimal world save data only after world state exists.

## 9. Acceptance Criteria

- Sessions can be created, searched, joined, and destroyed in Listen Server flow.
- World day and hour are server-owned and replicated to clients.
- Monster spawners create monsters only on the server.
- Spawned monsters appear on connected clients through replication.
- Session, world state, spawning, and save code are separated by responsibility.
- Optional cross-system notifications use delegates instead of hard dependencies.
- Cross-system reads use interfaces when the caller does not need a concrete class.
- LMK implementation files remain under `Fallen-Era/Source/Fallen-Era/LMK/Public` and `Fallen-Era/Source/Fallen-Era/LMK/Private`.
- Crafting, building, inventory, item, and base systems are not implemented as part of this scope.
