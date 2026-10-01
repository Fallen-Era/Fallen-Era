#include "Combat/AI/FEEnemyAIController.h"

#include "AbilitySystem/FallenEraGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Combat/AI/FEAITargetCoordinatorSubsystem.h"
#include "Combat/Character/FEEnemyCharacter.h"
#include "Combat/FECombatTeams.h"
#include "Combat/Component/FECombatComponent.h"
#include "Combat/Interface/FEDamageable.h"
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
	TargetMemories.Reset();
	CurrentTargetAcquiredTime = -1.0f;
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
	TargetMemories.Reset();
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
	FFE_AITargetMemory& Memory = FindOrAddTargetMemory(*TargetActor);
	Memory.LastKnownLocation = TargetActor->GetActorLocation();
	Memory.LastStimulusTime = GetWorld()->GetTimeSeconds();
	SelectCombatTarget(TargetActor);
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
	if (!GetPawn())
	{
		return;
	}

	const FAISenseID SightSenseId = UAISense::GetSenseID<UAISense_Sight>();
	const FAISenseID HearingSenseId = UAISense::GetSenseID<UAISense_Hearing>();
	const FAISenseID DamageSenseId = UAISense::GetSenseID<UAISense_Damage>();
	const bool bSightStimulus = Stimulus.Type == SightSenseId;
	const bool bHearingStimulus = Stimulus.Type == HearingSenseId;
	const bool bDamageStimulus = Stimulus.Type == DamageSenseId;

	if (bHearingStimulus)
	{
		if (Stimulus.WasSuccessfullySensed() && IsValidTarget(Actor))
		{
			SetAIActive(true);
			if (HasActiveSightStimulus(*Actor))
			{
				RecordSightStimulus(*Actor, Stimulus);
				EvaluateBestCombatTarget();
			}
			else if (!CurrentTarget.IsValid())
			{
				// Hearing remembers the reported position, not the moving source actor.
				SetInvestigationLocation(Stimulus.StimulusLocation);
			}
		}
		return;
	}

	if (!IsValidTarget(Actor))
	{
		if (CurrentTarget.Get() == Actor)
		{
			ClearTarget();
		}
		return;
	}

	if (Stimulus.WasSuccessfullySensed())
	{
		SetAIActive(true);
		if (bSightStimulus)
		{
			RecordSightStimulus(*Actor, Stimulus);
		}
		else if (bDamageStimulus)
		{
			RecordDamageStimulus(*Actor, Stimulus);
		}

		EvaluateBestCombatTarget(bDamageStimulus);
		if (CurrentTarget.Get() == Actor && HasActiveSightStimulus(*Actor))
		{
			bCurrentTargetVisible = true;
			bEncounterTargetPendingSight = false;
			TargetLostTime = -1.0f;
		}
	}
	else if (CurrentTarget.Get() == Actor &&
		bSightStimulus)
	{
		// Hearing and damage stimuli have short MaxAge values. Their expiration must not
		// discard a target that sight is still tracking.
		bCurrentTargetVisible = false;
		TargetLostTime = GetWorld()->GetTimeSeconds();
		StopMovement();
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
		StopMovement();
	}
	else if (TargetActor && !bEncounterTargetPendingSight && TargetLostTime < 0.0f)
	{
		// Perception MaxAge and combat memory are separate. Damage may stay in the
		// perception cache longer, but non-visible combat pursuit still times out here.
		TargetLostTime = GetWorld()->GetTimeSeconds();
	}
	if (TargetActor && (!IsValidTarget(TargetActor) ||
		(TargetLostTime >= 0.0f &&
			GetWorld()->GetTimeSeconds() - TargetLostTime > FMath::Max(0.0f, TargetForgetDelay))))
	{
		ClearTarget();
		TargetActor = nullptr;
	}

	EvaluateBestCombatTarget();
	TargetActor = CurrentTarget.Get();

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
	const float AttackRange = FMath::Max(0.0f, EnemyCharacter.GetAttackRange());
	const bool bCanTrackActor = HasActiveSightStimulus(TargetActor) || bEncounterTargetPendingSight;
	if (!bCanTrackActor)
	{
		const FVector LastKnownLocation = GetLastKnownTargetLocation(TargetActor);
		ClearFocus(EAIFocusPriority::Gameplay);
		SetFocalPoint(LastKnownLocation, EAIFocusPriority::Gameplay);
		if (GetMoveStatus() != EPathFollowingStatus::Moving)
		{
			MoveToLocation(
				LastKnownLocation,
				FMath::Max(50.0f, AttackRange * 0.8f),
				true,
				true,
				true,
				false);
		}
		return;
	}

	SetFocus(&TargetActor, EAIFocusPriority::Gameplay);
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

