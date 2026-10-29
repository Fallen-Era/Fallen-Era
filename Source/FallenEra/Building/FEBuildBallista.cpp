// Fill out your copyright notice in the Description page of Project Settings.

#include "FEBuildBallista.h"
#include "FEBuildingSubsystem.h"
#include "AbilitySystemGlobals.h"
#include "EngineUtils.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "FEBuildingGameplayTags.h"
#include "FEBuildingSettings.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/FallenEraGameplayTags.h"
#include "Combat/Component/FECombatComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "InputMappingContext.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Combat/GameplayEffect/FEDamageGameplayEffect.h"
#include "Combat/ObjectPool/FEProjectilePoolSubsystem.h"
#include "Combat/Projectile/FECombatProjectile.h"

#define LOCTEXT_NAMESPACE "FEBuilding"

namespace
{
    /** [Server] 탑승 중 조작자 상태 확인 간격 */
    constexpr float OperatorCheckInterval = 0.5f;
    
    /** 탑승 이동 잠금 표시 (MOVE_Custom 의 CustomMovementMode). 풀 때 이 번호인지 보고 탑승이 건 잠금만 푼다.
     *  다른 시스템이 MOVE_Custom 을 쓰게 되면 번호가 겹치지 않게 맞춘다 */
    constexpr uint8 OperatorMovementMode = 1;
    
    /** [Client] 조준을 서버로 보내는 최소 간격 (다른 플레이어에게 보이는 용도, 바뀌었을 때만) */
    constexpr double BallistaAimSendInterval = 0.1;

    /** [Client] 조준이 그대로여도 이 간격마다 다시 보낸다 — Unreliable 이라 마지막 패킷을 잃으면 다른 화면의 총신이 이전 각도에 멈춘다 */
    constexpr double BallistaAimRefreshInterval = 0.5;

    /** [Server] 조준 요청을 받는 최소 간격. 보내는 간격의 절반이라 지터로 몰려 온 정상 패킷은 거의 버리지 않는다 */
    constexpr double BallistaAimAcceptInterval = BallistaAimSendInterval * 0.5;

    /** [Server] 발사 요청 최소 간격. 장전 시간(2초)보다 훨씬 짧아 정상 발사는 걸리지 않는다 */
    constexpr double BallistaFireRequestInterval = 0.25;

    /** 발리스타 카메라 ↔ 캐릭터 시야 전환 시간 (초) */
    constexpr float BallistaViewBlendTime = 0.3f;
}

AFEBuildBallista::AFEBuildBallista()
{
    Capacity = 1; // 탄약 칸 1개. 한 칸 최대 개수는 MaxStackSize (BP 기본값)
    AmmoTag = FEBuildingTags::Item_Ammo_Bolt;
    
    // 함정과 달리 ASC 를 복제한다 — 나중에 볼트에 AttackData(명중 효과)를 붙이면 공격자 ASC 로 GameplayCue 가 나간다
    AbilitySystem->SetIsReplicated(true);
    
    // 조준(머리 회전)은 매 프레임 따라가야 해서 틱을 쓴다. 이 기계의 플레이어가 탔을 때만 켠다 (AFEBuildPiece 는 틱 없음)
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;

    Head = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Head"));
    Head->SetupAttachment(GetMesh());
    Head->SetCollisionEnabled(ECollisionEnabled::NoCollision); // 보이기만 — 총신이 벽·난간 자리 위로 나가도 설치를 막지 않게
    Head->SetCanEverAffectNavigation(false);

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(Head);

    Seat = CreateDefaultSubobject<USceneComponent>(TEXT("Seat"));
    Seat->SetupAttachment(GetMesh());
    
    Muzzle = CreateDefaultSubobject<USceneComponent>(TEXT("Muzzle"));
    Muzzle->SetupAttachment(Head); // 총신 끝. 머리와 같이 돈다
}

void AFEBuildBallista::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AFEBuildBallista, Operator);
    DOREPLIFETIME(AFEBuildBallista, AimYaw);
    DOREPLIFETIME(AFEBuildBallista, AimPitch);
    DOREPLIFETIME(AFEBuildBallista, NextFireTime);
}

