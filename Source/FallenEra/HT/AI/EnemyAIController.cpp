#include "HT/AI/EnemyAIController.h"

#include "AbilitySystem/FallenEraGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HT/Character/EnemyCharacter.h"
#include "HT/Combat/FECombatTeams.h"
#include "HT/Component/CombatComponent.h"
#include "HT/Interface/Damageable.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense.h"
#include "Perception/AISenseConfig_Damage.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Damage.h"
#include "Perception/AISense_Hearing.h"
#include "Perception/AISense_Sight.h"
#include "TimerManager.h"

AFE_EnemyAIController::AFE_EnemyAIController()
{
	PrimaryActorTick.bCanEverTick = false;
	SetGenericTeamId(FECombatTeams::Enemy);

	EnemyPerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("EnemyPerceptionComponent"));
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("HearingConfig"));
	DamageConfig = CreateDefaultSubobject<UAISenseConfig_Damage>(TEXT("DamageConfig"));
	ApplyPerceptionSettings();
	EnemyPerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(
		this, &AFE_EnemyAIController::HandleTargetPerceptionUpdated);
	SetPerceptionComponent(*EnemyPerceptionComponent);
}

ETeamAttitude::Type AFE_EnemyAIController::GetTeamAttitudeTowards(const AActor& Other) const
{
	const FGenericTeamId OtherTeam = FGenericTeamId::GetTeamIdentifier(&Other);
	const APawn* OtherPawn = Cast<APawn>(&Other);
	if (OtherTeam.GetId() == FECombatTeams::Player.GetId() ||
		(OtherPawn && OtherPawn->IsPlayerControlled()))
	{
		return ETeamAttitude::Hostile;
	}
	if (OtherTeam.GetId() == FECombatTeams::Enemy.GetId())
	{
		return ETeamAttitude::Friendly;
	}
	return ETeamAttitude::Neutral;
}

void AFE_EnemyAIController::GetActorEyesViewPoint(
	FVector& OutLocation,
	FRotator& OutRotation) const
{
	if (const APawn* ControlledPawn = GetPawn())
	{
		OutLocation = ControlledPawn->GetPawnViewLocation();
		// This controller intentionally has no per-frame tick. APawn::GetViewRotation would
		// otherwise return the stale ControlRotation and freeze sight on its initial +X axis.
		OutRotation = ControlledPawn->GetActorRotation();
		return;
	}

	Super::GetActorEyesViewPoint(OutLocation, OutRotation);
}

void AFE_EnemyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	if (!HasAuthority() || !Cast<AFE_EnemyCharacter>(InPawn))
	{
		return;
	}

	HomeLocation = InPawn->GetActorLocation();
	bUsesManagedSimulation = CastChecked<AFE_EnemyCharacter>(InPawn)->IsSpawnManaged();
	ApplyPerceptionSettings();
	EnemyPerceptionComponent->RequestStimuliListenerUpdate();
	const float SafeInterval = FMath::Max(0.1f, DecisionInterval);
	NextPatrolRequestTime = GetWorld()->GetTimeSeconds() + FMath::FRandRange(0.0f, SafeInterval);
	SetDecisionTimerRate(SafeInterval, FMath::FRandRange(0.0f, SafeInterval));
}

void AFE_EnemyAIController::OnUnPossess()
{
	GetWorldTimerManager().ClearTimer(DecisionTimerHandle);
	ClearTarget();
	bUsesManagedSimulation = false;
	Super::OnUnPossess();
}

void AFE_EnemyAIController::SetManagedSimulationActive(bool bActive)
{
	if (!HasAuthority())
	{
		return;
	}
	bUsesManagedSimulation = true;
	SetAIActive(bActive);
}

bool AFE_EnemyAIController::SetEncounterCombatTarget(AActor* TargetActor)
{
	if (!HasAuthority() || !IsValidTarget(TargetActor))
	{
		return false;
	}

	SetAIActive(true);
	ClearInvestigation();
	StopMovement();
	CurrentTarget = TargetActor;
	bCurrentTargetVisible = HasActiveSightStimulus(*TargetActor);
	bEncounterTargetPendingSight = !bCurrentTargetVisible;
	TargetLostTime = -1.0f;
	return true;
}