void AFE_EnemyAIController::EvaluateBestCombatTarget(bool bIgnoreMinimumLockTime)
{
	if (!GetPawn() || !GetWorld())
	{
		return;
	}

	RefreshVisibleTargetMemories();
	PruneTargetMemories();

	AActor* CurrentActor = CurrentTarget.Get();
	const FFE_AITargetMemory* CurrentMemory = CurrentActor ? FindTargetMemory(*CurrentActor) : nullptr;
	const float CurrentScore = CurrentActor && CurrentMemory && IsValidTarget(CurrentActor)
		? CalculateTargetScore(*CurrentActor, *CurrentMemory)
		: -TNumericLimits<float>::Max();

	AActor* BestActor = nullptr;
	float BestScore = -TNumericLimits<float>::Max();
	for (const FFE_AITargetMemory& Memory : TargetMemories)
	{
		AActor* Candidate = Memory.Actor.Get();
		if (!IsValidTarget(Candidate))
		{
			continue;
		}

		const float CandidateScore = CalculateTargetScore(*Candidate, Memory);
		if (CandidateScore > BestScore)
		{
			BestScore = CandidateScore;
			BestActor = Candidate;
		}
	}

	if (!BestActor)
	{
		if (CurrentActor && !IsValidTarget(CurrentActor))
		{
			ClearTarget();
		}
		return;
	}
	if (BestActor == CurrentActor)
	{
		return;
	}
	if (!CurrentActor || !CurrentMemory || !IsValidTarget(CurrentActor))
	{
		SelectCombatTarget(BestActor);
		return;
	}

	const FSAITargetSelectionSettings& Settings = GetTargetSelectionSettings();
	const float TimeSinceAcquired = GetWorld()->GetTimeSeconds() - CurrentTargetAcquiredTime;
	if (!bIgnoreMinimumLockTime &&
		TimeSinceAcquired < FMath::Max(0.0f, Settings.MinimumTargetLockTime))
	{
		return;
	}

	if (BestScore >= CurrentScore + FMath::Max(0.0f, Settings.SwitchScoreAdvantage))
	{
		SelectCombatTarget(BestActor);
	}
}

void AFE_EnemyAIController::SelectCombatTarget(AActor* TargetActor)
{
	if (!IsValidTarget(TargetActor) || CurrentTarget.Get() == TargetActor || !GetWorld())
	{
		return;
	}

	StopMovement();
	ClearInvestigation();
	if (UFE_AITargetCoordinatorSubsystem* Coordinator =
		GetWorld()->GetSubsystem<UFE_AITargetCoordinatorSubsystem>())
	{
		Coordinator->SetAssignedTarget(this, TargetActor);
	}

	CurrentTarget = TargetActor;
	CurrentTargetAcquiredTime = GetWorld()->GetTimeSeconds();
	bCurrentTargetVisible = HasActiveSightStimulus(*TargetActor);
	bEncounterTargetPendingSight = false;
	TargetLostTime = bCurrentTargetVisible ? -1.0f : GetWorld()->GetTimeSeconds();
}

FFE_AITargetMemory& AFE_EnemyAIController::FindOrAddTargetMemory(AActor& Actor)
{
	if (FFE_AITargetMemory* ExistingMemory = TargetMemories.FindByPredicate(
		[&Actor](const FFE_AITargetMemory& Memory)
		{
			return Memory.Actor.Get() == &Actor;
		}))
	{
		return *ExistingMemory;
	}

	FFE_AITargetMemory& NewMemory = TargetMemories.AddDefaulted_GetRef();
	NewMemory.Actor = &Actor;
	NewMemory.LastKnownLocation = Actor.GetActorLocation();
	return NewMemory;
}