void AFEBuildBallista::BeginPlay()
{
    Super::BeginPlay();

    // 탑승 입력은 화면이 있는 기계에서만 쓴다. 탑승 순간 멈칫하지 않게 미리 비동기로 받아 둔다.
    // 로드가 끝나기 전에 탔으면 완료 콜백이 그때 입력을 넣는다 — 게임 스레드를 멈추고 기다리지 않는다
    if (GetNetMode() != NM_DedicatedServer && !MappingContext.IsNull())
    {
        MappingLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(MappingContext.ToSoftObjectPath(),
            FStreamableDelegate::CreateUObject(this, &AFEBuildBallista::HandleMappingContextLoaded));
    }
    
    // 볼트는 모든 기계가 쓴다 — 서버는 발사, 클라는 리플리케이트된 볼트를 띄울 때. 첫 발사에 멈칫하지 않게 미리 받는다
    if (!BoltClass.IsNull())
    {
        BoltLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(BoltClass.ToSoftObjectPath());
    }
}

void AFEBuildBallista::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 부서지거나 철거되면 조작자를 내린다: 서버 = 이동 복구·Owner 해제 (리슨 호스트 화면은 SetOperator 의 OnRep 이 되돌린다),
    // 원격 조작자 기계 = 시야·입력·태그 복구. 두 번 불려도 EndLocalOperation 이 한 번만 처리한다
    if (HasAuthority())
    {
        SetOperator(nullptr);
    }
    EndLocalOperation();
    GetWorldTimerManager().ClearTimer(OperatorCheckTimer);
    Super::EndPlay(EndPlayReason);
}

