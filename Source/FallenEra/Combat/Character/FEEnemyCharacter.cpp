#include "Combat/Character/FEEnemyCharacter.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"

#include "AbilitySystem/FallenEraAbilitySet.h"
#include "AbilitySystem/FallenEraAbilitySystemComponent.h"
#include "AbilitySystem/FallenEraGameplayTags.h"
#include "AbilitySystem/Attributes/FallenEraAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Combat/Ability/FEEnemyMeleeAttackAbility.h"
#include "Combat/AI/FEAISettingsDataAsset.h"
#include "Combat/AI/FEEnemyAIController.h"
#include "Combat/FECombatTeams.h"
#include "Combat/Component/FECombatComponent.h"
#include "Combat/Component/FEEquipmentComponent.h"
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
	EquipmentComponent = CreateDefaultSubobject<UFE_EquipmentComponent>(TEXT("EquipmentComponent"));
	NavigationInvokerComponent = CreateDefaultSubobject<UNavigationInvokerComponent>(TEXT("NavigationInvokerComponent"));
	NavigationInvokerComponent->SetGenerationRadii(3000.0f, 5000.0f);

	AttackAbilityClass = UFE_EnemyMeleeAttackAbility::StaticClass();

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
	InitializeEnemyAbilitySystem();
	// Component BeginPlay may run before this character initializes its ASC.
	EquipmentComponent->RefreshEquipment();
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
	else if (!AISettingsDataAssets.IsEmpty())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("%s has no valid AI settings at index %d."),
			*GetName(),
			SelectedAISettingsIndex);
	}

	if (CombatComponent)
	{
		CombatComponent->SetHitReactionMontages(CachedAISettings.HitReactionMontages);
	}
	if (HasAuthority() && !CachedAISettings.DamageEffect.IsNull() && !CachedAISettings.DamageEffect.IsValid())
	{
		AISettingsEffectLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
			CachedAISettings.DamageEffect.ToSoftObjectPath(),
			FStreamableDelegate::CreateUObject(this, &ThisClass::FinishAISettingsInitialization));
		return;
	}
	FinishAISettingsInitialization();
}

void AFE_EnemyCharacter::FinishAISettingsInitialization()
{
	CachedAttackDamageEffect = CachedAISettings.DamageEffect.Get();
	bAISettingsCached = (AISettingsDataAssets.IsEmpty() || CachedAISettingsDataAsset != nullptr) &&
		(!HasAuthority() || CachedAISettings.DamageEffect.IsNull() || CachedAttackDamageEffect != nullptr);
}

void AFE_EnemyCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AISettingsLoadHandle) { AISettingsLoadHandle->CancelHandle(); AISettingsLoadHandle.Reset(); }
	if (AISettingsEffectLoadHandle) { AISettingsEffectLoadHandle->CancelHandle(); AISettingsEffectLoadHandle.Reset(); }
	Super::EndPlay(EndPlayReason);
}

bool AFE_EnemyCharacter::GatherAISettingsAssetPaths(TArray<FSoftObjectPath>& OutPaths, bool bGameplayDependencies) const
{
	for (int32 Index = 0; Index < AISettingsDataAssets.Num(); ++Index)
	{
		if (!bRandomizeAISettings && Index != SelectedAISettingsIndex) { continue; }
		const auto& Preset = AISettingsDataAssets[Index];
		if (Preset.IsNull()) { continue; }
		if (!bGameplayDependencies) { OutPaths.AddUnique(Preset.ToSoftObjectPath()); }
		else
		{
			const UFE_AISettingsDataAsset* Data = Preset.Get();
			if (!Data) { return false; }
			if (!Data->AISettings.DamageEffect.IsNull())
			{
				OutPaths.AddUnique(Data->AISettings.DamageEffect.ToSoftObjectPath());
			}
		}
	}
	return true;
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
		for (const UFallenEraAbilitySet* AbilitySet : DefaultAbilitySets)
		{
			if (AbilitySet)
			{
				AbilitySet->GiveToAbilitySystem(AbilitySystemComponent, this);
			}
		}

		if (AttackAbilityClass && !AbilitySystemComponent->FindAbilitySpecFromClass(AttackAbilityClass))
		{
			AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AttackAbilityClass, 1, INDEX_NONE, this));
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

bool AFE_EnemyCharacter::TryStartAttack(AActor* TargetActor)
{
	if (!HasAuthority() || !AbilitySystemComponent || !AttackAbilityClass || !IsValid(TargetActor) ||
		!bAISettingsCached || !UFE_CombatComponent::CanActorAttack(this) || IsAttackInProgress() || FECombatTeams::AreSameTeam(this, TargetActor) ||
		CachedAISettings.AttackMontages.IsEmpty())
	{
		return false;
	}
	if (FVector::DistSquared(GetActorLocation(), TargetActor->GetActorLocation()) >
		FMath::Square(FMath::Max(0.0f, CachedAISettings.AttackRange)))
	{
		return false;
	}

	PendingAttackTarget = TargetActor;
	const bool bActivated = AbilitySystemComponent->TryActivateAbilityByClass(AttackAbilityClass, false);
	if (!bActivated)
	{
		PendingAttackTarget.Reset();
	}
	return bActivated;
}

bool AFE_EnemyCharacter::IsAttackInProgress() const
{
	if (!AbilitySystemComponent || !AttackAbilityClass)
	{
		return false;
	}
	const FGameplayAbilitySpec* AttackSpec = AbilitySystemComponent->FindAbilitySpecFromClass(AttackAbilityClass);
	return AttackSpec && AttackSpec->IsActive();
}

void AFE_EnemyCharacter::CancelActiveAttack()
{
	if (!HasAuthority() || !AbilitySystemComponent || !AttackAbilityClass)
	{
		return;
	}
	if (const FGameplayAbilitySpec* AttackSpec = AbilitySystemComponent->FindAbilitySpecFromClass(AttackAbilityClass);
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
