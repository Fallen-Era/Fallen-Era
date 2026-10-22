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
#include "InputMappingContext.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "FEBuilding"

namespace
{
    /** [Server] 탑승 중 조작자 상태 확인 간격 */
    constexpr float OperatorCheckInterval = 0.5f;
}

AFEBuildBallista::AFEBuildBallista()
{
    Capacity = 1; // 탄약 칸 1개. 한 칸 최대 개수는 MaxStackSize (BP 기본값)
    AmmoTag = FEBuildingTags::Item_Ammo_Bolt;

    Head = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Head"));
    Head->SetupAttachment(GetMesh());
    Head->SetCollisionEnabled(ECollisionEnabled::NoCollision); // 보이기만 — 총신이 벽·난간 자리 위로 나가도 설치를 막지 않게
    Head->SetCanEverAffectNavigation(false);

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(Head);

    Seat = CreateDefaultSubobject<USceneComponent>(TEXT("Seat"));
    Seat->SetupAttachment(GetMesh());
}

void AFEBuildBallista::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AFEBuildBallista, Operator);
    DOREPLIFETIME(AFEBuildBallista, AimYaw);
    DOREPLIFETIME(AFEBuildBallista, AimPitch);
}

void AFEBuildBallista::BeginPlay()
{
    Super::BeginPlay();

    // 탑승 입력은 화면이 있는 기계에서만 쓴다. 탑승 순간 멈칫하지 않게 미리 비동기로 받아 둔다
    if (GetNetMode() != NM_DedicatedServer && !MappingContext.IsNull())
    {
        MappingLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(MappingContext.ToSoftObjectPath());
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

    // 이전 조작자: 이동 복구 (이미 사라지는 중이면 건드리지 않는다)
    if (ACharacter* Previous = Cast<ACharacter>(Operator); IsValid(Previous))
    {
        Previous->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    }

    Operator = NewOperator;

    if (ACharacter* Character = Cast<ACharacter>(Operator))
    {
        // 좌석으로 옮기고(캡슐 중심 = 좌석 바닥 + 반높이) 이동을 잠근다. 소유 클라도 OnRep 에서 같은 잠금을 한다
        const FVector SeatLocation = Seat->GetComponentLocation() + FVector(0.f, 0.f, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
        Character->SetActorLocationAndRotation(SeatLocation, FRotator(0.f, GetActorRotation().Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
        Character->GetCharacterMovement()->DisableMovement();
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
    // BeginPlay 의 미리 로드가 끝났으면 바로 반환된다. 그 전에 탑승하는 경우만 작은 IMC 를 동기로 받는다
    return MappingContext.LoadSynchronous();
}

int32 AFEBuildBallista::GetMappingPriority() const
{
    return MappingPriority;
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