void AFEBuildBallista::SetOperator(APawn* NewOperator)
{
    if (!HasAuthority() || NewOperator == Operator) return;

    // 이전 조작자: 탑승이 건 이동 잠금만 푼다 (이미 사라지는 중이면 건드리지 않는다. 죽어서 내리는 경우 HT 사망 처리의 잠금은 그대로 둔다)
    if (ACharacter* Previous = Cast<ACharacter>(Operator); IsValid(Previous))
    {
        UnlockOperatorMovement(Previous);
    }

    Operator = NewOperator;
    // 조작자를 Owner 로: 그 클라만 이 발리스타에 Server RPC 를 보낼 수 있다 (서버는 소유 연결에서 온 RPC 만 실행한다). nullptr 이면 해제
    SetOwner(Operator);

    if (ACharacter* Character = Cast<ACharacter>(Operator))
    {
        // 좌석으로 옮기고(캡슐 중심 = 좌석 바닥 + 반높이) 이동을 잠근다. 소유 클라도 OnRep 에서 같은 잠금을 한다
        const FVector SeatLocation = Seat->GetComponentLocation() + FVector(0.f, 0.f, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
        Character->SetActorLocationAndRotation(SeatLocation, FRotator(0.f, GetActorRotation().Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
        LockOperatorMovement(Character);
        GetWorldTimerManager().SetTimer(OperatorCheckTimer, this, &AFEBuildBallista::CheckOperator, OperatorCheckInterval, true);
    }
    else
    {
        GetWorldTimerManager().ClearTimer(OperatorCheckTimer);
        AimYaw = 0; // 내리면 정면으로
        AimPitch = 0;
        OnRep_Aim();
    }

    UE_LOG(LogFEBuilding, Log, TEXT("%s operator: %s"), *GetName(), *GetNameSafe(Operator));
    OnRep_Operator(); // 리슨 호스트는 OnRep 이 자동으로 불리지 않는다
}

void AFEBuildBallista::LockOperatorMovement(ACharacter* Character)
{
    UCharacterMovementComponent* Movement = Character != nullptr ? Character->GetCharacterMovement() : nullptr;
    if (Movement == nullptr) return;

    // MOVE_None(DisableMovement) 대신 전용 번호를 단 MOVE_Custom — 움직이지 않는 건 같고(PhysCustom 이 비어 있다),
    // 풀 때 이 잠금이 탑승 것인지 알 수 있다
    Movement->StopMovementImmediately();
    Movement->SetMovementMode(MOVE_Custom, OperatorMovementMode);
}

void AFEBuildBallista::UnlockOperatorMovement(ACharacter* Character)
{
    UCharacterMovementComponent* Movement = Character != nullptr ? Character->GetCharacterMovement() : nullptr;
    if (Movement == nullptr) return;

    // 탑승 잠금일 때만 푼다. 탑승 중 죽으면 HT 사망 처리가 MOVE_None 으로 바꿔 두므로 그대로 둔다.
    // 클라에서는 사망 신호(폰)와 하차 신호(발리스타)가 다른 액터로 와서 순서가 정해져 있지 않다 —
    // 하차가 먼저 오면 여기서 걷기로 돌리고, 뒤에 온 사망 처리가 다시 끈다. 어느 순서든 결과가 같다
    if (Movement->MovementMode == MOVE_Custom && Movement->CustomMovementMode == OperatorMovementMode)
    {
        Movement->SetMovementMode(MOVE_Walking);
    }
    else if (Movement->MovementMode == MOVE_None)
    {
        UE_LOG(LogFEBuilding, Log, TEXT("%s dismount: movement stays disabled (not the ballista lock)"), *Character->GetName());
    }
}

FVector2D AFEBuildBallista::ApplyLocalAim(const FRotator& ControlRotation)
{
    const float Yaw = FMath::Clamp(FRotator::NormalizeAxis(ControlRotation.Yaw - GetActorRotation().Yaw), -MaxYaw, MaxYaw);
    const float Pitch = FMath::Clamp(FRotator::NormalizeAxis(ControlRotation.Pitch), MinPitch, MaxPitch);
    SetHeadAim(Yaw, Pitch);
    return FVector2D(Yaw, Pitch);
}

void AFEBuildBallista::SetReplicatedAim(int8 Yaw, int8 Pitch)
{
    if (!HasAuthority()) return;

    // 클라 값은 믿지 않고 범위로 다시 자른다
    AimYaw = static_cast<int8>(FMath::RoundToInt(FMath::Clamp(static_cast<float>(Yaw), -MaxYaw, MaxYaw)));
    AimPitch = static_cast<int8>(FMath::RoundToInt(FMath::Clamp(static_cast<float>(Pitch), MinPitch, MaxPitch)));
    OnRep_Aim(); // 리슨 호스트 화면
}

bool AFEBuildBallista::TryFire(APawn* InstigatorPawn, float Yaw, float Pitch, FText& OutReason)
{
    if (!HasAuthority() || GetState() != EFEBuildPieceState::Built) return false;

    // 1) 탄약 — 장전보다 먼저 본다 (빈 발리스타 연타가 장전 시간을 잡아먹지 않게)
    if (GetAmmoCount() <= 0)
    {
        OutReason = LOCTEXT("BallistaNoAmmo", "탄약이 없습니다 (Tab: 탄약)");
        return false;
    }

    // 2) 볼트 클래스·풀. BeginPlay 의 비동기 로드가 끝나기 전이면 기다리지 않고 거절한다 (리슨 서버의 동기 로드는 호스트 화면까지 멈춘다).
    //    장전보다 먼저 — 준비 전 요청이 장전 시간을 쓰지 않게
    UFE_ProjectilePoolSubsystem* Pool = GetWorld()->GetSubsystem<UFE_ProjectilePoolSubsystem>();
    if (BoltClass.IsNull() || Pool == nullptr)
    {
        UE_LOG(LogFEBuilding, Warning, TEXT("%s cannot fire: Bolt Class is empty or projectile pool missing"), *GetName());
        return false;
    }
    UClass* LoadedBoltClass = BoltClass.Get();
    if (LoadedBoltClass == nullptr)
    {
        OutReason = LOCTEXT("BallistaBoltNotReady", "발사 준비 중입니다");
        return false;
    }

    // 3) 장전 — 발리스타마다 센다 (NextFireTime 은 리플리케이트되어 HUD 도 같은 값을 본다).
    //    클라가 이미 막으므로 여기 걸리면 지터·조작된 요청 → 안내 없이 로그만
    if (!FEBuilding::ConsumeRequestSlot(NextFireTime, GetWorld()->GetTimeSeconds(), ReloadTime))
    {
        UE_LOG(LogFEBuilding, Verbose, TEXT("%s fire rejected: reloading"), *GetName());
        return false;
    }

    // 4) 조준: 범위로 자르고 다른 화면용 값(int8)을 맞춘 뒤, 머리는 정밀값으로 돌린다 (Muzzle·Camera 가 이 회전을 따른다)
    const float ClampedYaw = FMath::Clamp(Yaw, -MaxYaw, MaxYaw);
    const float ClampedPitch = FMath::Clamp(Pitch, MinPitch, MaxPitch);
    SetReplicatedAim(static_cast<int8>(FMath::RoundToInt(ClampedYaw)), static_cast<int8>(FMath::RoundToInt(ClampedPitch)));
    SetHeadAim(ClampedYaw, ClampedPitch);

    // 5) 방향: 머즐에서 "카메라 정면 AimConvergeDistance 지점"으로 → 화면 중앙 조준점과 그 거리에서 만난다
    const FVector MuzzleLocation = Muzzle->GetComponentLocation();
    const FVector AimPoint = Camera->GetComponentLocation() + Camera->GetForwardVector() * AimConvergeDistance;
    const FVector Direction = (AimPoint - MuzzleLocation).GetSafeNormal(UE_SMALL_NUMBER, Muzzle->GetForwardVector());

    // 6) 탄약 1 → 풀에서 볼트. 못 받으면 탄약을 되돌린다
    TakeFromSlot(0, AmmoTag, 1);
    AFE_CombatProjectile* Bolt = Pool->AcquireProjectile(LoadedBoltClass, FTransform(Direction.Rotation(), MuzzleLocation), this, InstigatorPawn, false);
    if (Bolt == nullptr)
    {
        StoreItems(AmmoTag, 1);
        UE_LOG(LogFEBuilding, Warning, TEXT("%s cannot fire: projectile pool returned nothing"), *GetName());
        return false;
    }

    // DamageSource = 발리스타: 명중하면 발리스타의 CombatComponent 가 발리스타 ASC 의 AttackPower 로 피해를 준다.
    // WeaponData·AttackData 없음 = 피격 효과·넉백 없음 (필요해지면 HT 형식의 AttackData 를 붙인다)
    Bolt->InitializeProjectile(this, nullptr, nullptr, UFE_DamageGameplayEffect::StaticClass(), Direction * BoltSpeed, BoltGravityScale);

    UE_LOG(LogFEBuilding, Log, TEXT("%s fired by %s (ammo %d)"), *GetName(), *GetNameSafe(InstigatorPawn), GetAmmoCount());
    return true;
}

int32 AFEBuildBallista::GetAmmoCount() const
{
    return GetSlotCount(0, AmmoTag);
}

bool AFEBuildBallista::IsReloading() const
{
    // 서버 시각으로 비교한다. 서버는 월드 시각 그대로, 클라는 GameState 가 맞춰 둔 서버 시각 —
    // 받은 값 기준이라 한쪽 지연만큼 늦게 가므로 클라는 서버보다 조금 늦게 "준비됨"을 본다 (일찍 쏴서 거절당하지 않는다)
    const AGameStateBase* GameState = GetWorld()->GetGameState();
    const double ServerNow = GameState != nullptr ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
    return ServerNow < NextFireTime;
}

bool AFEBuildBallista::AcceptsItem(FGameplayTag ItemTag, FText& OutReason) const
{
    if (ItemTag == AmmoTag) return true;

    OutReason = FText::Format(LOCTEXT("BallistaAmmoOnly", "{0}만 넣을 수 있습니다"), UFEBuildingSettings::Get()->GetItemDisplayName(AmmoTag));
    return false;
}

FText AFEBuildBallista::GetSlotLabel(int32 SlotIndex) const
{
    return UFEBuildingSettings::Get()->GetItemDisplayName(AmmoTag);
}

bool AFEBuildBallista::CanInteractBuilt(AActor* InstigatorActor) const
{
    return true; // 사용 중이어도 프롬프트로 "사용 중"을 보여 준다. 실제 탑승은 InteractBuilt 가 다시 판단
}

FText AFEBuildBallista::GetInteractTextBuilt(AActor* InstigatorActor) const
{
    return Operator != nullptr
        ? LOCTEXT("BallistaInUse", "사용 중")
        : LOCTEXT("BallistaMount", "탑승 (Tab: 탄약)");
}

void AFEBuildBallista::InteractBuilt(AActor* InstigatorActor)
{
    // [Server Only] 비어 있을 때만 탄다. 거리는 상호작용 컴포넌트가 이미 확인했다
    if (Operator == nullptr)
    {
        SetOperator(Cast<APawn>(InstigatorActor));
    }
}

void AFEBuildBallista::InteractBuiltLocal(AActor* InstigatorActor)
{
    // 보관함 패널을 열지 않는다 — E 는 탑승. 탄약 UI 는 Tab
}

void AFEBuildBallista::OnRep_Operator()
{
    // 이 기계의 플레이어가 내렸다 — 조작자가 비었거나 다른 사람으로 바뀌었다 (폰이 이미 사라진 경우 포함: 기억해 둔 폰이 null 이어도 Operator 가 비면 내린다)
    if (bLocalOperation && (Operator == nullptr || Operator != LocalOperatorPawn.Get()))
    {
        EndLocalOperation();
    }

    // 이 기계의 플레이어가 탔다
    if (!bLocalOperation && Operator != nullptr && Operator->IsLocallyControlled())
    {
        BeginLocalOperation(Operator);
    }
}

void AFEBuildBallista::OnRep_Aim()
{
    // 조작자 기계는 로컬 조준을 쓴다 — 서버 값은 늦게 도착하므로 덮어쓰면 화면이 뒤로 튄다 (리슨 호스트는 SetReplicatedAim 이 직접 부른다)
    if (bLocalOperation) return;
    SetHeadAim(AimYaw, AimPitch);
}

void AFEBuildBallista::HandleMappingContextLoaded()
{
    // 로드가 끝나기 전에 이 기계의 플레이어가 탔으면 이제 넣는다 (탑승 중이 아니거나 이미 넣었으면 AddLocalInput 이 무시한다)
    AddLocalInput();
}

void AFEBuildBallista::CheckOperator()
{
    // 이벤트로 알 수 없는 경우들: 조작자가 사라짐(접속 끊김), 죽음(HT State.Dead), 밀려서 멀어짐
    APawn* Current = Operator;
    const UAbilitySystemComponent* ASC = IsValid(Current) ? UFE_CombatComponent::FindAbilitySystemComponent(Current) : nullptr;
    const bool bIsDead = ASC != nullptr && ASC->HasMatchingGameplayTag(FallenEraGameplayTags::State_Dead);
    const bool bTooFar = IsValid(Current) && FVector::Dist(Current->GetActorLocation(), Seat->GetComponentLocation()) > MaxOperatorDistance;
    if (!IsValid(Current) || bIsDead || bTooFar)
    {
        SetOperator(nullptr);
    }
}

void AFEBuildBallista::SetHeadAim(float Yaw, float Pitch)
{
    Head->SetRelativeRotation(FRotator(Pitch, Yaw, 0.f));
}

AFEBuildBallista* AFEBuildBallista::FindOperatedBy(const AActor* OperatorActor)
{
    // 상태를 저장하지 않고 찾는다: 발리스타는 몇 개 안 되고(클래스 해시로 발리스타만 돈다), 부르는 곳은 클릭(GA)과 0.1초 HUD 타이머뿐
    if (OperatorActor == nullptr) return nullptr;

    for (TActorIterator<AFEBuildBallista> It(OperatorActor->GetWorld()); It; ++It)
    {
        if (It->Operator.Get() == OperatorActor)
        {
            return *It;
        }
    }
    return nullptr;
}

void AFEBuildBallista::RequestFire()
{
    APlayerController* PlayerController = LocalController.Get();
    if (!bLocalOperation || PlayerController == nullptr) return; // 이 기계의 플레이어가 탄 발리스타만

    // 장전 중 — HUD 가 이미 "장전 중"을 보여 준다. 리플리케이트 값이라 누가 쐈든, 어느 발리스타로 옮겨 탔든 맞다
    if (IsReloading()) return;

    if (GetAmmoCount() <= 0)
    {
        UFEBuildingSubsystem::ShowLocalNotice(GetWorld(), LOCTEXT("BallistaNoAmmo", "탄약이 없습니다 (Tab: 탄약)"));
        return;
    }

    // 지금 화면의 조준(정밀값)으로 보낸다. 장전 시작은 서버가 정해 리플리케이트한다 —
    // 클라 예측이 없어서 서버가 거절해도 클라 장전이 따로 돌지 않는다 (응답 전 연타는 서버 요청 간격 제한이 버린다)
    const FVector2D Aim = ApplyLocalAim(PlayerController->GetControlRotation());
    ServerFire(Aim.X, Aim.Y);
}

void AFEBuildBallista::RequestExit()
{
    if (bLocalOperation)
    {
        ServerExit();
    }
}

void AFEBuildBallista::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    UpdateLocalAim(); // 틱은 이 기계의 플레이어가 탔을 때만 켜진다
}

void AFEBuildBallista::BeginLocalOperation(APawn* Pawn)
{
    APlayerController* PlayerController = Pawn != nullptr ? Pawn->GetController<APlayerController>() : nullptr;
    if (PlayerController == nullptr) return;

    bLocalOperation = true;
    LocalOperatorPawn = Pawn;
    LocalController = PlayerController;

    // 이동 잠금. 서버도 같은 처리를 한다 — 소유 클라의 예측이 서버와 어긋나 위치 보정이 생기지 않게
    LockOperatorMovement(Cast<ACharacter>(Pawn));

    // 시야를 발리스타 카메라로, 조준은 정면에서 시작
    PlayerController->SetViewTargetWithBlend(this, BallistaViewBlendTime);
    PlayerController->SetControlRotation(FRotator(0.f, GetActorRotation().Yaw, 0.f));

    // 탑승 입력(좌클릭 발사·E/우클릭 내림). IMC 가 아직 로드 전이면 로드 완료 콜백이 넣는다
    AddLocalInput();

    // State.Building.Mounted: State.Building 하위라 상호작용 프롬프트가 꺼지고, 건설 컴포넌트가 이 태그로 빌드 모드 진입을 막는다.
    // 로컬 loose 태그 — 다른 기계에는 보이지 않는다
    if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn))
    {
        ASC->SetLooseGameplayTagCount(FEBuildingTags::State_Building_Mounted, 1);
        LocalTagOwner = ASC;
    }

    LastAimSendTime = -1.0;    // 첫 틱에 바로 보낸다
    SetActorTickEnabled(true); // 조준 추종
    UE_LOG(LogFEBuilding, Log, TEXT("%s mounted %s"), *GetNameSafe(Pawn), *GetName());
}

