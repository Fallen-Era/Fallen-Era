#include "HT/Ability/EnemyMeleeAttackAbility.h"

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "HT/Ability/Tasks/MeleeTraceTask.h"
#include "HT/Character/EnemyCharacter.h"
#include "HT/Collision/FECollisionChannels.h"
#include "HT/Combat/FECombatTeams.h"
#include "HT/Component/CombatComponent.h"

UFE_EnemyMeleeAttackAbility::UFE_EnemyMeleeAttackAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	ActivationPolicy = EFallenEraAbilityActivationPolicy::OnInputTriggered;
}

bool UFE_EnemyMeleeAttackAbility::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags) ||
		!ActorInfo || !ActorInfo->IsNetAuthority())
	{
		return false;
	}

	const AFE_EnemyCharacter* EnemyCharacter = Cast<AFE_EnemyCharacter>(ActorInfo->AvatarActor.Get());
	const AActor* TargetActor = EnemyCharacter ? EnemyCharacter->GetPendingAttackTarget() : nullptr;
	const UWorld* World = EnemyCharacter ? EnemyCharacter->GetWorld() : nullptr;
	if (!EnemyCharacter || !IsValid(TargetActor) || !World || EnemyCharacter->GetAttackMontageSettings().IsEmpty() ||
		FECombatTeams::AreSameTeam(EnemyCharacter, TargetActor) ||
		World->GetTimeSeconds() < NextActivationAllowedTime)
	{
		return false;
	}

	return FVector::DistSquared(EnemyCharacter->GetActorLocation(), TargetActor->GetActorLocation()) <=
		FMath::Square(FMath::Max(0.0f, EnemyCharacter->GetAttackRange()));
}

void UFE_EnemyMeleeAttackAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	CachedEnemyCharacter = Cast<AFE_EnemyCharacter>(GetAvatarActorFromActorInfo());
	const FSAIAttackMontageSettings* SelectedAttack = CachedEnemyCharacter
		? SelectAttackSettings(*CachedEnemyCharacter)
		: nullptr;
	if (SelectedAttack)
	{
		ActiveAttackSettings = *SelectedAttack;
		ActiveMontage = ActiveAttackSettings.AttackMontage;
	}
	if (!CachedEnemyCharacter || !ActiveMontage || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		FinishAttack(true);
		return;
	}

	DamagedActors.Reset();
	NextActivationAllowedTime = CachedEnemyCharacter->GetWorld()->GetTimeSeconds() +
		FMath::Max(0.01f, CachedEnemyCharacter->GetAttackCooldown());

	const FGameplayTag StartTag = ActiveAttackSettings.TraceStartEventTag;
	if (StartTag.IsValid())
	{
		TraceStartTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, StartTag, nullptr, true, true);
		TraceStartTask->EventReceived.AddDynamic(this, &UFE_EnemyMeleeAttackAbility::HandleTraceStart);
		TraceStartTask->ReadyForActivation();
	}
	else
	{
		StartTrace();
	}

	const FGameplayTag EndTag = ActiveAttackSettings.TraceEndEventTag;
	if (EndTag.IsValid() && EndTag != StartTag)
	{
		TraceEndTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, EndTag, nullptr, true, true);
		TraceEndTask->EventReceived.AddDynamic(this, &UFE_EnemyMeleeAttackAbility::HandleTraceEnd);
		TraceEndTask->ReadyForActivation();
	}

	const FGameplayTag ResetTag = ActiveAttackSettings.AttackResetEventTag;
	if (ResetTag.IsValid() && ResetTag != StartTag && ResetTag != EndTag)
	{
		AttackResetTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, ResetTag, nullptr, true, true);
		AttackResetTask->EventReceived.AddDynamic(this, &UFE_EnemyMeleeAttackAbility::HandleAttackReset);
		AttackResetTask->ReadyForActivation();
	}

	AttackTimeoutTask = UAbilityTask_WaitDelay::WaitDelay(
		this,
		FMath::Max(0.1f, ActiveMontage->GetPlayLength() + 0.1f));
	AttackTimeoutTask->OnFinish.AddDynamic(this, &UFE_EnemyMeleeAttackAbility::HandleAttackTimeout);
	AttackTimeoutTask->ReadyForActivation();

	// Register event listeners before montage playback so a notify near time zero cannot be missed.
	if (UFE_CombatComponent* CombatComponent = CachedEnemyCharacter->GetCombatComponent())
	{
		CombatComponent->PlayAttackMontage(ActiveMontage, false);
	}
}

void UFE_EnemyMeleeAttackAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	StopTrace();
	if (CachedEnemyCharacter)
	{
		CachedEnemyCharacter->ClearPendingAttackTarget();
	}
	DamagedActors.Reset();
	CachedEnemyCharacter = nullptr;
	ActiveMontage = nullptr;
	ActiveAttackSettings = FSAIAttackMontageSettings();
	TraceStartTask = nullptr;
	TraceEndTask = nullptr;
	AttackResetTask = nullptr;
	AttackTimeoutTask = nullptr;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UFE_EnemyMeleeAttackAbility::HandleTraceStart(FGameplayEventData Payload)
{
	(void)Payload;
	StartTrace();
}