void AFE_EnemyAIController::OnMoveCompleted(
	FAIRequestID RequestID,
	const FPathFollowingResult& Result)
{
	Super::OnMoveCompleted(RequestID, Result);
	if (InvestigationMoveRequestId.IsEquivalent(RequestID))
	{
		// Path following already accepted the projected NavMesh goal. The raw sound
		// location can differ in height or obstacle offset, so completion is authoritative.
		ClearInvestigation();
		ScheduleNextPatrolRequest();
		return;
	}
	if (!CurrentTarget.IsValid())
	{
		ScheduleNextPatrolRequest();
	}
}

void AFE_EnemyAIController::HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!HasAuthority() || !Actor)
	{
		return;
	}
	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		return;
	}

	const FAISenseID SightSenseId = UAISense::GetSenseID<UAISense_Sight>();
	const FAISenseID HearingSenseId = UAISense::GetSenseID<UAISense_Hearing>();
	const bool bSightStimulus = Stimulus.Type == SightSenseId;
	const bool bHearingStimulus = Stimulus.Type == HearingSenseId;

	if (bHearingStimulus)
	{
		if (Stimulus.WasSuccessfullySensed() && IsValidTarget(Actor))
		{
			SetAIActive(true);
			if (!HasActiveSightStimulus(*Actor))
			{
				if (!CurrentTarget.IsValid())
				{
					// Hearing remembers the reported position, not the moving source actor.
					SetInvestigationLocation(Stimulus.StimulusLocation);
				}
				return;
			}
		}
		else
		{
			return;
		}
	}

	if (Stimulus.WasSuccessfullySensed() && IsValidTarget(Actor))
	{
		SetAIActive(true);
		const bool bSelectedNewTarget = !CurrentTarget.IsValid() ||
			FVector::DistSquared(ControlledPawn->GetActorLocation(), Actor->GetActorLocation()) <
			FVector::DistSquared(ControlledPawn->GetActorLocation(), CurrentTarget->GetActorLocation());
		if (bSelectedNewTarget)
		{
			ClearInvestigation();
			StopMovement();
			CurrentTarget = Actor;
			bCurrentTargetVisible = HasActiveSightStimulus(*Actor);
			bEncounterTargetPendingSight = false;
			// Hearing/damage acquisition is still an active perception. Do not start the
			// forget timer until that stimulus expires or sight is acquired and then lost.
			TargetLostTime = -1.0f;
		}
		if (CurrentTarget.Get() == Actor)
		{
			if (bSightStimulus ||
				HasActiveSightStimulus(*Actor))
			{
				bCurrentTargetVisible = true;
				bEncounterTargetPendingSight = false;
				TargetLostTime = -1.0f;
			}
			else if (!bCurrentTargetVisible && TargetLostTime >= 0.0f)
			{
				// A new hearing/damage event refreshes an already-running lost-target timer.
				TargetLostTime = GetWorld()->GetTimeSeconds();
			}
		}
	}
	else if (CurrentTarget.Get() == Actor &&
		bSightStimulus)
	{
		// Hearing and damage stimuli have short MaxAge values. Their expiration must not
		// discard a target that sight is still tracking.
		bCurrentTargetVisible = false;
		TargetLostTime = GetWorld()->GetTimeSeconds();
	}
}