void AFEBuildBallista::EndLocalOperation()
{
    if (!bLocalOperation) return;
    bLocalOperation = false;
    SetActorTickEnabled(false);

    // 탑승이 건 이동 잠금만 푼다 — 탑승 중 사망했으면 HT 사망 처리의 잠금이 그대로 남는다
    APawn* Pawn = LocalOperatorPawn.Get();
    UnlockOperatorMovement(Cast<ACharacter>(Pawn));

    // 시야: 아직 이 발리스타를 보고 있을 때만 컨트롤러의 지금 폰으로 되돌린다 (그새 다른 폰에 빙의했으면 빙의가 이미 시야를 옮겼다)
    APlayerController* PlayerController = LocalController.Get();
    if (PlayerController != nullptr && PlayerController->GetViewTarget() == this)
    {
        PlayerController->SetViewTargetWithBlend(PlayerController->GetPawn(), BallistaViewBlendTime);
    }

    // 입력·태그는 넣었던 곳에서 뺀다 (로컬 플레이어 입력·PlayerState 의 ASC 는 폰이 사라져도 남아 있다)
    UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalInputOwner.Get();
    UInputMappingContext* LoadedMappingContext = MappingContext.Get();
    if (InputSubsystem != nullptr && LoadedMappingContext != nullptr)
    {
        InputSubsystem->RemoveMappingContext(LoadedMappingContext);
    }
    if (UAbilitySystemComponent* ASC = LocalTagOwner.Get())
    {
        ASC->SetLooseGameplayTagCount(FEBuildingTags::State_Building_Mounted, 0);
    }

    // 다른 기계와 같은 각도로 맞춘다 (조작 중엔 로컬 조준이라 리플리케이트 값과 다를 수 있고, 값이 그대로면 OnRep 이 오지 않는다)
    SetHeadAim(AimYaw, AimPitch);
    UE_LOG(LogFEBuilding, Log, TEXT("%s dismounted %s"), *GetNameSafe(Pawn), *GetName());

    LocalOperatorPawn = nullptr;
    LocalController = nullptr;
    LocalInputOwner = nullptr;
    LocalTagOwner = nullptr;
}

