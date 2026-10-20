#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AbilitySystem/FallenEraAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/FallenEraAttributeSet.h"
#include "Camera/CameraComponent.h"
#include "Combat/Ability/FEHitscanAttackAbility.h"
#include "Combat/Collision/FECollisionChannels.h"
#include "Combat/Component/FEEquipmentComponent.h"
#include "Combat/FECombatGameplayTags.h"
#include "Combat/Interface/FECombatPresentation.h"
#include "Combat/Tests/FEMeleeDamageTestActor.h"
#include "Combat/Weapon/FEWeaponItemData.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "FallenEraCharacter.h"
#include "FallenEraPlayerState.h"
#include "GameFramework/PlayerController.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_CombatTraceViewTest, "FallenEra.Combat.Presentation.FirstPersonAttackOrigin",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFE_CombatTraceViewTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	UClass* PlayerClass = LoadClass<AFallenEraCharacter>(nullptr,
		TEXT("/Game/Combat/Character/PlayerCharacter/BP_HT_PlayerCharacter.BP_HT_PlayerCharacter_C"));
	AFallenEraCharacter* Source = PlayerClass ? World->SpawnActor<AFallenEraCharacter>(PlayerClass,
		FVector(0.0f, 0.0f, 100.0f), FRotator::ZeroRotator) : nullptr;
	if (!TestNotNull(TEXT("Concrete player Blueprint"), Source))
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
		return false;
	}
	APlayerController* Controller = World->SpawnActor<APlayerController>();
	Controller->SetAsLocalPlayerController();
	Controller->PlayerState = World->SpawnActor<AFallenEraPlayerState>();
	World->InitializeActorsForPlay(FURL());
	Controller->Possess(Source);
	UFallenEraAbilitySystemComponent* ASC = Source->GetFallenEraAbilitySystemComponent();
	if (!TestNotNull(TEXT("Player ASC"), ASC))
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
		return false;
	}
	ASC->SetNumericAttributeBase(UFallenEraAttributeSet::GetAttackPowerAttribute(), 20.0f);
	ASC->SetNumericAttributeBase(UFallenEraAttributeSet::GetAccuracyAttribute(), 1.0f);
	ASC->SetNumericAttributeBase(UFallenEraAttributeSet::GetRecoilControlAttribute(), 1.0f);
	FVector FirstOrigin;
	FRotator FirstRotation;
	IFE_CombatPresentation::GetCombatTraceView(Source, FirstOrigin, FirstRotation);
	TestTrue(TEXT("FP attack starts at FP camera"), FirstOrigin.Equals(Source->GetFirstPersonCameraComponent()->GetComponentLocation()));
	Source->SetThirdPersonView(true);
	FMinimalViewInfo ThirdView;
	Source->CalcCamera(0.0f, ThirdView);
	Source->GetFirstPersonCameraComponent()->SetWorldRotation(FRotator(0.0f, -90.0f, 0.0f));
	FVector Origin;
	FRotator Rotation;
	IFE_CombatPresentation::GetCombatTraceView(Source, Origin, Rotation);
	FVector ActiveLocation;
	FRotator ActiveRotation;
	Controller->GetPlayerViewPoint(ActiveLocation, ActiveRotation);
	TestTrue(TEXT("TP attacks keep the FP origin"), Origin.Equals(FirstOrigin));
	TestFalse(TEXT("TP shoulder location is not the attack origin"), Origin.Equals(ThirdView.Location));
	TestTrue(TEXT("Aim follows current view, not inactive FP camera rotation"), Rotation.Equals(ActiveRotation));
	TestFalse(TEXT("Fixture has stale FP rotation"), Rotation.Equals(Source->GetFirstPersonCameraComponent()->GetComponentRotation()));

	const float CameraBehindOrigin = FVector::DotProduct(Origin - ThirdView.Location, Rotation.Vector());
	AFE_MeleeDamageTestActor* Target = World->SpawnActor<AFE_MeleeDamageTestActor>(
		ThirdView.Location + Rotation.Vector() * (CameraBehindOrigin + 100.0f), FRotator::ZeroRotator);
	UBoxComponent* TargetBox = CastChecked<UBoxComponent>(Target->GetRootComponent());
	TargetBox->SetCollisionResponseToChannel(FECollisionChannels::PlayerHitscanTrace, ECR_Overlap);
	const FFE_CombatAim Aim = IFE_CombatPresentation::ResolveCombatAim(Source, 150.0f, FECollisionChannels::PlayerHitscanTrace);
	TestTrue(TEXT("TP center query accepts a damageable Overlap response"), Aim.bHasCameraTarget);
	TestTrue(TEXT("TP camera selects the point but FP remains the origin"), Aim.Origin.Equals(Origin) && Aim.CameraStart.Equals(ThirdView.Location));
	TestFalse(TEXT("TP aim converges instead of staying parallel"), Aim.Direction.Equals(Rotation.Vector()));
	TestEqual(TEXT("Aim selection does not apply damage"), Target->HitCount, 0);
	UFE_RangedWeaponItemData* Weapon = NewObject<UFE_RangedWeaponItemData>(Source);
	UFE_HitscanAttackData* Hitscan = NewObject<UFE_HitscanAttackData>(Weapon);
	Hitscan->InputTag = FallenEraCombatGameplayTags::Ability_Input_Combat_LeftClick;
	Hitscan->AbilityClass = UFE_HitscanAttackAbility::StaticClass();
	Hitscan->bAutomatic = false;
	Hitscan->bDrawDebugTrace = false;
	Weapon->Template.AttackActions.Add(Hitscan);
	FindFProperty<FObjectPropertyBase>(UFE_EquipmentComponent::StaticClass(), TEXT("CurrentWeaponData"))
		->SetObjectPropertyValue_InContainer(Source->GetEquipmentComponent(), Weapon);
	FGameplayAbilitySpec Spec(UFE_HitscanAttackAbility::StaticClass(), 1, INDEX_NONE, Weapon);
	Spec.GetDynamicSpecSourceTags().AddTag(Hitscan->InputTag);
	const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(Spec);
	const auto FireAgain = [&]()
	{
		FGameplayAbilitySpec FreshSpec(UFE_HitscanAttackAbility::StaticClass(), 1, INDEX_NONE, Weapon);
		FreshSpec.GetDynamicSpecSourceTags().AddTag(Hitscan->InputTag);
		return ASC->TryActivateAbility(ASC->GiveAbility(FreshSpec));
	};
	TestTrue(TEXT("TP hitscan activates"), ASC->TryActivateAbility(Handle));
	TestEqual(TEXT("TP hitscan converges on the shoulder-camera center target"), Target->HitCount, 1);
	TestTrue(TEXT("Actual hitscan damage ray starts at FP camera"), Target->LastRequest.HitResult.TraceStart.Equals(Origin));

	UFE_MeleeAttackData* Melee = NewObject<UFE_MeleeAttackData>(Weapon);
	Melee->TraceRange = 150.0f;
	Melee->TraceHalfExtent = FVector(5.0f);
	Melee->bDrawDebugTrace = false;
	NewObject<UFE_MeleeDamageTestAbility>(Source)->RunTrace(Source, Weapon, Melee);
	TestEqual(TEXT("TP melee reaches the camera-center target within FP reach"), Target->HitCount, 2);
	TestTrue(TEXT("Actual melee center ray starts at FP camera"), Target->LastRequest.HitResult.TraceStart.Equals(Origin));

	UFE_ProjectileAttackData* Projectile = NewObject<UFE_ProjectileAttackData>(Weapon);
	Projectile->bUseAimDirection = true;
	Projectile->bDrawDebugAim = false;
	FVector LaunchLocation;
	FVector LaunchDirection;
	TestTrue(TEXT("Projectile launch can be resolved"), NewObject<UFE_ProjectileAimTestAbility>(Source)
		->GetPreviewLaunch(Source, Projectile, LaunchLocation, LaunchDirection));
	FCollisionQueryParams Query(SCENE_QUERY_STAT(FE_TestAimOrigin), true, Source);
	const FFE_CombatAim ProjectileAim = IFE_CombatPresentation::ResolveCombatAim(Source, Projectile->AimTraceRange, FECollisionChannels::PlayerHitscanTrace);
	TArray<FHitResult> AimHits;
	World->LineTraceMultiByChannel(AimHits, Origin, ProjectileAim.AimPoint,
		FECollisionChannels::PlayerHitscanTrace, Query);
	const FVector ExpectedPoint = AimHits.IsEmpty() ? ProjectileAim.AimPoint : AimHits[0].ImpactPoint;
	TestTrue(TEXT("Projectile converges on the validated TP center point"),
		LaunchDirection.Equals((ExpectedPoint - LaunchLocation).GetSafeNormal(), 0.001f));
	TestTrue(TEXT("Projectile physical spawn fallback is unchanged"), LaunchLocation.Equals(Source->GetActorLocation()));

	AActor* Wall = World->SpawnActor<AActor>();
	UBoxComponent* WallBox = NewObject<UBoxComponent>(Wall);
	Wall->SetRootComponent(WallBox);
	WallBox->SetBoxExtent(FVector(2.0f));
	WallBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	WallBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	WallBox->SetCollisionResponseToChannel(FECollisionChannels::PlayerHitscanTrace, ECR_Block);
	WallBox->SetCollisionResponseToChannel(FECollisionChannels::PlayerMeleeTrace, ECR_Block);
	WallBox->SetCollisionResponseToChannel(FECollisionChannels::PlayerMeleeAreaTrace, ECR_Block);
	WallBox->RegisterComponent();
	Wall->SetActorLocation((Origin + Aim.AimPoint) * 0.5f);
	TestTrue(TEXT("Shoulder camera still sees target around FP obstruction"),
		IFE_CombatPresentation::ResolveCombatAim(Source, 150.0f, FECollisionChannels::PlayerHitscanTrace).bHasCameraTarget);
	TestTrue(TEXT("Separate shot is activated for cover test"), FireAgain());
	TestEqual(TEXT("FP damage trace cannot shoot through cover visible around camera"), Target->HitCount, 2);
	NewObject<UFE_MeleeDamageTestAbility>(Source)->RunTrace(Source, Weapon, Melee);
	TestEqual(TEXT("Melee center and area remain blocked by cover"), Target->HitCount, 2);
	NewObject<UFE_ProjectileAimTestAbility>(Source)->GetPreviewLaunch(Source, Projectile, LaunchLocation, LaunchDirection);
	TestTrue(TEXT("Projectile aims at FP-path obstruction, not the hidden enemy"),
		LaunchDirection.Equals((Wall->GetActorLocation() - LaunchLocation).GetSafeNormal(), 0.08f));
	WallBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	AFE_MeleeDamageTestActor* Behind = World->SpawnActor<AFE_MeleeDamageTestActor>(
		ThirdView.Location + Rotation.Vector() * (CameraBehindOrigin * 0.5f), FRotator::ZeroRotator);
	CastChecked<UBoxComponent>(Behind->GetRootComponent())->SetCollisionResponseToChannel(FECollisionChannels::PlayerHitscanTrace, ECR_Block);
	const FFE_CombatAim BehindAim = IFE_CombatPresentation::ResolveCombatAim(Source, 150.0f, FECollisionChannels::PlayerHitscanTrace);
	TestFalse(TEXT("Camera hits behind FP origin are not aim targets"), BehindAim.bHasCameraTarget);
	TestTrue(TEXT("Fallback cannot reverse the attack direction"), FVector::DotProduct(BehindAim.Direction, Rotation.Vector()) > 0.0f);
	Behind->SetActorEnableCollision(false);
	Target->SetActorEnableCollision(false);
	const FFE_CombatAim MissAim = IFE_CombatPresentation::ResolveCombatAim(Source, 150.0f, FECollisionChannels::PlayerHitscanTrace);
	TestTrue(TEXT("A missed center query converges on the far camera point"),
		!MissAim.bHasCameraTarget && MissAim.AimPoint.Equals(MissAim.CameraEnd));
	Target->SetActorLocation(ThirdView.Location + Rotation.Vector() * (CameraBehindOrigin + 250.0f));
	Target->SetActorEnableCollision(true);
	NewObject<UFE_MeleeDamageTestAbility>(Source)->RunTrace(Source, Weapon, Melee);
	TestEqual(TEXT("Camera correction does not extend melee weapon reach"), Target->HitCount, 2);
	Target->SetActorLocation(ThirdView.Location + Rotation.Vector() * (CameraBehindOrigin + 100.0f));
	AFE_MeleeDamageTestActor* Farther = World->SpawnActor<AFE_MeleeDamageTestActor>(Origin + Aim.Direction * 250.0f, FRotator::ZeroRotator);
	CastChecked<UBoxComponent>(Farther->GetRootComponent())->SetCollisionResponseToChannel(FECollisionChannels::PlayerHitscanTrace, ECR_Overlap);
	Hitscan->PenetrationCount = 1;
	TestTrue(TEXT("Separate shot is activated for penetration test"), FireAgain());
	TestEqual(TEXT("Camera-selected target is damaged exactly once"), Target->HitCount, 3);
	TestEqual(TEXT("Actual trace continues beyond aim point for penetration"), Farther->HitCount, 1);
	Source->SetThirdPersonView(false);
	IFE_CombatPresentation::GetCombatTraceView(Source, Origin, Rotation);
	TestTrue(TEXT("Returning to FP keeps the same attack origin"), Origin.Equals(FirstOrigin));
	const FFE_CombatAim FirstAim = IFE_CombatPresentation::ResolveCombatAim(Source, 150.0f, FECollisionChannels::PlayerHitscanTrace);
	TestTrue(TEXT("FP has no shoulder convergence correction"), FirstAim.Direction.Equals(Rotation.Vector()) && FirstAim.CameraStart.Equals(Origin));
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

#endif
