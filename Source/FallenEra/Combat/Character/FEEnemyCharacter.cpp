#include "Combat/Character/FEEnemyCharacter.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"

#include "AbilitySystem/FallenEraAbilitySet.h"
#include "AbilitySystem/FallenEraAbilitySystemComponent.h"
#include "AbilitySystem/FallenEraGameplayTags.h"
#include "AbilitySystem/Attributes/FallenEraAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Combat/Collision/FECollisionChannels.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Combat/Ability/FEEnemyMeleeAttackAbility.h"
#include "Combat/AI/FEAISettingsDataAsset.h"
#include "Combat/AI/FEEnemyAIController.h"
#include "Combat/FECombatTeams.h"
#include "Combat/Component/FECombatComponent.h"
#include "Combat/Component/FEProceduralHitReactionComponent.h"
#include "Combat/Component/FEKnockdownComponent.h"
#include "Combat/Component/FEEquipmentComponent.h"
#include "Combat/GameplayEffect/FEDamageGameplayEffect.h"
#include "Combat/Interface/FEDamageable.h"
#include "NavigationInvokerComponent.h"
#include "Net/UnrealNetwork.h"

AFE_EnemyCharacter::AFE_EnemyCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
	AIControllerClass = AFE_EnemyAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	AbilitySystemComponent = CreateDefaultSubobject<UFallenEraAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	// AI pawns have no autonomous owning client. Minimal avoids replicating every active GE
	// to every simulated client while still replicating required tags and GameplayCues.
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	AttributeSet = CreateDefaultSubobject<UFallenEraAttributeSet>(TEXT("AttributeSet"));
	CombatComponent = CreateDefaultSubobject<UFE_CombatComponent>(TEXT("CombatComponent"));
	CombatComponent->DeferCombatConfiguration();
	PhysicalHitReactionComponent = CreateDefaultSubobject<UFE_ProceduralHitReactionComponent>(TEXT("PhysicalHitReaction"));
	KnockdownComponent = CreateDefaultSubobject<UFE_KnockdownComponent>(TEXT("Knockdown"));
	EquipmentComponent = CreateDefaultSubobject<UFE_EquipmentComponent>(TEXT("EquipmentComponent"));
	EquipmentComponent->SetInitialArmorInitializationDeferred(true);
	NavigationInvokerComponent = CreateDefaultSubobject<UNavigationInvokerComponent>(TEXT("NavigationInvokerComponent"));
	NavigationInvokerComponent->SetGenerationRadii(3000.0f, 5000.0f);

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionProfileName(TEXT("Enemy"));
	}
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->bOrientRotationToMovement = true;
	}
	bReplicates = true;
}

FGenericTeamId AFE_EnemyCharacter::GetGenericTeamId() const
{
	return FECombatTeams::Enemy;
}

TArray<FFE_ConditionApplicationChance> AFE_EnemyCharacter::GetConditionApplicationChances_Implementation() const
{
	return CachedAISettings.ConditionApplicationChances;
}

UAbilitySystemComponent* AFE_EnemyCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

FFE_CombatDamageResult AFE_EnemyCharacter::ReceiveCombatDamage_Implementation(
	const FFE_CombatDamageRequest& DamageRequest)
{
	return CombatComponent
		? CombatComponent->ApplyGameplayEffectDamage(DamageRequest)
		: FFE_CombatDamageResult();
}

void AFE_EnemyCharacter::BeginPlay()
{
	Super::BeginPlay();
	ApplyManagedVisualCullDistance();
	SelectAISettingsIndexAtBeginPlay();
	CacheSelectedAISettings();
	if (USkeletalMeshComponent* CharacterMesh = GetMesh())
	{
		if (GetNetMode() == NM_DedicatedServer)
		{
			// Dedicated servers only need montage notifies and current attack socket transforms.
			CharacterMesh->VisibilityBasedAnimTickOption =
				EVisibilityBasedAnimTickOption::OnlyTickMontagesAndRefreshBonesWhenPlayingMontages;
		}
		else if (HasAuthority())
		{
			// Socket-based server hits require bone refresh even outside the host's viewport.
			CharacterMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickMontagesAndRefreshBonesWhenPlayingMontages;
		}
	}
}

void AFE_EnemyCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AFE_EnemyCharacter, SelectedAISettingsIndex);
	DOREPLIFETIME(AFE_EnemyCharacter, ManagedVisualCullDistance);
}