void AFE_EnemyAIController::UpdateDecision()
{
	AFE_EnemyCharacter* EnemyCharacter = Cast<AFE_EnemyCharacter>(GetPawn());
	if (!EnemyCharacter || EnemyCharacter->IsDead())
	{
		StopMovement();
		ClearTarget();
		if (EnemyCharacter)
		{
			EnemyCharacter->CancelActiveAttack();
			EnemyCharacter->SetNavigationInvokerActive(false);
		}
		if (EnemyPerceptionComponent)
		{
			EnemyPerceptionComponent->SetSenseEnabled(UAISense_Sight::StaticClass(), false);
			EnemyPerceptionComponent->SetSenseEnabled(UAISense_Hearing::StaticClass(), false);
			EnemyPerceptionComponent->SetSenseEnabled(UAISense_Damage::StaticClass(), false);
		}
		GetWorldTimerManager().ClearTimer(DecisionTimerHandle);
		return;
	}

	if (UAbilitySystemComponent* AbilitySystem = EnemyCharacter->GetAbilitySystemComponent();
		AbilitySystem && AbilitySystem->HasMatchingGameplayTag(FallenEraGameplayTags::State_Stunned))
	{
		StopMovement();
		ClearFocus(EAIFocusPriority::Gameplay);
		EnemyCharacter->CancelActiveAttack();
		return;
	}

	if (!bUsesManagedSimulation)
	{
		SetAIActive(IsPlayerWithinActivationDistance());
	}
	if (!bAIActive)
	{
		return;
	}

	AActor* TargetActor = CurrentTarget.Get();
	if (TargetActor && HasActiveSightStimulus(*TargetActor))
	{
		bCurrentTargetVisible = true;
		bEncounterTargetPendingSight = false;
		TargetLostTime = -1.0f;
	}
	else if (TargetActor && bCurrentTargetVisible)
	{
		bCurrentTargetVisible = false;
		TargetLostTime = GetWorld()->GetTimeSeconds();
	}
	else if (TargetActor && !bEncounterTargetPendingSight && TargetLostTime < 0.0f &&
		!HasActiveNonSightStimulus(*TargetActor))
	{
		// A target acquired only by hearing/damage keeps being investigated while that
		// stimulus is active. Once it expires without sight acquisition, start forgetting.
		TargetLostTime = GetWorld()->GetTimeSeconds();
	}
	if (TargetActor && (!IsValidTarget(TargetActor) ||
		(TargetLostTime >= 0.0f &&
			GetWorld()->GetTimeSeconds() - TargetLostTime > FMath::Max(0.0f, TargetForgetDelay))))
	{
		ClearTarget();
		TargetActor = nullptr;
	}

	if (TargetActor)
	{
		UpdateCombat(*EnemyCharacter, *TargetActor);
	}
	else if (bHasInvestigationLocation)
	{
		UpdateInvestigation(*EnemyCharacter);
	}
	else
	{
		UpdatePatrol(*EnemyCharacter);
	}
}

void AFE_EnemyAIController::UpdateInvestigation(AFE_EnemyCharacter& EnemyCharacter)
{
	SetMovementSpeed(EnemyCharacter, EnemyCharacter.GetChaseMovementSpeed());
	ClearFocus(EAIFocusPriority::Gameplay);

	if (GetMoveStatus() == EPathFollowingStatus::Moving)
	{
		return;
	}

	FAIMoveRequest MoveRequest;
	MoveRequest.SetGoalLocation(InvestigationLocation);
	MoveRequest.SetAcceptanceRadius(FMath::Max(0.0f, InvestigationAcceptanceRadius));
	MoveRequest.SetUsePathfinding(true);
	MoveRequest.SetProjectGoalLocation(true);
	MoveRequest.SetCanStrafe(false);
	MoveRequest.SetReachTestIncludesAgentRadius(true);
	MoveRequest.SetAllowPartialPath(true);

	const FPathFollowingRequestResult RequestResult = MoveTo(MoveRequest);
	if (RequestResult.Code == EPathFollowingRequestResult::RequestSuccessful)
	{
		InvestigationMoveRequestId = RequestResult.MoveId;
	}
	else
	{
		// AlreadyAtGoal and Failed both finish this investigation. This prevents a
		// projected or unreachable sound point from permanently blocking Patrol.
		ClearInvestigation();
		ScheduleNextPatrolRequest();
	}
}

void AFE_EnemyAIController::UpdatePatrol(AFE_EnemyCharacter& EnemyCharacter)
{
	SetMovementSpeed(EnemyCharacter, EnemyCharacter.GetPatrolMovementSpeed());
	ClearFocus(EAIFocusPriority::Gameplay);
	if (!GetWorld() || GetMoveStatus() == EPathFollowingStatus::Moving ||
		GetWorld()->GetTimeSeconds() < NextPatrolRequestTime)
	{
		return;
	}

	UNavigationSystemV1* NavigationSystem = UNavigationSystemV1::GetCurrent(GetWorld());
	FNavLocation PatrolLocation;
	if (NavigationSystem && NavigationSystem->GetRandomReachablePointInRadius(
		HomeLocation, FMath::Max(0.0f, PatrolRadius), PatrolLocation))
	{
		MoveToLocation(PatrolLocation.Location, FMath::Max(0.0f, PatrolAcceptanceRadius));
	}
	else
	{
		NextPatrolRequestTime = GetWorld()->GetTimeSeconds() + 1.0f;
	}
}