void UFE_EnemyMeleeAttackAbility::HandleTraceEnd(FGameplayEventData Payload)
{
	StopTrace();
	if (ActiveAttackSettings.AttackResetEventTag == Payload.EventTag)
	{
		FinishAttack();
	}
}

void UFE_EnemyMeleeAttackAbility::HandleAttackReset(FGameplayEventData Payload)
{
	(void)Payload;
	FinishAttack();
}

void UFE_EnemyMeleeAttackAbility::HandleAttackTimeout()
{
	FinishAttack();
}

void UFE_EnemyMeleeAttackAbility::StartTrace()
{
	if (bTraceActive || !CachedEnemyCharacter)
	{
		return;
	}
	DamagedActors.Reset();
	bTraceActive = true;
	TraceTickTask = UFE_MeleeTraceTask::StartMeleeTrace(this);
	TraceTickTask->OnTraceTick.AddDynamic(this, &UFE_EnemyMeleeAttackAbility::ExecuteTrace);
	TraceTickTask->ReadyForActivation();
}

void UFE_EnemyMeleeAttackAbility::StopTrace()
{
	bTraceActive = false;
	if (TraceTickTask)
	{
		TraceTickTask->EndTask();
		TraceTickTask = nullptr;
	}
}

void UFE_EnemyMeleeAttackAbility::ExecuteTrace()
{
	if (!bTraceActive || !CachedEnemyCharacter || !CachedEnemyCharacter->HasAuthority())
	{
		return;
	}

	USkeletalMeshComponent* CharacterMesh = CachedEnemyCharacter->GetMesh();
	UWorld* World = CachedEnemyCharacter->GetWorld();
	UFE_CombatComponent* CombatComponent = CachedEnemyCharacter->GetCombatComponent();
	if (!CharacterMesh || !World || !CombatComponent)
	{
		return;
	}

	const FName StartSocket = ActiveAttackSettings.TraceStartSocket;
	const FName EndSocket = ActiveAttackSettings.TraceEndSocket;
	const FVector TraceStart = CharacterMesh->DoesSocketExist(StartSocket)
		? CharacterMesh->GetSocketLocation(StartSocket)
		: CachedEnemyCharacter->GetActorLocation();
	const FVector TraceEnd = CharacterMesh->DoesSocketExist(EndSocket)
		? CharacterMesh->GetSocketLocation(EndSocket)
		: TraceStart + CachedEnemyCharacter->GetActorForwardVector() * CachedEnemyCharacter->GetAttackRange();

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(FE_EnemyMeleeAbility), false, CachedEnemyCharacter);
	QueryParams.bReturnPhysicalMaterial = true;
	TArray<FHitResult> Hits;
	if (!World->SweepMultiByChannel(
		Hits,
		TraceStart,
		TraceEnd,
		FQuat::Identity,
		FECollisionChannels::EnemyTrace,
		FCollisionShape::MakeSphere(FMath::Max(0.0f, ActiveAttackSettings.TraceRadius)),
		QueryParams))
	{
		return;
	}

	for (const FHitResult& Hit : Hits)
	{
		AActor* TargetActor = Hit.GetActor();
		if (!TargetActor || DamagedActors.Contains(TargetActor) ||
			FECombatTeams::AreSameTeam(CachedEnemyCharacter, TargetActor))
		{
			continue;
		}

		const TSubclassOf<UGameplayEffect> DamageEffect = CachedEnemyCharacter->GetCachedAttackDamageEffect();
		const bool bApplied = DamageEffect
			? CombatComponent->ApplyDamageWithEffectFromHit(TargetActor, DamageEffect, Hit, nullptr)
			: CombatComponent->ApplyDamageFromHit(TargetActor, Hit, nullptr);
		if (!bApplied)
		{
			continue;
		}

		DamagedActors.Add(TargetActor);
		if (DamagedActors.Num() >= FMath::Max(1, ActiveAttackSettings.MaxHitTargets))
		{
			break;
		}
	}
}

void UFE_EnemyMeleeAttackAbility::FinishAttack(bool bWasCancelled)
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled);
	}
}

const FSAIAttackMontageSettings* UFE_EnemyMeleeAttackAbility::SelectAttackSettings(
	const AFE_EnemyCharacter& EnemyCharacter) const
{
	const TArray<FSAIAttackMontageSettings>& AttackSettings =
		EnemyCharacter.GetAttackMontageSettings();
	if (AttackSettings.IsEmpty())
	{
		return nullptr;
	}

	const int32 StartIndex = FMath::RandHelper(AttackSettings.Num());
	for (int32 Offset = 0; Offset < AttackSettings.Num(); ++Offset)
	{
		const FSAIAttackMontageSettings& Candidate =
			AttackSettings[(StartIndex + Offset) % AttackSettings.Num()];
		if (Candidate.AttackMontage)
		{
			return &Candidate;
		}
	}
	return nullptr;
}