void AFE_EnemyCharacter::SelectAISettingsIndexAtBeginPlay()
{
	if (!HasAuthority() || !bRandomizeAISettings)
	{
		return;
	}

	TArray<int32, TInlineAllocator<8>> ValidIndices;
	for (int32 Index = 0; Index < AISettingsDataAssets.Num(); ++Index)
	{
		if (!AISettingsDataAssets[Index].IsNull())
		{
			ValidIndices.Add(Index);
		}
	}

	if (!ValidIndices.IsEmpty())
	{
		SelectedAISettingsIndex = ValidIndices[FMath::RandHelper(ValidIndices.Num())];
		ForceNetUpdate();
	}
}

void AFE_EnemyCharacter::OnRep_SelectedAISettingsIndex()
{
	CacheSelectedAISettings();
}

void AFE_EnemyCharacter::OnRep_ManagedVisualCullDistance()
{
	ApplyManagedVisualCullDistance();
}

void AFE_EnemyCharacter::CacheSelectedAISettings()
{
	if (AISettingsLoadHandle) { AISettingsLoadHandle->CancelHandle(); AISettingsLoadHandle.Reset(); }
	if (AISettingsEffectLoadHandle) { AISettingsEffectLoadHandle->CancelHandle(); AISettingsEffectLoadHandle.Reset(); }
	bAISettingsCached = false;
	CachedAISettings = FSAISettings();
	CachedAttackDamageEffect = nullptr;
	CachedAttackAbilityClass = nullptr;
	CachedAISettingsDataAsset = nullptr;

	if (AISettingsDataAssets.IsValidIndex(SelectedAISettingsIndex) &&
		!AISettingsDataAssets[SelectedAISettingsIndex].IsNull())
	{
		const auto& Asset = AISettingsDataAssets[SelectedAISettingsIndex];
		if (!Asset.IsValid())
		{
			AISettingsLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
				Asset.ToSoftObjectPath(), FStreamableDelegate::CreateUObject(this, &ThisClass::CacheLoadedAISettings));
			return;
		}
	}
	CacheLoadedAISettings();
}

void AFE_EnemyCharacter::CacheLoadedAISettings()
{
	CachedAISettingsDataAsset = AISettingsDataAssets.IsValidIndex(SelectedAISettingsIndex)
		? AISettingsDataAssets[SelectedAISettingsIndex].Get() : nullptr;
	if (CachedAISettingsDataAsset)
	{
		CachedAISettings = CachedAISettingsDataAsset->AISettings;
	}
	else
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("%s has no valid AI settings at index %d."),
			*GetName(),
			SelectedAISettingsIndex);
		return;
	}

	if (CombatComponent)
	{
		CombatComponent->SetHitReactionMontages(CachedAISettings.HitReactionMontages);
	}
	TArray<FSoftObjectPath> DependencyPaths;
	CachedAISettings.GatherAssetPaths(DependencyPaths, HasAuthority());
	if (!CachedAISettings.AreAssetsLoaded(HasAuthority()))
	{
		AISettingsEffectLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
			DependencyPaths,
			FStreamableDelegate::CreateUObject(this, &ThisClass::FinishAISettingsInitialization));
		return;
	}
	FinishAISettingsInitialization();
}

void AFE_EnemyCharacter::FinishAISettingsInitialization()
{
	if (!CachedAISettingsDataAsset ||
		!CachedAISettings.AreAssetsLoaded(HasAuthority()))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s could not load its AI preset dependencies. AI initialization is deferred."), *GetName());
		return;
	}
	CachedAttackDamageEffect = CachedAISettings.DamageEffect.IsNull()
		? UFE_DamageGameplayEffect::StaticClass() : CachedAISettings.DamageEffect.Get();
	CachedAttackAbilityClass = CachedAISettings.AttackAbilityClass.Get();
	CombatComponent->SetPreloadedCombatConfiguration(
		CachedAISettings.DamageEffect, CachedAISettings.DeathMontage,
		CachedAISettings.HitZoneMappingData, CachedAISettings.HitZoneMultiplierData);
	PhysicalHitReactionComponent->Configure(CachedAISettings.PhysicalHitReaction);
	KnockdownComponent->Configure(CachedAISettings.LegKnockdown);
	if (HasAuthority())
	{
		EquipmentComponent->DefaultArmorDataAssets = CachedAISettings.DefaultArmorDataAssets;
	}
	// GAS and equipment must be ready before the decision timer can start an attack.
	InitializeEnemyAbilitySystem();
	EquipmentComponent->SetInitialArmorInitializationDeferred(false);
	EquipmentComponent->RefreshEquipment();
	bAISettingsCached = true;
}

