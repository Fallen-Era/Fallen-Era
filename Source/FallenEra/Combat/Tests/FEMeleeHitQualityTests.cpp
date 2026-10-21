#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/FallenEraAttributeSet.h"
#include "Combat/Collision/FECollisionChannels.h"
#include "Combat/Collision/FECameraMeleeTrace.h"
#include "Combat/Component/FECombatComponent.h"
#include "Combat/Component/FEEquipmentComponent.h"
#include "Combat/GameplayEffect/FEDamageGameplayEffect.h"
#include "Combat/Tests/FEMeleeDamageTestActor.h"
#include "Combat/Weapon/FEWeaponItemData.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_MeleeHitQualityTest, "FallenEra.Combat.Melee.HitQuality",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFE_MeleeHitQualityTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Test world"), World)) { return false; }
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ACharacter* Source = World->SpawnActor<ACharacter>();
	Source->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	UAbilitySystemComponent* ASC = NewObject<UAbilitySystemComponent>(Source);
	ASC->RegisterComponent();
	ASC->InitializeComponent();
	UFallenEraAttributeSet* Stats = NewObject<UFallenEraAttributeSet>(Source);
	ASC->AddAttributeSetSubobject(Stats);
	ASC->InitAbilityActorInfo(Source, Source);
	Stats->InitAttackPower(20.0f);
	UFE_CombatComponent* Combat = NewObject<UFE_CombatComponent>(Source);
	Combat->RegisterComponent();
	UFE_MeleeWeaponItemData* Weapon = NewObject<UFE_MeleeWeaponItemData>(Source);
	Weapon->Template.DefaultDamageEffect = UFE_DamageGameplayEffect::StaticClass();
	UFE_MeleeAttackData* Attack = NewObject<UFE_MeleeAttackData>(Weapon);
	Attack->MaxHitCount = 4;
	Attack->TraceHalfExtent = FVector(10.0f, 60.0f, 15.0f);
	Attack->IndirectDamageMultiplier = 0.4f;
	const FVector Eye = Source->GetPawnViewLocation();
	auto AddTarget = [World, Eye](const FVector& Offset)
	{
		return World->SpawnActor<AFE_MeleeDamageTestActor>(Eye + Offset, FRotator::ZeroRotator);
	};
	AFE_MeleeDamageTestActor* Center = AddTarget(FVector(40.0f, 0.0f, 0.0f));
	AFE_MeleeDamageTestActor* Side = AddTarget(FVector(120.0f, 35.0f, 0.0f));
	AFE_MeleeDamageTestActor* Behind = AddTarget(FVector(90.0f, 0.0f, 0.0f));
	// Actor ProcessEvent suppresses BlueprintNativeEvent dispatch before actors are initialized.
	World->InitializeActorsForPlay(FURL());
	TestTrue(TEXT("Damageable event dispatch is enabled"), World->AreActorsInitialized());
	TestTrue(TEXT("Source component is discoverable"), Source->FindComponentByClass<UFE_CombatComponent>() == Combat);
	TestTrue(TEXT("Source remains alive"), UFE_CombatComponent::IsActorAlive(Source));
	TestTrue(TEXT("Target implements damageable"), Center->GetClass()->ImplementsInterface(UFE_Damageable::StaticClass()));
	TestTrue(TEXT("Target collider registered"), Center->GetRootComponent()->IsRegistered());
	TArray<FHitResult> PreviewHits;
	FHitResult PreviewCenter;
	const FFE_CameraMeleeTraceVolume PreviewVolume = FECameraMeleeTrace::MakeVolume(Eye, Source->GetBaseAimRotation(), Attack->TraceRange, Attack->TraceHalfExtent);
	FECameraMeleeTrace::Sweep(*World, Source, PreviewVolume, FECollisionChannels::PlayerMeleeTrace, PreviewHits, &PreviewCenter);
	TestEqual(TEXT("Fixture collects three actual targets"), PreviewHits.Num(), 3);
	TestTrue(TEXT("Fixture center ray reaches intended target"), PreviewCenter.GetActor() == Center);
	UFE_MeleeDamageTestAbility* Ability = NewObject<UFE_MeleeDamageTestAbility>(Source);
	Ability->RunTrace(Source, Weapon, Attack);
	TestEqual(TEXT("Center enemy receives exactly one hit, not line plus box"), Center->HitCount, 1);
	TestEqual(TEXT("Center receives full damage"), Center->Health, 985.0f);
	TestEqual(TEXT("Close center does not shrink side reach"), Side->HitCount, 1);
	TestEqual(TEXT("Side receives post-defense x0.4 damage without an ASC"), Side->Health, 994.0f);
	TestEqual(TEXT("Enemy behind center is allowed as glancing target"), Behind->LastRequest.HitQualityMultiplier, 0.4f);
	TestEqual(TEXT("Shared attack power is unchanged"), Stats->GetAttackPower(), 20.0f);
	Ability->RunTrace(Source, Weapon, Attack);
	TestEqual(TEXT("Same swing does not damage again"), Side->HitCount, 1);
	Source->SetActorRotation(FVector(120.0f, 35.0f, 0.0f).Rotation());
	Ability->RunTrace(Source, Weapon, Attack);
	TestEqual(TEXT("Reaiming cannot damage the same target again in one swing"), Side->HitCount, 1);
	TestEqual(TEXT("First glancing damage remains unchanged"), Side->Health, 994.0f);
	Source->SetActorRotation(FRotator::ZeroRotator);
	AFE_MeleeDamageTestActor* Wall = AddTarget(FVector(150.0f, 0.0f, 0.0f));
	UBoxComponent* WallBox = CastChecked<UBoxComponent>(Wall->GetRootComponent());
	WallBox->SetBoxExtent(FVector(5.0f, 100.0f, 100.0f));
	WallBox->SetCollisionResponseToChannel(FECollisionChannels::PlayerMeleeAreaTrace, ECR_Block);
	AFE_MeleeDamageTestActor* FarSide = AddTarget(FVector(180.0f, 35.0f, 0.0f));
	NewObject<UFE_MeleeDamageTestAbility>(Source)->RunTrace(Source, Weapon, Attack);
	TestEqual(TEXT("Wall blocks area hits behind it"), FarSide->HitCount, 0);
	TestEqual(TEXT("Off-center sweep blocker receives no world damage"), Wall->HitCount, 0);
	// World interaction has its own one-object budget, not an enemy target slot.
	CastChecked<UBoxComponent>(Center->GetRootComponent())->SetCollisionResponseToChannel(FECollisionChannels::PlayerMeleeTrace, ECR_Ignore);
	CastChecked<UBoxComponent>(Behind->GetRootComponent())->SetCollisionResponseToChannel(FECollisionChannels::PlayerMeleeTrace, ECR_Ignore);
	const int32 CenterHitsBefore = Center->HitCount;
	Attack->MaxHitCount = 1;
	NewObject<UFE_MeleeDamageTestAbility>(Source)->RunTrace(Source, Weapon, Attack);
	TestEqual(TEXT("Only center ray damages the wall"), Wall->HitCount, 1);
	TestEqual(TEXT("Center world hit remains full strength"), Wall->LastRequest.HitQualityMultiplier, 1.0f);
	TestEqual(TEXT("World hit does not consume the only enemy slot"), Center->HitCount, CenterHitsBefore + 1);
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_MeleeNotifyOnceTest, "FallenEra.Combat.Melee.NotifyOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFE_MeleeNotifyOnceTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Test world"), World)) { return false; }
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ACharacter* Source = World->SpawnActor<ACharacter>();
	Source->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	UAbilitySystemComponent* ASC = NewObject<UAbilitySystemComponent>(Source);
	ASC->RegisterComponent();
	ASC->InitializeComponent();
	UFallenEraAttributeSet* Stats = NewObject<UFallenEraAttributeSet>(Source);
	ASC->AddAttributeSetSubobject(Stats);
	ASC->InitAbilityActorInfo(Source, Source);
	Stats->InitAttackPower(20.0f);
	UFE_CombatComponent* Combat = NewObject<UFE_CombatComponent>(Source);
	Combat->RegisterComponent();
	UFE_EquipmentComponent* Equipment = NewObject<UFE_EquipmentComponent>(Source);
	Equipment->RegisterComponent();
	UFE_MeleeWeaponItemData* Weapon = NewObject<UFE_MeleeWeaponItemData>(Source);
	Weapon->Template.DefaultDamageEffect = UFE_DamageGameplayEffect::StaticClass();
	UFE_MeleeAttackData* Attack = NewObject<UFE_MeleeAttackData>(Weapon);
	Attack->AbilityClass = UFE_MeleeAttackAbility::StaticClass();
	Attack->AttackStartEventTag = FGameplayTag::RequestGameplayTag(TEXT("GameplayEvent.Attack.TraceStart"));
	Attack->AttackEndEventTag = FGameplayTag::RequestGameplayTag(TEXT("GameplayEvent.Attack.TraceEnd"));
	Attack->AttackResetEventTag = FGameplayTag::RequestGameplayTag(TEXT("GameplayEvent.Attack.Reset"));
	Attack->AttackInterval = 10.0f;
	Attack->MaxHitCount = 4;
	Weapon->Template.AttackActions.Add(Attack);
	// Isolate notify behavior from asynchronous equipment loading/presentation.
	FindFProperty<FObjectProperty>(Equipment->GetClass(), TEXT("CurrentWeaponData"))
		->SetObjectPropertyValue_InContainer(Equipment, Weapon);
	const FVector TargetLocation = Source->GetPawnViewLocation() + FVector(80.0f, 0.0f, 0.0f);
	AFE_MeleeDamageTestActor* Target = World->SpawnActor<AFE_MeleeDamageTestActor>(TargetLocation, FRotator::ZeroRotator);
	World->InitializeActorsForPlay(FURL());
	const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(
		FGameplayAbilitySpec(UFE_MeleeAttackAbility::StaticClass(), 1, INDEX_NONE, Weapon));
	auto SendEvent = [ASC](FGameplayTag Tag)
	{
		FGameplayEventData Payload;
		Payload.EventTag = Tag;
		ASC->HandleGameplayEvent(Tag, &Payload);
	};
	TestTrue(TEXT("Melee activates"), ASC->TryActivateAbility(Handle));
	World->Tick(LEVELTICK_All, 0.05f);
	TestEqual(TEXT("No hit before notify"), Target->HitCount, 0);
	SendEvent(Attack->AttackStartEventTag);
	TestEqual(TEXT("Notify hits immediately, without waiting for tick"), Target->HitCount, 1);
	AFE_MeleeDamageTestActor* LateTarget = World->SpawnActor<AFE_MeleeDamageTestActor>(
		TargetLocation + FVector(30.0f, 15.0f, 0.0f), FRotator::ZeroRotator);
	World->Tick(LEVELTICK_All, 0.05f);
	SendEvent(Attack->AttackStartEventTag);
	TestEqual(TEXT("No tick damage after notify"), Target->HitCount, 1);
	TestEqual(TEXT("Duplicate notify cannot hit newly entering target"), LateTarget->HitCount, 0);
	SendEvent(Attack->AttackEndEventTag);
	SendEvent(Attack->AttackResetEventTag);
	SendEvent(Attack->AttackStartEventTag);
	TestEqual(TEXT("Held input permits one new trace after reset"), Target->HitCount, 2);
	TestEqual(TEXT("Next swing can hit the new target"), LateTarget->HitCount, 1);

	SendEvent(Attack->AttackResetEventTag);
	Target->SetActorLocation(TargetLocation + FVector(1000.0f, 0.0f, 0.0f));
	LateTarget->SetActorLocation(TargetLocation + FVector(1000.0f, 0.0f, 0.0f));
	SendEvent(Attack->AttackStartEventTag);
	Target->SetActorLocation(TargetLocation);
	World->Tick(LEVELTICK_All, 0.05f);
	SendEvent(Attack->AttackStartEventTag);
	TestEqual(TEXT("Miss consumes the swing even if a target later enters"), Target->HitCount, 2);
	SendEvent(Attack->AttackResetEventTag);
	SendEvent(Attack->AttackEndEventTag);
	SendEvent(Attack->AttackStartEventTag);
	TestEqual(TEXT("End notify suppresses a late start notify"), Target->HitCount, 2);
	ASC->CancelAbilityHandle(Handle);
	SendEvent(Attack->AttackStartEventTag);
	TestEqual(TEXT("Cancellation removes notify listener"), Target->HitCount, 2);
	Attack->AttackStartEventTag = FGameplayTag();
	TestTrue(TEXT("Ability can reactivate after cancellation"), ASC->TryActivateAbility(Handle));
	TestEqual(TEXT("No configured tag falls back to one immediate query"), Target->HitCount, 3);
	World->Tick(LEVELTICK_All, 0.05f);
	TestEqual(TEXT("Fallback also has no tick damage"), Target->HitCount, 3);
	ASC->CancelAbilityHandle(Handle);
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

#endif