const FFE_AITargetMemory* AFE_EnemyAIController::FindTargetMemory(const AActor& Actor) const
{
	return TargetMemories.FindByPredicate(
		[&Actor](const FFE_AITargetMemory& Memory)
		{
			return Memory.Actor.Get() == &Actor;
		});
}

void AFE_EnemyAIController::RecordSightStimulus(AActor& Actor, const FAIStimulus& Stimulus)
{
	(void)Stimulus;
	FFE_AITargetMemory& Memory = FindOrAddTargetMemory(Actor);
	const float CurrentTime = GetWorld()->GetTimeSeconds();
	Memory.LastKnownLocation = Actor.GetActorLocation();
	Memory.LastStimulusTime = CurrentTime;
	Memory.LastSeenTime = CurrentTime;
}

void AFE_EnemyAIController::RecordDamageStimulus(AActor& Actor, const FAIStimulus& Stimulus)
{
	FFE_AITargetMemory& Memory = FindOrAddTargetMemory(Actor);
	const float CurrentTime = GetWorld()->GetTimeSeconds();
	const float DecaySeconds = GetTargetSelectionSettings().DamageThreatDecaySeconds;
	if (Memory.LastDamageTime >= 0.0f && DecaySeconds > UE_SMALL_NUMBER)
	{
		Memory.AccumulatedDamage *= FMath::Exp(
			-(CurrentTime - Memory.LastDamageTime) / DecaySeconds);
	}
	else if (DecaySeconds <= UE_SMALL_NUMBER)
	{
		Memory.AccumulatedDamage = 0.0f;
	}

	Memory.AccumulatedDamage += FMath::Max(0.0f, Stimulus.Strength);
	Memory.LastDamageTime = CurrentTime;
	Memory.LastStimulusTime = CurrentTime;
	Memory.LastKnownLocation = Actor.GetActorLocation();
}

void AFE_EnemyAIController::RefreshVisibleTargetMemories()
{
	if (!EnemyPerceptionComponent || !GetWorld())
	{
		return;
	}

	TArray<AActor*> VisibleActors;
	EnemyPerceptionComponent->GetCurrentlyPerceivedActors(
		UAISense_Sight::StaticClass(), VisibleActors);
	const float CurrentTime = GetWorld()->GetTimeSeconds();
	for (AActor* VisibleActor : VisibleActors)
	{
		if (!IsValidTarget(VisibleActor))
		{
			continue;
		}
		FFE_AITargetMemory& Memory = FindOrAddTargetMemory(*VisibleActor);
		Memory.LastKnownLocation = VisibleActor->GetActorLocation();
		Memory.LastStimulusTime = CurrentTime;
		Memory.LastSeenTime = CurrentTime;
	}
}

void AFE_EnemyAIController::PruneTargetMemories()
{
	if (!GetWorld())
	{
		return;
	}

	const float CurrentTime = GetWorld()->GetTimeSeconds();
	const float MemoryDuration = FMath::Max(
		0.0f, GetTargetSelectionSettings().TargetMemoryDuration);
	TargetMemories.RemoveAll(
		[this, CurrentTime, MemoryDuration](const FFE_AITargetMemory& Memory)
		{
			AActor* Actor = Memory.Actor.Get();
			if (!IsValidTarget(Actor))
			{
				return true;
			}
			if (CurrentTarget.Get() == Actor || HasActiveSightStimulus(*Actor))
			{
				return false;
			}
			return Memory.LastStimulusTime < 0.0f ||
				CurrentTime - Memory.LastStimulusTime > MemoryDuration;
		});
}