void AFE_EnemyAIController::UpdateCombat(AFE_EnemyCharacter& EnemyCharacter, AActor& TargetActor)
{
	SetMovementSpeed(EnemyCharacter, EnemyCharacter.GetChaseMovementSpeed());
	SetFocus(&TargetActor, EAIFocusPriority::Gameplay);
	const float AttackRange = FMath::Max(0.0f, EnemyCharacter.GetAttackRange());
	const float DistanceSquared = FVector::DistSquared(
		EnemyCharacter.GetActorLocation(), TargetActor.GetActorLocation());
	if (DistanceSquared <= FMath::Square(AttackRange) && LineOfSightTo(&TargetActor))
	{
		StopMovement();
		FVector FacingDirection = TargetActor.GetActorLocation() - EnemyCharacter.GetActorLocation();
		FacingDirection.Z = 0.0f;
		if (!FacingDirection.IsNearlyZero())
		{
			EnemyCharacter.SetActorRotation(FacingDirection.Rotation());
		}
		EnemyCharacter.TryStartAttack(&TargetActor);
		return;
	}

	if (GetMoveStatus() != EPathFollowingStatus::Moving)
	{
		MoveToActor(&TargetActor, FMath::Max(50.0f, AttackRange * 0.8f), true, true, true);
	}
}

void AFE_EnemyAIController::ClearTarget()
{
	StopMovement();
	CurrentTarget.Reset();
	TargetLostTime = -1.0f;
	bCurrentTargetVisible = false;
	bEncounterTargetPendingSight = false;
	ClearInvestigation();
	ClearFocus(EAIFocusPriority::Gameplay);
}

void AFE_EnemyAIController::SetInvestigationLocation(const FVector& Location)
{
	if (!FAISystem::IsValidLocation(Location))
	{
		return;
	}

	StopMovement();
	CurrentTarget.Reset();
	bCurrentTargetVisible = false;
	bEncounterTargetPendingSight = false;
	TargetLostTime = -1.0f;
	InvestigationLocation = Location;
	bHasInvestigationLocation = true;
	ClearFocus(EAIFocusPriority::Gameplay);
}

void AFE_EnemyAIController::ClearInvestigation()
{
	InvestigationMoveRequestId = FAIRequestID::InvalidRequest;
	bHasInvestigationLocation = false;
	InvestigationLocation = FVector::ZeroVector;
}

bool AFE_EnemyAIController::IsValidTarget(const AActor* Actor) const
{
	if (!IsValid(Actor) || !Actor->GetClass()->ImplementsInterface(UFE_Damageable::StaticClass()) ||
		!IsPlayerTarget(Actor))
	{
		return false;
	}

	if (UAbilitySystemComponent* AbilitySystem = UFE_CombatComponent::FindAbilitySystemComponent(
		const_cast<AActor*>(Actor)))
	{
		return !AbilitySystem->HasMatchingGameplayTag(FallenEraGameplayTags::State_Dead);
	}
	return true;
}

bool AFE_EnemyAIController::IsPlayerWithinActivationDistance() const
{
	const APawn* ControlledPawn = GetPawn();
	const UWorld* World = GetWorld();
	if (!ControlledPawn || !World)
	{
		return false;
	}

	const float ActivationDistanceSquared = FMath::Square(FMath::Max(0.0f, ActivationDistance));
	for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		const APlayerController* PlayerController = Iterator->Get();
		const APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
		if (PlayerPawn &&
			FVector::DistSquared(ControlledPawn->GetActorLocation(), PlayerPawn->GetActorLocation()) <=
			ActivationDistanceSquared)
		{
			return true;
		}
	}
	return false;
}

void AFE_EnemyAIController::SetAIActive(bool bNewActive)
{
	if (bAIActive == bNewActive)
	{
		return;
	}

	bAIActive = bNewActive;
	if (EnemyPerceptionComponent)
	{
		// Sight performs continuous queries, so it sleeps with distant AI. Hearing and damage
		// stay enabled for legacy placed AI, while centrally managed AI sleeps completely.
		EnemyPerceptionComponent->SetSenseEnabled(UAISense_Sight::StaticClass(), bAIActive);
		EnemyPerceptionComponent->SetSenseEnabled(
			UAISense_Hearing::StaticClass(), bAIActive || !bUsesManagedSimulation);
		EnemyPerceptionComponent->SetSenseEnabled(
			UAISense_Damage::StaticClass(), bAIActive || !bUsesManagedSimulation);
	}
	if (AFE_EnemyCharacter* EnemyCharacter = Cast<AFE_EnemyCharacter>(GetPawn()))
	{
		if (!bAIActive)
		{
			EnemyCharacter->CancelActiveAttack();
		}
		EnemyCharacter->SetNavigationInvokerActive(bAIActive);
	}

	if (bAIActive)
	{
		SetDecisionTimerRate(FMath::Max(0.1f, DecisionInterval));
	}
	else
	{
		ClearTarget();
		if (bUsesManagedSimulation)
		{
			GetWorldTimerManager().ClearTimer(DecisionTimerHandle);
		}
		else
		{
			SetDecisionTimerRate(FMath::Max(0.1f, DormantDecisionInterval));
		}
	}
}