void AFE_EnemyCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AISettingsLoadHandle) { AISettingsLoadHandle->CancelHandle(); AISettingsLoadHandle.Reset(); }
	if (AISettingsEffectLoadHandle) { AISettingsEffectLoadHandle->CancelHandle(); AISettingsEffectLoadHandle.Reset(); }
	Super::EndPlay(EndPlayReason);
}

bool AFE_EnemyCharacter::GatherAISettingsAssetPaths(TArray<FSoftObjectPath>& OutPaths, bool bGameplayDependencies) const
{
	bool bHasPreset = false;
	for (int32 Index = 0; Index < AISettingsDataAssets.Num(); ++Index)
	{
		if (!bRandomizeAISettings && Index != SelectedAISettingsIndex) { continue; }
		const auto& Preset = AISettingsDataAssets[Index];
		if (Preset.IsNull()) { continue; }
		bHasPreset = true;
		if (!bGameplayDependencies) { OutPaths.AddUnique(Preset.ToSoftObjectPath()); }
		else
		{
			const UFE_AISettingsDataAsset* Data = Preset.Get();
			if (!Data) { return false; }
			Data->AISettings.GatherAssetPaths(OutPaths, true);
		}
	}
	return bHasPreset;
}

void AFE_EnemyCharacter::InitializeEnemyAbilitySystem()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	AbilitySystemComponent->InitAbilityActorInfo(this, this);

	if (HasAuthority() && !AbilitySystemComponent->HasMatchingGameplayTag(FallenEraGameplayTags::State_AbilitySystem_Initialized))
	{
		TArray<UFallenEraAbilitySet*> StartupAbilitySets;
		for (const auto& AbilitySet : CachedAISettings.DefaultAbilitySets)
		{
			StartupAbilitySets.AddUnique(AbilitySet.Get());
		}
		for (const UFallenEraAbilitySet* AbilitySet : StartupAbilitySets)
		{
			if (AbilitySet)
			{
				AbilitySet->GiveToAbilitySystem(AbilitySystemComponent, this);
			}
		}

		for (const FFE_AIStartupEffect& Effect : CachedAISettings.StartupEffects)
		{
			if (Effect.GameplayEffect.IsNull()) { continue; }
			FGameplayEffectContextHandle Context = AbilitySystemComponent->MakeEffectContext();
			Context.AddSourceObject(CachedAISettingsDataAsset);
			const FGameplayEffectSpecHandle Spec = AbilitySystemComponent->MakeOutgoingSpec(
				Effect.GameplayEffect.Get(), FMath::Max(0.0f, Effect.EffectLevel), Context);
			if (Spec.IsValid()) { AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()); }
		}

		if (CachedAttackAbilityClass && !AbilitySystemComponent->FindAbilitySpecFromClass(CachedAttackAbilityClass))
		{
			AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(CachedAttackAbilityClass, 1, INDEX_NONE, this));
		}

		AbilitySystemComponent->AddLooseGameplayTag(
			FallenEraGameplayTags::State_AbilitySystem_Initialized,
			1,
			EGameplayTagReplicationState::TagAndCountToAll);
		AbilitySystemComponent->TryActivateAbilitiesOnSpawn();
	}
}

float AFE_EnemyCharacter::GetHealth() const
{
	return AttributeSet ? AttributeSet->GetHealth() : 0.0f;
}

float AFE_EnemyCharacter::GetMaxHealth() const
{
	return AttributeSet ? AttributeSet->GetMaxHealth() : 0.0f;
}

bool AFE_EnemyCharacter::IsDead() const
{
	return AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(FallenEraGameplayTags::State_Dead);
}

