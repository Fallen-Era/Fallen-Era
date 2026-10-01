#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "EnemyAIController.generated.h"

class UAIPerceptionComponent;
class UAISenseConfig_Damage;
class UAISenseConfig_Hearing;
class UAISenseConfig_Sight;
class AFE_EnemyCharacter;
struct FSAITargetSelectionSettings;

struct FFE_AITargetMemory
{
	TWeakObjectPtr<AActor> Actor;
	FVector LastKnownLocation = FVector::ZeroVector;
	float LastStimulusTime = -1.0f;
	float LastSeenTime = -1.0f;
	float LastDamageTime = -1.0f;
	float AccumulatedDamage = 0.0f;
};

/** Event-driven sight with a staggered low-frequency decision loop for open-world enemies. */
UCLASS()
class FALLENERA_API AFE_EnemyAIController : public AAIController
{
	GENERATED_BODY()

public:
	AFE_EnemyAIController();

	virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;
	virtual void GetActorEyesViewPoint(FVector& OutLocation, FRotator& OutRotation) const override;

	/** Central spawn management calls this instead of making every controller scan all players. */
	void SetManagedSimulationActive(bool bActive);

	/** Encounter spawns chase this target immediately, before perception has produced a stimulus. */
	bool SetEncounterCombatTarget(AActor* TargetActor);

	UFUNCTION(BlueprintPure, Category="FallenEra|AI|Target")
	AActor* GetCurrentCombatTarget() const { return CurrentTarget.Get(); }

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;

private:
	UFUNCTION()
	void HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	void UpdateDecision();
	void UpdatePatrol(AFE_EnemyCharacter& EnemyCharacter);
	void UpdateInvestigation(AFE_EnemyCharacter& EnemyCharacter);
	void UpdateCombat(AFE_EnemyCharacter& EnemyCharacter, AActor& TargetActor);
	void EvaluateBestCombatTarget(bool bIgnoreMinimumLockTime = false);
	void SelectCombatTarget(AActor* TargetActor);
	void ClearTarget();
	void SetInvestigationLocation(const FVector& Location);
	void ClearInvestigation();
	bool IsValidTarget(const AActor* Actor) const;
	bool IsPlayerWithinActivationDistance() const;
	void SetAIActive(bool bNewActive);
	void SetDecisionTimerRate(float Interval, float InitialDelay = -1.0f);
	void ScheduleNextPatrolRequest();
	void ApplyPerceptionSettings();
	bool IsPlayerTarget(const AActor* Actor) const;
	bool HasActiveSightStimulus(const AActor& Actor) const;
	FFE_AITargetMemory& FindOrAddTargetMemory(AActor& Actor);
	const FFE_AITargetMemory* FindTargetMemory(const AActor& Actor) const;
	void RecordSightStimulus(AActor& Actor, const FAIStimulus& Stimulus);
	void RecordDamageStimulus(AActor& Actor, const FAIStimulus& Stimulus);
	void RefreshVisibleTargetMemories();
	void PruneTargetMemories();
	float CalculateTargetScore(const AActor& Actor, const FFE_AITargetMemory& Memory) const;
	const FSAITargetSelectionSettings& GetTargetSelectionSettings() const;
	FVector GetLastKnownTargetLocation(const AActor& Actor) const;
	void SetMovementSpeed(AFE_EnemyCharacter& EnemyCharacter, float MovementSpeed) const;

	UPROPERTY(VisibleAnywhere, Category="FallenEra|AI")
	TObjectPtr<UAIPerceptionComponent> EnemyPerceptionComponent;

	UPROPERTY(VisibleAnywhere, Category="FallenEra|AI")
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	UPROPERTY(VisibleAnywhere, Category="FallenEra|AI")
	TObjectPtr<UAISenseConfig_Hearing> HearingConfig;

	UPROPERTY(VisibleAnywhere, Category="FallenEra|AI")
	TObjectPtr<UAISenseConfig_Damage> DamageConfig;

	UPROPERTY(EditDefaultsOnly, Category="FallenEra|AI|Perception|Sight", meta=(ClampMin="0.0"))
	float SightRadius = 2500.0f;

	UPROPERTY(EditDefaultsOnly, Category="FallenEra|AI|Perception|Sight", meta=(ClampMin="0.0"))
	float LoseSightRadius = 3000.0f;

	UPROPERTY(EditDefaultsOnly, Category="FallenEra|AI|Perception|Sight", meta=(ClampMin="0.0", ClampMax="180.0"))
	float PeripheralVisionAngleDegrees = 70.0f;

	UPROPERTY(EditDefaultsOnly, Category="FallenEra|AI|Perception|Sight", meta=(ClampMin="0.0"))
	float SightMaxAge = 2.0f;

	UPROPERTY(EditDefaultsOnly, Category="FallenEra|AI|Perception|Hearing", meta=(ClampMin="0.0"))
	float HearingRange = 3500.0f;

	UPROPERTY(EditDefaultsOnly, Category="FallenEra|AI|Perception|Hearing", meta=(ClampMin="0.0"))
	float HearingMaxAge = 1000.f;

	UPROPERTY(EditDefaultsOnly, Category="FallenEra|AI|Perception|Damage", meta=(ClampMin="0.0"))
	float DamageMaxAge = 1000.f;

	/** Decision frequency is intentionally lower than frame rate and staggered per controller. */
	UPROPERTY(EditDefaultsOnly, Category="FallenEra|AI|Performance", meta=(ClampMin="0.1"))
	float DecisionInterval = 0.25f;

	UPROPERTY(EditDefaultsOnly, Category="FallenEra|AI|Performance", meta=(ClampMin="0.1"))
	float DormantDecisionInterval = 2.0f;

	/** Perception, path following, and navigation invocation sleep outside this player distance. */
	UPROPERTY(EditDefaultsOnly, Category="FallenEra|AI|Performance", meta=(ClampMin="0.0"))
	float ActivationDistance = 12000.0f;

	UPROPERTY(EditDefaultsOnly, Category="FallenEra|AI|Perception", meta=(ClampMin="0.0"))
	float TargetForgetDelay = 2.0f;

	UPROPERTY(EditDefaultsOnly, Category="FallenEra|AI|Patrol", meta=(ClampMin="0.0"))
	float PatrolRadius = 1200.0f;

	UPROPERTY(EditDefaultsOnly, Category="FallenEra|AI|Patrol", meta=(ClampMin="0.0"))
	float PatrolAcceptanceRadius = 80.0f;

	UPROPERTY(EditDefaultsOnly, Category="FallenEra|AI|Patrol", meta=(ClampMin="0.0"))
	FVector2D PatrolWaitRange = FVector2D(1.0f, 3.0f);

	UPROPERTY(EditDefaultsOnly, Category="FallenEra|AI|Perception|Hearing", meta=(ClampMin="0.0"))
	float InvestigationAcceptanceRadius = 100.0f;

	TWeakObjectPtr<AActor> CurrentTarget;
	TArray<FFE_AITargetMemory> TargetMemories;
	FVector HomeLocation = FVector::ZeroVector;
	FVector InvestigationLocation = FVector::ZeroVector;
	float TargetLostTime = -1.0f;
	float NextPatrolRequestTime = 0.0f;
	float CurrentTargetAcquiredTime = -1.0f;
	FAIRequestID InvestigationMoveRequestId = FAIRequestID::InvalidRequest;
	FTimerHandle DecisionTimerHandle;
	bool bAIActive = true;
	bool bUsesManagedSimulation = false;
	bool bCurrentTargetVisible = false;
	bool bEncounterTargetPendingSight = false;
	bool bHasInvestigationLocation = false;
};