void AFEBuildBallista::AddLocalInput()
{
    // 탑승 중이고 아직 안 넣었을 때만 — 탑승 때와 IMC 로드 완료 때 두 번 불려도 한 번만 들어간다
    APlayerController* PlayerController = LocalController.Get();
    UInputMappingContext* LoadedMappingContext = MappingContext.Get(); // 로드 전이면 null (동기 로드 없음)
    if (!bLocalOperation || LocalInputOwner.IsValid() || PlayerController == nullptr || LoadedMappingContext == nullptr) return;

    if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
    {
        // 전투(20)·빌드 모드(30)보다 높은 우선순위로 같은 키를 가져온다 → 개인 무기가 나가지 않는다
        InputSubsystem->AddMappingContext(LoadedMappingContext, MappingPriority);
        LocalInputOwner = InputSubsystem;
    }
}

void AFEBuildBallista::UpdateLocalAim()
{
    APlayerController* PlayerController = LocalController.Get();
    if (PlayerController == nullptr) return;

    // 범위로 자른 각도를 머리에 바로 적용하고, 컨트롤 회전도 그 범위로 되돌린다 — 마우스가 한계 밖으로 쌓이면 돌아올 때 멈춘 것처럼 느껴진다
    const FVector2D Aim = ApplyLocalAim(PlayerController->GetControlRotation());
    PlayerController->SetControlRotation(FRotator(Aim.Y, GetActorRotation().Yaw + Aim.X, 0.f));

    // 다른 플레이어용: 바뀌었으면 BallistaAimSendInterval 이하 빈도로, 그대로여도 BallistaAimRefreshInterval 마다 다시 보낸다
    const int8 Yaw = static_cast<int8>(FMath::RoundToInt(Aim.X)); // MaxYaw ≤ 127 이라 int8 안에 들어간다
    const int8 Pitch = static_cast<int8>(FMath::RoundToInt(Aim.Y));
    const double Now = GetWorld()->GetTimeSeconds();
    const double SinceLastSend = Now - LastAimSendTime;
    const bool bChanged = Yaw != LastSentAimYaw || Pitch != LastSentAimPitch;
    const bool bSendDue = LastAimSendTime < 0.0 || (bChanged && SinceLastSend >= BallistaAimSendInterval) || SinceLastSend >= BallistaAimRefreshInterval;
    if (bSendDue)
    {
        LastSentAimYaw = Yaw;
        LastSentAimPitch = Pitch;
        LastAimSendTime = Now;
        ServerSetAim(Yaw, Pitch);
    }
}

