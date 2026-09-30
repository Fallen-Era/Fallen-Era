// Fill out your copyright notice in the Description page of Project Settings.

#include "FEInteractionComponent.h"
#include "FEInteractable.h"
#include "FEInteractPromptViewModel.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

UFEInteractionComponent::UFEInteractionComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true); // Server/Client RPC 에 필요

    ObjectTypes.Add(ECC_WorldStatic);
    ObjectTypes.Add(ECC_WorldDynamic);
    ObjectTypes.Add(ECC_Pawn);
    ObjectTypes.Add(ECC_PhysicsBody);
    ObjectTypes.Add(ECC_GameTraceChannel2); // BuildPiece (DefaultEngine.ini). 청사진은 Visibility 를 무시하므로 오브젝트 타입으로 잡는다
}

void UFEInteractionComponent::BeginPlay()
{
    Super::BeginPlay();
    // 틱 대신 10Hz 타이머. 프롬프트는 이 정도면 충분하고, 서버·원격 폰에서는 UpdatePrompt 가 바로 빠져나간다.
    GetWorld()->GetTimerManager().SetTimer(PromptTimer, this, &UFEInteractionComponent::UpdatePrompt, 0.1f, true);
}

UFEInteractPromptViewModel* UFEInteractionComponent::GetPromptViewModel()
{
    if (PromptViewModel == nullptr)
    {
        PromptViewModel = NewObject<UFEInteractPromptViewModel>(this);
    }
    return PromptViewModel;
}

void UFEInteractionComponent::UpdatePrompt()
{
    const APawn* Pawn = Cast<APawn>(GetOwner());
    if (Pawn == nullptr || !Pawn->IsLocallyControlled())
    {
        return;
    }
    AActor* Target = FindInteractTarget();
    const FText Prompt = Target ? IFEInteractable::Execute_GetInteractText(Target, GetOwner()) : FText::GetEmpty();
    GetPromptViewModel()->SetPrompt(Prompt);
}

bool UFEInteractionComponent::IsInteractable(const AActor* Target, AActor* InstigatorActor)
{
    const bool bImplements = IsValid(Target) && Target->GetClass()->ImplementsInterface(UFEInteractable::StaticClass());
    if (!bImplements)
    {
        return false;
    }
    return IFEInteractable::Execute_CanInteract(const_cast<AActor*>(Target), InstigatorActor);
}

void UFEInteractionComponent::TryInteract()
{
    if (AActor* Target = FindInteractTarget())
    {
        ServerInteract(Target);
    }
}

AActor* UFEInteractionComponent::FindInteractTarget() const
{
    AActor* Owner = GetOwner();
    const UWorld* World = GetWorld();
    FVector ViewLocation;
    FRotator ViewRotation;
    if (Owner == nullptr || World == nullptr || !GetViewPoint(ViewLocation, ViewRotation))
    {
        return nullptr;
    }

    const FCollisionObjectQueryParams ObjectQuery = MakeObjectQuery();
    FCollisionQueryParams Params(SCENE_QUERY_STAT(FEInteractAim), false, Owner);

    // 1) 조준한 대상 우선
    FHitResult Hit;
    const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * InteractDistance;
    if (World->LineTraceSingleByObjectType(Hit, ViewLocation, TraceEnd, ObjectQuery, Params))
    {
        if (IsInteractable(Hit.GetActor(), Owner))
        {
            return Hit.GetActor();
        }
    }

    // 2) 주변 후보 중 정면·근접 점수 최고. 점수 = 거리 × (2 − dot): 가깝고 정면일수록 작다
    TArray<FOverlapResult> Overlaps;
    const FVector OwnerLocation = Owner->GetActorLocation();
    World->OverlapMultiByObjectType(Overlaps, OwnerLocation, FQuat::Identity, ObjectQuery, FCollisionShape::MakeSphere(SearchRadius), Params);

    const FVector Forward2D = Owner->GetActorForwardVector().GetSafeNormal2D();
    AActor* Best = nullptr;
    float BestScore = TNumericLimits<float>::Max();
    for (const FOverlapResult& Overlap : Overlaps)
    {
        AActor* Candidate = Overlap.GetActor();
        if (Candidate == Best || !IsInteractable(Candidate, Owner))
        {
            continue;
        }
        const FVector ToCandidate = Candidate->GetActorLocation() - OwnerLocation;
        const float Distance = ToCandidate.Size2D();
        const float FacingDot = Distance > KINDA_SMALL_NUMBER ? FVector::DotProduct(Forward2D, ToCandidate.GetSafeNormal2D()) : 1.f;
        if (FacingDot < MinFacingDot)
        {
            continue;
        }
        const float Score = Distance * (2.f - FacingDot);
        if (Score < BestScore)
        {
            BestScore = Score;
            Best = Candidate;
        }
    }
    return Best;
}

bool UFEInteractionComponent::GetViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
    const APawn* Pawn = Cast<APawn>(GetOwner());
    if (Pawn == nullptr)
    {
        return false;
    }
    if (const APlayerController* PlayerController = Pawn->GetController<APlayerController>())
    {
        PlayerController->GetPlayerViewPoint(OutLocation, OutRotation);
    }
    else
    {
        Pawn->GetActorEyesViewPoint(OutLocation, OutRotation);
    }
    return true;
}

FCollisionObjectQueryParams UFEInteractionComponent::MakeObjectQuery() const
{
    FCollisionObjectQueryParams Query;
    for (const TEnumAsByte<ECollisionChannel>& Channel : ObjectTypes)
    {
        Query.AddObjectTypesToQuery(Channel);
    }
    return Query;
}

bool UFEInteractionComponent::ServerInteract_Validate(AActor* Target)
{
    return Target != nullptr;
}

void UFEInteractionComponent::ServerInteract_Implementation(AActor* Target)
{
    AActor* Owner = GetOwner();
    const bool bCanHandle = Owner != nullptr && Owner->HasAuthority() && IsValid(Target);
    if (!bCanHandle)
    {
        return;
    }

    // 클라 타게팅은 조준 거리 + 탐색 반경 안. 지연을 감안해 여유를 준다.
    const float MaxReach = InteractDistance + SearchRadius + 200.f;
    if (FVector::Dist(Owner->GetActorLocation(), Target->GetActorLocation()) > MaxReach)
    {
        return;
    }
    if (!IsInteractable(Target, Owner))
    {
        return;
    }

    IFEInteractable::Execute_Interact(Target, Owner);
    ClientInteracted(Target);
}

void UFEInteractionComponent::ClientInteracted_Implementation(AActor* Target)
{
    const bool bImplements = IsValid(Target) && Target->GetClass()->ImplementsInterface(UFEInteractable::StaticClass());
    if (bImplements)
    {
        IFEInteractable::Execute_InteractLocal(Target, GetOwner());
    }
}

