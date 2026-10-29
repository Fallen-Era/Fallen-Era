// Fill out your copyright notice in the Description page of Project Settings.

#include "FEBuildTrap.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/FallenEraGameplayTags.h"
#include "Combat/Component/FECombatComponent.h"
#include "Components/StaticMeshComponent.h"
#include "TimerManager.h"

namespace
{
    /** 완성된 함정 메시의 콜리전 프로필 (DefaultEngine.ini). 적·플레이어 Overlap, 공격 트레이스 Ignore */
    const FName TrapCollisionProfile(TEXT("BuildTrap"));
}

FGenericTeamId AFEBuildTrap::GetGenericTeamId() const
{
    return FGenericTeamId::NoTeam;
}

void AFEBuildTrap::BeginPlay()
{
    Super::BeginPlay();
    if (!HasAuthority()) return; // 피해 판정은 서버만

    UStaticMeshComponent* TrapMesh = GetMesh();
    TrapMesh->OnComponentBeginOverlap.AddDynamic(this, &AFEBuildTrap::HandleBeginOverlap);
    TrapMesh->OnComponentEndOverlap.AddDynamic(this, &AFEBuildTrap::HandleEndOverlap);

    // 바인딩 전에 이미 겹쳐 있던 대상 (복원된 완성 함정 위에 서 있던 경우)
    TArray<AActor*> AlreadyOverlapping;
    TrapMesh->GetOverlappingActors(AlreadyOverlapping);
    for (AActor* Actor : AlreadyOverlapping)
    {
        HandleBeginOverlap(TrapMesh, Actor, nullptr, INDEX_NONE, false, FHitResult());
    }
}

void AFEBuildTrap::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 함정이 부서지거나 철거돼도 걸어 둔 감속 태그가 남지 않게
    while (!Victims.IsEmpty())
    {
        RemoveVictimAt(Victims.Num() - 1);
    }
    GetWorldTimerManager().ClearTimer(DamageTimer);
    Super::EndPlay(EndPlayReason);
}

void AFEBuildTrap::OnApplyState(EFEBuildPieceState NewState)
{
    Super::OnApplyState(NewState);
    if (NewState != EFEBuildPieceState::Built) return;

    // 완성된 함정: 적·플레이어가 밟고 지나가며 겹침으로 맞는다. 내비를 막지 않아 좀비가 피해 가지 않는다
    UStaticMeshComponent* TrapMesh = GetMesh();
    TrapMesh->SetCollisionProfileName(TrapCollisionProfile);
    TrapMesh->SetCanEverAffectNavigation(false);
    TrapMesh->SetGenerateOverlapEvents(true);
    if (TrapMesh->IsRegistered())
    {
        TrapMesh->UpdateOverlaps(); // 청사진이 완성되는 순간 그 위에 서 있던 대상도 바로 잡는다
    }
}

void AFEBuildTrap::HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    const bool bIsTarget = GetState() == EFEBuildPieceState::Built && OtherActor != nullptr && OtherActor != this
        && OtherActor->GetClass()->ImplementsInterface(UFE_Damageable::StaticClass());
    const bool bAlreadyTracked = Victims.ContainsByPredicate([OtherActor](const FFEBuildTrapVictim& Victim)
    {
        return Victim.Actor == OtherActor; // 캡슐·메시처럼 한 액터의 여러 컴포넌트가 겹쳐도 한 명
    });
    if (!bIsTarget || bAlreadyTracked) return;

    FFEBuildTrapVictim& Victim = Victims.AddDefaulted_GetRef();
    Victim.Actor = OtherActor;
    if (SlowTag.IsValid())
    {
        if (UAbilitySystemComponent* ASC = UFE_CombatComponent::FindAbilitySystemComponent(OtherActor))
        {
            // 서버 전용 loose 태그(복제 안 함, AI가 서버에서 읽는다). 겹친 철조망마다 +1 이라 하나에서 나와도 다른 철조망의 감속은 남는다
            ASC->AddLooseGameplayTag(SlowTag);
            Victim.TaggedASC = ASC;
        }
    }

    // 타이머가 꺼져 있으면(첫 대상) 바로 한 번 맞고, 이미 돌고 있으면 다음 주기에 다른 대상과 같이 맞는다 (대상마다 타이머를 두지 않는다).
    // 대상이 없으면 TickDamage 가 타이머를 끈다
    if (!GetWorldTimerManager().IsTimerActive(DamageTimer))
    {
        GetWorldTimerManager().SetTimer(DamageTimer, this, &AFEBuildTrap::TickDamage, DamageInterval, true, 0.f);
    }
}

void AFEBuildTrap::HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    // 같은 액터의 다른 컴포넌트가 아직 겹쳐 있으면 그대로 둔다
    if (OtherActor == nullptr || GetMesh()->IsOverlappingActor(OtherActor)) return;

    const int32 Index = Victims.IndexOfByPredicate([OtherActor](const FFEBuildTrapVictim& Victim)
    {
        return Victim.Actor == OtherActor;
    });
    if (Index != INDEX_NONE)
    {
        RemoveVictimAt(Index);
    }
}

void AFEBuildTrap::TickDamage()
{
    // 사라졌거나 죽은 대상 정리 — 쓰러진 몸 위에서 내구도를 쓰지 않게
    for (int32 Index = Victims.Num() - 1; Index >= 0; --Index)
    {
        AActor* Actor = Victims[Index].Actor.Get();
        const UAbilitySystemComponent* ASC = Actor ? UFE_CombatComponent::FindAbilitySystemComponent(Actor) : nullptr;
        const bool bIsDead = ASC != nullptr && ASC->HasMatchingGameplayTag(FallenEraGameplayTags::State_Dead);
        if (!IsValid(Actor) || bIsDead)
        {
            RemoveVictimAt(Index);
        }
    }
    if (Victims.IsEmpty())
    {
        GetWorldTimerManager().ClearTimer(DamageTimer);
        return;
    }

    // 복사본으로 돈다 — 피해로 대상이 죽거나 사라지면 EndOverlap 이 그 사이에 목록을 바꾼다
    const TArray<FFEBuildTrapVictim> Snapshot = Victims;
    for (const FFEBuildTrapVictim& Victim : Snapshot)
    {
        if (IsActorBeingDestroyed()) return; // 내구도가 다 닳아 이번 틱에 부서졌다

        AActor* Actor = Victim.Actor.Get();
        if (Actor != nullptr && DealDamage(Actor))
        {
            UE_LOG(LogFEBuilding, Verbose, TEXT("%s hit %s"), *GetName(), *Actor->GetName());
            if (SelfDamagePerHit > 0.f)
            {
                ApplyStructureDamage(SelfDamagePerHit, Actor); // 피해를 줄 때마다 내구도 감소. 0 이면 파괴 연출 후 Destroy
            }
        }
    }
}

void AFEBuildTrap::RemoveVictimAt(int32 Index)
{
    if (UAbilitySystemComponent* ASC = Victims[Index].TaggedASC.Get())
    {
        ASC->RemoveLooseGameplayTag(SlowTag);
    }
    Victims.RemoveAtSwap(Index);
}