bool AFEBuildBallista::ServerFire_Validate(float Yaw, float Pitch)
{
    return FMath::IsFinite(Yaw) && FMath::IsFinite(Pitch); // NaN·무한대는 조작된 패킷. 범위는 TryFire 가 자른다
}

void AFEBuildBallista::ServerFire_Implementation(float Yaw, float Pitch)
{
    // 간격 제한이 가장 먼저 (수리와 같은 이유) — 빈 발리스타 연타 같은 요청도 거절 로그·안내가 이 간격당 1번.
    // 장전(ReloadTime)과는 별개라 정상 발사는 걸리지 않는다
    if (!FEBuilding::ConsumeRequestSlot(NextFireRequestTime, GetWorld()->GetTimeSeconds(), BallistaFireRequestInterval)) return;

    // 보낸 쪽은 엔진이 소유 연결(= 조작자)로 이미 걸렀다. 내린 뒤에는 Owner 가 없어 엔진이 버리고, 여기는 마지막 안전장치
    if (Operator == nullptr) return;

    FText Reason;
    if (!TryFire(Operator, Yaw, Pitch, Reason) && !Reason.IsEmpty())
    {
        UE_LOG(LogFEBuilding, Log, TEXT("Fire rejected: %s"), *Reason.ToString());
        ClientShowNotice(Reason);
    }
}

