#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Combat/Collision/FECameraMeleeTrace.h"
#include "Combat/Collision/FECollisionChannels.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/CollisionProfile.h"
#include "GameFramework/Actor.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_CameraMeleeTraceTest, "FallenEra.Combat.Melee.CameraVolume",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFE_CameraMeleeTraceTest::RunTest(const FString& Parameters)
{
	ECollisionChannel ObjectChannel;
	FCollisionResponseParams Responses;
	TestTrue(TEXT("EnemyMesh collision profile exists"), UCollisionProfile::GetChannelAndResponseParams(TEXT("EnemyMesh"), ObjectChannel, Responses));
	TestEqual(TEXT("Enemy mesh blocks center melee"), Responses.CollisionResponse.GetResponse(FECollisionChannels::PlayerMeleeTrace), ECR_Block);
	TestEqual(TEXT("Enemy mesh overlaps area melee"), Responses.CollisionResponse.GetResponse(FECollisionChannels::PlayerMeleeAreaTrace), ECR_Overlap);
	TestTrue(TEXT("EnemyCapsule collision profile exists"), UCollisionProfile::GetChannelAndResponseParams(TEXT("EnemyCapsule"), ObjectChannel, Responses));
	TestEqual(TEXT("Enemy capsule ignores center melee"), Responses.CollisionResponse.GetResponse(FECollisionChannels::PlayerMeleeTrace), ECR_Ignore);
	TestEqual(TEXT("Enemy capsule ignores area melee"), Responses.CollisionResponse.GetResponse(FECollisionChannels::PlayerMeleeAreaTrace), ECR_Ignore);
	TestTrue(TEXT("BuildPiece collision profile exists"), UCollisionProfile::GetChannelAndResponseParams(TEXT("BuildPiece"), ObjectChannel, Responses));
	TestEqual(TEXT("Building blocks center melee"), Responses.CollisionResponse.GetResponse(FECollisionChannels::PlayerMeleeTrace), ECR_Block);
	TestEqual(TEXT("Building blocks area melee"), Responses.CollisionResponse.GetResponse(FECollisionChannels::PlayerMeleeAreaTrace), ECR_Block);
	const FVector Eye = FVector::ZeroVector;
	const FVector TallSize(10.0f, 15.0f, 60.0f);
	FFE_CameraMeleeTraceVolume Tall = FECameraMeleeTrace::MakeVolume(Eye, FRotator::ZeroRotator, 200.0f, TallSize);
	TestTrue(TEXT("Range measures front face, not box center"),
		(Tall.End + Tall.Rotation.GetForwardVector() * Tall.HalfExtent.X).Equals(FVector(200.0f, 0.0f, 0.0f)));
	TestTrue(TEXT("No volume behind camera"),
		(Tall.Start - Tall.Rotation.GetForwardVector() * Tall.HalfExtent.X).Equals(Eye));
	const FFE_CameraMeleeTraceVolume Turned = FECameraMeleeTrace::MakeVolume(Eye, FRotator(45.0f, 90.0f, 0.0f), 200.0f, TallSize);
	TestTrue(TEXT("Pitch and yaw orient the whole volume"), Turned.Rotation.Equals(FRotator(45.0f, 90.0f, 0.0f).Quaternion()));
	TestFalse(TEXT("Zero reach has no hit volume"), FECameraMeleeTrace::MakeVolume(Eye, FRotator::ZeroRotator, 0.0f, TallSize).bValid);
	const FFE_CameraMeleeTraceVolume Thick = FECameraMeleeTrace::MakeVolume(Eye, FRotator::ZeroRotator, 20.0f, FVector(100.0f));
	TestEqual(TEXT("Oversized depth is clamped to reach"), Thick.HalfExtent.X, 10.0);

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Query world"), World)) { return false; }
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	UPhysicalMaterial* Material = NewObject<UPhysicalMaterial>(World);
	auto AddBox = [World, Material](const FVector& Location, const FVector& Size, ECollisionResponse Response)
	{
		AActor* Actor = World->SpawnActor<AActor>();
		UBoxComponent* Box = NewObject<UBoxComponent>(Actor);
		Actor->SetRootComponent(Box);
		Box->SetBoxExtent(Size);
		Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Box->SetCollisionResponseToAllChannels(ECR_Ignore);
		Box->SetCollisionResponseToChannel(FECollisionChannels::PlayerMeleeTrace,
			Response == ECR_Block ? ECR_Block : ECR_Ignore);
		Box->SetCollisionResponseToChannel(FECollisionChannels::PlayerMeleeAreaTrace,
			Response);
		Box->SetPhysMaterialOverride(Material);
		Box->RegisterComponent();
		Actor->SetActorLocation(Location);
		return Actor;
	};
	AActor* Near = AddBox(FVector(40.0f, 0.0f, 0.0f), FVector(5.0f), ECR_Overlap);
	AActor* High = AddBox(FVector(140.0f, 0.0f, 45.0f), FVector(5.0f), ECR_Overlap);
	AActor* Wide = AddBox(FVector(140.0f, 45.0f, 0.0f), FVector(5.0f), ECR_Overlap);
	AActor* Far = AddBox(FVector(215.0f, 0.0f, 0.0f), FVector(5.0f), ECR_Overlap);
	AActor* Behind = AddBox(FVector(-30.0f, 0.0f, 0.0f), FVector(5.0f), ECR_Overlap);
	TArray<FHitResult> Hits;
	auto HasActor = [&Hits](const AActor* Actor)
	{
		return Hits.ContainsByPredicate([Actor](const FHitResult& Hit) { return Hit.GetActor() == Actor; });
	};
	FECameraMeleeTrace::Sweep(*World, nullptr, Tall, FECollisionChannels::PlayerMeleeTrace, Hits);
	TestTrue(TEXT("Near target is not missed by endpoint-only detection"), HasActor(Near));
	const FHitResult* NearHit = Hits.FindByPredicate([Near](const FHitResult& Hit) { return Hit.GetActor() == Near; });
	TestTrue(TEXT("Physical material is returned for impact audio"), NearHit && NearHit->PhysMaterial.Get() == Material);
	TestTrue(TEXT("Tall box reaches high target"), HasActor(High));
	TestFalse(TEXT("Tall narrow box excludes side target"), HasActor(Wide));
	TestFalse(TEXT("No extra forward reach from box depth"), HasActor(Far));
	TestFalse(TEXT("No targets behind camera"), HasActor(Behind));
	FFE_CameraMeleeTraceVolume Horizontal = FECameraMeleeTrace::MakeVolume(Eye, FRotator::ZeroRotator, 200.0f, FVector(10.0f, 60.0f, 15.0f));
	FECameraMeleeTrace::Sweep(*World, nullptr, Horizontal, FECollisionChannels::PlayerMeleeTrace, Hits);
	TestTrue(TEXT("Wide low box reaches side target"), HasActor(Wide));
	TestFalse(TEXT("Wide low box excludes high target"), HasActor(High));
	FECameraMeleeTrace::Sweep(*World, Near, Tall, FECollisionChannels::PlayerMeleeTrace, Hits);
	TestFalse(TEXT("Attacker excluded"), HasActor(Near));
	AActor* Close = AddBox(FVector(8.0f, 0.0f, 0.0f), FVector(3.0f), ECR_Overlap);
	FECameraMeleeTrace::Sweep(*World, nullptr, Tall, FECollisionChannels::PlayerMeleeTrace, Hits);
	TestTrue(TEXT("Target overlapping initial box is still hittable"), HasActor(Close));
	AActor* Side = AddBox(FVector(0.0f, 140.0f, 0.0f), FVector(5.0f), ECR_Overlap);
	FFE_CameraMeleeTraceVolume Sideways = FECameraMeleeTrace::MakeVolume(Eye, FRotator(0.0f, 90.0f, 0.0f), 200.0f, TallSize);
	FECameraMeleeTrace::Sweep(*World, nullptr, Sideways, FECollisionChannels::PlayerMeleeTrace, Hits);
	TestTrue(TEXT("Query follows camera yaw instead of actor forward or sockets"), HasActor(Side));
	AActor* Wall = AddBox(FVector(90.0f, 0.0f, 0.0f), FVector(5.0f, 100.0f, 100.0f), ECR_Block);
	FECameraMeleeTrace::Sweep(*World, nullptr, Tall, FECollisionChannels::PlayerMeleeTrace, Hits);
	TestTrue(TEXT("Blocking wall still returns an impact"), HasActor(Wall));
	TestFalse(TEXT("Cannot hit high target through wall"), HasActor(High));
	TestTrue(TEXT("Visible near target remains before wall"), HasActor(Near));

	TestTrue(TEXT("Center wall does not change configured box geometry"),
		(Tall.End + Tall.Rotation.GetForwardVector() * Tall.HalfExtent.X).Equals(FVector(200.0f, 0.0f, 0.0f), 0.01f));
	AActor* SideBlock = AddBox(FVector(50.0f, 30.0f, 0.0f), FVector(5.0f), ECR_Block);
	FECameraMeleeTrace::Sweep(*World, nullptr, Horizontal, FECollisionChannels::PlayerMeleeTrace, Hits);
	TestFalse(TEXT("World object off the center ray is never an area target"), HasActor(SideBlock));
	TestTrue(TEXT("Center world target is returned first"), !Hits.IsEmpty() && Hits[0].GetActor() == Wall);

	Wall->SetActorLocation(FVector(1000.0f));
	SideBlock->SetActorLocation(FVector(1000.0f));
	UPrimitiveComponent* NearComponent = CastChecked<UPrimitiveComponent>(Near->GetRootComponent());
	NearComponent->SetCollisionResponseToChannel(FECollisionChannels::PlayerMeleeTrace, ECR_Block);
	AActor* Flank = AddBox(FVector(33.0f, -30.0f, 0.0f), FVector(5.0f), ECR_Overlap);
	AActor* BehindNear = AddBox(FVector(65.0f, 0.0f, 0.0f), FVector(5.0f), ECR_Overlap);
	Horizontal = FECameraMeleeTrace::MakeVolume(Eye, FRotator::ZeroRotator, 200.0f, FVector(10.0f, 60.0f, 15.0f));
	FECameraMeleeTrace::Sweep(*World, nullptr, Horizontal, FECollisionChannels::PlayerMeleeTrace, Hits);
	TestTrue(TEXT("Center enemy has priority"), !Hits.IsEmpty() && Hits[0].GetActor() == Near);
	TestTrue(TEXT("Off-center enemy is hittable without a center-ray hit"), HasActor(Flank));
	TestTrue(TEXT("Enemy behind center remains a glancing candidate"), HasActor(BehindNear));
	TestTrue(TEXT("Close center does not remove farther side enemy"), HasActor(Wide));
	TestEqual(TEXT("Center enemy is returned only once"),
		Hits.FilterByPredicate([Near](const FHitResult& Hit) { return Hit.GetActor() == Near; }).Num(), 1);
	TestTrue(TEXT("Center enemy preserves material and area classification"),
		Hits[0].PhysMaterial.Get() == Material && FECameraMeleeTrace::IsAreaTarget(Hits[0]));
	TestTrue(TEXT("Center enemy does not shrink area range"),
		(Horizontal.End + Horizontal.Rotation.GetForwardVector() * Horizontal.HalfExtent.X).Equals(FVector(200.0f, 0.0f, 0.0f), 0.01f));

	// A center miss must not disable off-center detection or retain the previous frame's cap.
	NearComponent->SetCollisionResponseToChannel(FECollisionChannels::PlayerMeleeTrace, ECR_Ignore);
	SideBlock->SetActorLocation(FVector(1000.0f));
	Horizontal = FECameraMeleeTrace::MakeVolume(Eye, FRotator::ZeroRotator, 200.0f, FVector(10.0f, 60.0f, 15.0f));
	FECameraMeleeTrace::Sweep(*World, nullptr, Horizontal, FECollisionChannels::PlayerMeleeTrace, Hits);
	TestTrue(TEXT("Center miss uses full range for side enemy"), HasActor(Wide));
	TestTrue(TEXT("Center miss restores full forward reach"),
		(Horizontal.End + Horizontal.Rotation.GetForwardVector() * Horizontal.HalfExtent.X).Equals(FVector(200.0f, 0.0f, 0.0f), 0.01f));


	AddBox(FVector(10.0f, 0.0f, 0.0f), FVector(5.0f, 100.0f, 100.0f), ECR_Block);
	FECameraMeleeTrace::Sweep(*World, nullptr, Horizontal, FECollisionChannels::PlayerMeleeTrace, Hits);
	TestFalse(TEXT("Initial wall penetration fails closed for glancing candidates"), HasActor(Wide));
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

#endif