void AFE_EnemyAIController::ApplyPerceptionSettings()
{
	if (!EnemyPerceptionComponent || !SightConfig || !HearingConfig || !DamageConfig)
	{
		return;
	}

	SightConfig->SightRadius = FMath::Max(0.0f, SightRadius);
	SightConfig->LoseSightRadius = FMath::Max(SightConfig->SightRadius, LoseSightRadius);
	SightConfig->PeripheralVisionAngleDegrees = FMath::Clamp(PeripheralVisionAngleDegrees, 0.0f, 180.0f);
	SightConfig->SetMaxAge(FMath::Max(0.0f, SightMaxAge));
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = false;
	// A player-controlled pawn without a team is neutral. IsValidTarget still rejects other neutral actors.
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;

	HearingConfig->HearingRange = FMath::Max(0.0f, HearingRange);
	HearingConfig->SetMaxAge(FMath::Max(0.0f, HearingMaxAge));
	HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
	HearingConfig->DetectionByAffiliation.bDetectFriendlies = false;
	HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;

	DamageConfig->SetMaxAge(FMath::Max(0.0f, DamageMaxAge));
	EnemyPerceptionComponent->ConfigureSense(*SightConfig);
	EnemyPerceptionComponent->ConfigureSense(*HearingConfig);
	EnemyPerceptionComponent->ConfigureSense(*DamageConfig);
	EnemyPerceptionComponent->SetDominantSense(UAISense_Sight::StaticClass());
}

bool AFE_EnemyAIController::IsPlayerTarget(const AActor* Actor) const
{
	if (!Actor)
	{
		return false;
	}
	if (FGenericTeamId::GetTeamIdentifier(Actor).GetId() == FECombatTeams::Player.GetId())
	{
		return true;
	}
	const APawn* TargetPawn = Cast<APawn>(Actor);
	return TargetPawn && TargetPawn->IsPlayerControlled();
}

bool AFE_EnemyAIController::HasActiveSightStimulus(const AActor& Actor) const
{
	return EnemyPerceptionComponent && EnemyPerceptionComponent->HasActiveStimulus(
		Actor, UAISense::GetSenseID<UAISense_Sight>());
}

bool AFE_EnemyAIController::HasActiveNonSightStimulus(const AActor& Actor) const
{
	return EnemyPerceptionComponent &&
		(EnemyPerceptionComponent->HasActiveStimulus(
			Actor, UAISense::GetSenseID<UAISense_Hearing>()) ||
		 EnemyPerceptionComponent->HasActiveStimulus(
			Actor, UAISense::GetSenseID<UAISense_Damage>()));
}

void AFE_EnemyAIController::SetMovementSpeed(
	AFE_EnemyCharacter& EnemyCharacter,
	float MovementSpeed) const
{
	if (UCharacterMovementComponent* MovementComponent = EnemyCharacter.GetCharacterMovement())
	{
		MovementComponent->MaxWalkSpeed = FMath::Max(0.0f, MovementSpeed);
	}
}

void AFE_EnemyAIController::SetDecisionTimerRate(float Interval, float InitialDelay)
{
	const float SafeInterval = FMath::Max(0.1f, Interval);
	GetWorldTimerManager().SetTimer(
		DecisionTimerHandle,
		this,
		&AFE_EnemyAIController::UpdateDecision,
		SafeInterval,
		true,
		InitialDelay >= 0.0f ? InitialDelay : SafeInterval);
}

void AFE_EnemyAIController::ScheduleNextPatrolRequest()
{
	if (!GetWorld())
	{
		return;
	}

	const float MinWait = FMath::Max(0.0f, FMath::Min(PatrolWaitRange.X, PatrolWaitRange.Y));
	const float MaxWait = FMath::Max(MinWait, FMath::Max(PatrolWaitRange.X, PatrolWaitRange.Y));
	NextPatrolRequestTime = GetWorld()->GetTimeSeconds() + FMath::FRandRange(MinWait, MaxWait);
}