bool AFEBuildBallista::ServerSetAim_Validate(int8 Yaw, int8 Pitch)
{
    return true; // 범위는 SetReplicatedAim 이 자른다
}

void AFEBuildBallista::ServerSetAim_Implementation(int8 Yaw, int8 Pitch)
{
    // 간격 제한이 가장 먼저 — 조작된 클라가 쏟아 보내도 처리량이 정해진다. 정상 클라는 이 간격의 2배로 보낸다
    // (몰려 와서 하나를 버려도 0.5초 재전송으로 다시 맞춰진다)
    if (!FEBuilding::ConsumeRequestSlot(NextAimRequestTime, GetWorld()->GetTimeSeconds(), BallistaAimAcceptInterval)) return;

    if (Operator != nullptr)
    {
        SetReplicatedAim(Yaw, Pitch);
    }
}

bool AFEBuildBallista::ServerExit_Validate()
{
    return true;
}

void AFEBuildBallista::ServerExit_Implementation()
{
    SetOperator(nullptr); // 보낸 쪽은 엔진이 소유 연결(= 조작자)로 이미 걸렀다
}

void AFEBuildBallista::ClientShowNotice_Implementation(const FText& Text)
{
    UFEBuildingSubsystem::ShowLocalNotice(GetWorld(), Text);
}

#undef LOCTEXT_NAMESPACE