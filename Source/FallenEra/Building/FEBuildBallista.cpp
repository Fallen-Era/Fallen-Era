// Fill out your copyright notice in the Description page of Project Settings.

#include "FEBuildBallista.h"
#include "FEBuildingComponent.h"
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
}

AFEBuildBallista::AFEBuildBallista()
{
    Capacity = 1; // 탄약 칸 1개. 한 칸 최대 개수는 MaxStackSize (BP 기본값)
    AmmoTag = FEBuildingTags::Item_Ammo_Bolt;
    
    // 함정과 달리 ASC 를 복제한다 — 나중에 볼트에 AttackData(명중 효과)를 붙이면 공격자 ASC 로 GameplayCue 가 나간다
    AbilitySystem->SetIsReplicated(true);

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
    // 부서지거나 철거되면 조작자를 내린다 (서버 = 이동 복구 + 호스트 화면, 원격 조작자 기계 = 시야·입력 복구)
    if (HasAuthority())
    {
        SetOperator(nullptr);
    }
    else if (APawn* Known = LocalKnownOperator.Get(); Known != nullptr && Known->IsLocallyControlled())
    {
        if (UFEBuildingComponent* Building = Known->FindComponentByClass<UFEBuildingComponent>())
        {
            Building->HandleBallistaDismounted(this);
        }
    }
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

APawn* AFEBuildBallista::GetOperator() const
{
    return Operator;
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

UInputMappingContext* AFEBuildBallista::GetMappingContext() const
{
    // 이미 로드됐을 때만 돌려준다. 로드 전에 탄 경우는 HandleMappingContextLoaded 가 나중에 넣는다 (게임 스레드를 막는 동기 로드 없음)
    return MappingContext.Get();
}

int32 AFEBuildBallista::GetMappingPriority() const
{
    return MappingPriority;
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
    APawn* Previous = LocalKnownOperator.Get();
    LocalKnownOperator = Operator;

    // 이 기계의 플레이어가 내렸다
    if (Previous != nullptr && Previous != Operator && Previous->IsLocallyControlled())
    {
        if (UFEBuildingComponent* Building = Previous->FindComponentByClass<UFEBuildingComponent>())
        {
            Building->HandleBallistaDismounted(this);
        }
    }

    // 이 기계의 플레이어가 탔다
    if (Operator != nullptr && Operator != Previous && Operator->IsLocallyControlled())
    {
        if (UFEBuildingComponent* Building = Operator->FindComponentByClass<UFEBuildingComponent>())
        {
            Building->HandleBallistaMounted(this);
        }
    }
}

void AFEBuildBallista::OnRep_Aim()
{
    // 조작자 기계는 로컬 조준을 쓴다 — 서버 값은 늦게 도착하므로 덮어쓰면 화면이 뒤로 튄다
    if (Operator != nullptr && Operator->IsLocallyControlled()) return;
    SetHeadAim(AimYaw, AimPitch);
}

void AFEBuildBallista::HandleMappingContextLoaded()
{
    // 로드가 끝나기 전에 이 기계의 플레이어가 탔으면 이제 입력을 넣는다 (이미 넣었으면 AddBallistaInput 이 무시한다)
    APawn* Known = LocalKnownOperator.Get();
    if (Known == nullptr || !Known->IsLocallyControlled()) return;

    if (UFEBuildingComponent* Building = Known->FindComponentByClass<UFEBuildingComponent>())
    {
        Building->AddBallistaInput();
    }
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

#undef LOCTEXT_NAMESPACE