float AFE_EnemyAIController::CalculateTargetScore(
	const AActor& Actor,
	const FFE_AITargetMemory& Memory) const
{
	const APawn* ControlledPawn = GetPawn();
	const UWorld* World = GetWorld();
	if (!ControlledPawn || !World)
	{
		return -TNumericLimits<float>::Max();
	}

	const FSAITargetSelectionSettings& Settings = GetTargetSelectionSettings();
	const float CurrentTime = World->GetTimeSeconds();
	const float EvaluationRadius = FMath::Max(1.0f, LoseSightRadius);
	const float Distance = FVector::Dist(ControlledPawn->GetActorLocation(), Actor.GetActorLocation());
	float Score = FMath::Max(0.0f, Settings.ProximityScore) *
		(1.0f - FMath::Clamp(Distance / EvaluationRadius, 0.0f, 1.0f));

	if (HasActiveSightStimulus(Actor))
	{
		Score += FMath::Max(0.0f, Settings.VisibleTargetScore);
	}
	else if (Memory.LastSeenTime >= 0.0f && TargetForgetDelay > UE_SMALL_NUMBER)
	{
		const float SeenRecency = 1.0f - FMath::Clamp(
			(CurrentTime - Memory.LastSeenTime) / TargetForgetDelay, 0.0f, 1.0f);
		Score += FMath::Max(0.0f, Settings.VisibleTargetScore) * 0.25f * SeenRecency;
	}

	if (Memory.LastDamageTime >= 0.0f && Settings.DamageThreatDecaySeconds > UE_SMALL_NUMBER)
	{
		const float DamageThreat = Memory.AccumulatedDamage * FMath::Exp(
			-(CurrentTime - Memory.LastDamageTime) / Settings.DamageThreatDecaySeconds);
		Score += DamageThreat * FMath::Max(0.0f, Settings.DamageScorePerPoint);
	}

	if (CurrentTarget.Get() == &Actor)
	{
		Score += FMath::Max(0.0f, Settings.CurrentTargetScore);
	}

	if (const UFE_AITargetCoordinatorSubsystem* Coordinator =
		World->GetSubsystem<UFE_AITargetCoordinatorSubsystem>())
	{
		int32 OtherEnemyCount = Coordinator->GetTargetLoad(&Actor);
		if (CurrentTarget.Get() == &Actor)
		{
			OtherEnemyCount = FMath::Max(0, OtherEnemyCount - 1);
		}
		Score -= OtherEnemyCount * FMath::Max(0.0f, Settings.TargetLoadPenalty);
	}

	const uint32 PreferenceHash = HashCombine(GetTypeHash(GetFName()), GetTypeHash(Actor.GetFName()));
	const float Preference = static_cast<float>(PreferenceHash & 0xffff) / 65535.0f;
	Score += Preference * FMath::Max(0.0f, Settings.PreferenceVariance);
	return Score;
}

const FSAITargetSelectionSettings& AFE_EnemyAIController::GetTargetSelectionSettings() const
{
	if (const AFE_EnemyCharacter* EnemyCharacter = Cast<AFE_EnemyCharacter>(GetPawn()))
	{
		return EnemyCharacter->GetTargetSelectionSettings();
	}
	static const FSAITargetSelectionSettings DefaultSettings;
	return DefaultSettings;
}

FVector AFE_EnemyAIController::GetLastKnownTargetLocation(const AActor& Actor) const
{
	if (const FFE_AITargetMemory* Memory = FindTargetMemory(Actor);
		Memory && FAISystem::IsValidLocation(Memory->LastKnownLocation))
	{
		return Memory->LastKnownLocation;
	}
	return Actor.GetActorLocation();
}

void AFE_EnemyAIController::ClearTarget()
{
	StopMovement();
	AActor* PreviousTarget = CurrentTarget.Get();
	if (UFE_AITargetCoordinatorSubsystem* Coordinator =
		GetWorld() ? GetWorld()->GetSubsystem<UFE_AITargetCoordinatorSubsystem>() : nullptr)
	{
		Coordinator->ClearAssignedTarget(this);
	}
	CurrentTarget.Reset();
	CurrentTargetAcquiredTime = -1.0f;
	if (PreviousTarget)
	{
		TargetMemories.RemoveAll(
			[PreviousTarget](const FFE_AITargetMemory& Memory)
			{
				return Memory.Actor.Get() == PreviousTarget;
			});
	}
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
	if (UFE_AITargetCoordinatorSubsystem* Coordinator =
		GetWorld() ? GetWorld()->GetSubsystem<UFE_AITargetCoordinatorSubsystem>() : nullptr)
	{
		Coordinator->ClearAssignedTarget(this);
	}
	CurrentTarget.Reset();
	CurrentTargetAcquiredTime = -1.0f;
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
