#include "Combat/Tests/FEEnemyObstacleTestActor.h"

#include "Combat/Collision/FECollisionChannels.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationPath.h"

AFE_EnemyObstacleTestActor::AFE_EnemyObstacleTestActor()
{
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	Box->SetupAttachment(RootComponent);
	Box->SetBoxExtent(FVector(25.0f, 200.0f, 200.0f));
	Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Box->SetCollisionResponseToAllChannels(ECR_Ignore);
	Box->SetCollisionResponseToChannel(FECollisionChannels::EnemyTrace, ECR_Block);
}

FFE_CombatDamageResult AFE_EnemyObstacleTestActor::ReceiveCombatDamage_Implementation(const FFE_CombatDamageRequest& Request)
{
	++HitCount;
	LastRequest = Request;
	FFE_CombatDamageResult Result;
	Result.bHandled = Result.bDamageApplied = true;
	Result.AppliedDamage = Request.SourceAttackPower;
	return Result;
}

FPathFollowingRequestResult AFE_EnemyObstacleTestController::MoveTo(const FAIMoveRequest& MoveRequest, FNavPathSharedPtr* OutPath)
{
	++MoveRequests;
	FPathFollowingRequestResult Result;
	Result.Code = EPathFollowingRequestResult::Failed;
	if (bProvidePartialPath)
	{
		FNavPathSharedPtr Path = MakeShared<FNavigationPath>();
		const FVector Start = GetPawn()->GetNavAgentLocation();
		Path->GetPathPoints().Emplace(Start);
		Path->GetPathPoints().Emplace(Start + FVector(40.0f, 0.0f, 0.0f));
		Path->SetIsPartial(true);
		Path->MarkReady();
		Result.MoveId = GetPathFollowingComponent()->RequestMove(MoveRequest, Path);
		Result.Code = Result.MoveId.IsValid() ? EPathFollowingRequestResult::RequestSuccessful : EPathFollowingRequestResult::Failed;
		if (OutPath) { *OutPath = Path; }
	}
	return Result;
}