bool AFE_EnemyCharacter::IsTargetWithinAttackRange(const AActor* TargetActor) const
{
	if (!IsValid(TargetActor)) { return false; }
	const float Range = FMath::Max(0.0f, CachedAISettings.AttackRange);
	const FVector Origin = GetActorLocation();
	if (TargetActor->IsA<APawn>())
	{
		return FVector::DistSquared(Origin, TargetActor->GetActorLocation()) <= FMath::Square(Range);
	}
	TInlineComponentArray<UPrimitiveComponent*> Components(TargetActor);
	for (UPrimitiveComponent* Component : Components)
	{
		if (!Component->IsQueryCollisionEnabled() || Component->GetCollisionResponseToChannel(FECollisionChannels::EnemyTrace) != ECR_Block ||
			Component->Bounds.GetBox().ComputeSquaredDistanceToPoint(Origin) > FMath::Square(Range)) { continue; }
		FVector SurfacePoint;
		const float Distance = Component->GetClosestPointOnCollision(Origin, SurfacePoint);
		if (Distance >= 0.0f && Distance <= Range) { return true; }
		if (Distance < 0.0f)
		{
			// Complex-only geometry may not support closest-point queries. Require a real surface hit, never just its bounds.
			FHitResult Hit;
			FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(FE_AIObstacleReach), true, this);
			if (Component->LineTraceComponent(Hit, Origin, Component->Bounds.Origin, QueryParams) && Hit.Distance <= Range) { return true; }
		}
	}
	return false;
}

bool AFE_EnemyCharacter::TryStartAttack(AActor* TargetActor)
{
	if (!HasAuthority() || !AbilitySystemComponent || !CachedAttackAbilityClass || !IsValid(TargetActor) ||
		!bAISettingsCached || !UFE_CombatComponent::CanActorAttack(this) || IsAttackInProgress() || FECombatTeams::AreSameTeam(this, TargetActor) ||
		CachedAISettings.AttackMontages.IsEmpty())
	{
		return false;
	}
	if (!IsTargetWithinAttackRange(TargetActor))
	{
		return false;
	}

	PendingAttackTarget = TargetActor;
	const bool bActivated = AbilitySystemComponent->TryActivateAbilityByClass(CachedAttackAbilityClass, false);
	if (!bActivated)
	{
		PendingAttackTarget.Reset();
	}
	return bActivated;
}

bool AFE_EnemyCharacter::IsAttackInProgress() const
{
	if (!AbilitySystemComponent || !CachedAttackAbilityClass)
	{
		return false;
	}
	const FGameplayAbilitySpec* AttackSpec = AbilitySystemComponent->FindAbilitySpecFromClass(CachedAttackAbilityClass);
	return AttackSpec && AttackSpec->IsActive();
}

void AFE_EnemyCharacter::CancelActiveAttack()
{
	if (!HasAuthority() || !AbilitySystemComponent || !CachedAttackAbilityClass)
	{
		return;
	}
	if (const FGameplayAbilitySpec* AttackSpec = AbilitySystemComponent->FindAbilitySpecFromClass(CachedAttackAbilityClass);
		AttackSpec && AttackSpec->IsActive())
	{
		AbilitySystemComponent->CancelAbilityHandle(AttackSpec->Handle);
	}
	PendingAttackTarget.Reset();
}

void AFE_EnemyCharacter::ClearPendingAttackTarget()
{
	PendingAttackTarget.Reset();
}

void AFE_EnemyCharacter::ConfigureSpawnManagement(
	float VisualCullDistance,
	float NetCullDistance)
{
	if (!HasAuthority())
	{
		return;
	}
	bManagedBySpawnSubsystem = true;
	ManagedVisualCullDistance = FMath::Max(0.0f, VisualCullDistance);
	SetNetCullDistanceSquared(FMath::Square(FMath::Max(0.0f, NetCullDistance)));
	ApplyManagedVisualCullDistance();
	ForceNetUpdate();
}

void AFE_EnemyCharacter::SetManagedSimulationActive(bool bActive)
{
	if (!HasAuthority() || !bManagedBySpawnSubsystem)
	{
		return;
	}
	bManagedSimulationActive = bActive;
	if (AFE_EnemyAIController* EnemyController = Cast<AFE_EnemyAIController>(GetController()))
	{
		EnemyController->SetManagedSimulationActive(bActive);
	}
}

void AFE_EnemyCharacter::ApplyManagedVisualCullDistance()
{
	if (USkeletalMeshComponent* CharacterMesh = GetMesh())
	{
		CharacterMesh->SetCullDistance(ManagedVisualCullDistance);
	}
}

void AFE_EnemyCharacter::SetNavigationInvokerActive(bool bActive)
{
	if (!NavigationInvokerComponent)
	{
		return;
	}
	if (bActive)
	{
		NavigationInvokerComponent->Activate(false);
	}
	else
	{
		NavigationInvokerComponent->Deactivate();
	}
}
