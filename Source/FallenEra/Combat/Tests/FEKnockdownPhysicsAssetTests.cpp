#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Character.h"
#include "HAL/FileManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "UObject/SavePackage.h"

/** Asset migration is opt-in; normal test runs only validate the saved assets. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_KnockdownPhysicsAssetTest,
	"FallenEra.Combat.Reaction.KnockdownPhysicsAsset", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFE_KnockdownPhysicsAssetTest::RunTest(const FString& Parameters)
{
	const TCHAR* AssetPath = TEXT("/Game/Combat/Character/EnemyCharacter/Physics/PA_FE_Enemy");
	UPhysicsAsset* Asset = LoadObject<UPhysicsAsset>(nullptr, AssetPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	const bool bConfigure = FParse::Param(FCommandLine::Get(), TEXT("FEConfigureKnockdownPhysics"));
	const TArray<FString> EnemyPaths = {
		TEXT("/Game/Combat/Character/EnemyCharacter/BP_ArmoredEnemy"),
		TEXT("/Game/Combat/Character/EnemyCharacter/BP_HeavyZombie") };
	const TArray<FString> MontagePaths = {
		TEXT("/Game/Combat/Character/EnemyCharacter/Animations/AM_Getup_Back"),
		TEXT("/Game/Combat/Character/EnemyCharacter/Animations/AM_GetUp_Front") };
	if (bConfigure)
	{
		// These are dirty user assets: back up their current bytes, never restore from Git HEAD.
		const FString BackupDir = FPaths::ProjectSavedDir() / TEXT("Backups/KnockdownPhysics") /
			FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
		IFileManager::Get().MakeDirectory(*BackupDir, true);
		TArray<FString> BackupPaths = EnemyPaths;
		BackupPaths.Append(MontagePaths);
		if (Asset) { BackupPaths.Add(AssetPath); }
		for (const FString& Path : BackupPaths)
		{
			const FString Filename = FPackageName::LongPackageNameToFilename(Path, FPackageName::GetAssetPackageExtension());
			if (!TestEqual(TEXT("Back up ") + Path,
				IFileManager::Get().Copy(*(BackupDir / FPaths::GetCleanFilename(Filename)), *Filename), COPY_OK)) { return false; }
		}
		AddInfo(TEXT("Current asset backup: ") + FPaths::ConvertRelativePathToFull(BackupDir));
		auto Save = [this](UObject* Object)
		{
			UPackage* Package = Object->GetOutermost();
			const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
			FSavePackageArgs Args;
			Args.TopLevelFlags = RF_Public | RF_Standalone;
			return TestTrue(TEXT("Save ") + Object->GetName(), UPackage::SavePackage(Package, Object, *Filename, Args));
		};
		if (!Asset)
		{
			UPhysicsAsset* Source = LoadObject<UPhysicsAsset>(nullptr, TEXT("/Game/Characters/Mannequins/Rigs/PA_Mannequin"));
			if (!TestNotNull(TEXT("Source Physics Asset retained unchanged"), Source)) { return false; }
			Asset = DuplicateObject<UPhysicsAsset>(Source, CreatePackage(AssetPath), TEXT("PA_FE_Enemy"));
			Asset->SetFlags(RF_Public | RF_Standalone);
		}
		Asset->Modify();
		int32 RootIndex = Asset->FindBodyIndex(TEXT("root"));
		if (RootIndex == INDEX_NONE)
		{
			USkeletalBodySetup* Root = NewObject<USkeletalBodySetup>(Asset, NAME_None, RF_Transactional);
			Root->BoneName = TEXT("root");
			Root->AggGeom.SphereElems.Add(FKSphereElem(1.0f));
			RootIndex = Asset->SkeletalBodySetups.Add(Root);
		}
		USkeletalBodySetup* Root = Asset->SkeletalBodySetups[RootIndex];
		Root->PhysicsType = PhysType_Kinematic;
		Root->CollisionReponse = EBodyCollisionResponse::BodyCollision_Disabled;
		Root->bConsiderForBounds = false;
		// No root/pelvis constraint: this is a coordinate anchor, not a tether for the falling pelvis.
		for (int32 Index = 0; Index < Asset->SkeletalBodySetups.Num(); ++Index)
		{
			if (Index != RootIndex) { Asset->DisableCollision(RootIndex, Index); }
		}
		Asset->UpdateBodySetupIndexMap();
		Asset->UpdateBoundsBodiesArray();
		Asset->PostEditChange();
		if (!Save(Asset)) { return false; }
		for (const FString& Path : EnemyPaths)
		{
			UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *Path);
			if (!TestNotNull(TEXT("Enemy BP ") + Path, Blueprint)) { return false; }
			ACharacter* Default = CastChecked<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject());
			Blueprint->Modify();
			Default->GetMesh()->Modify();
			Default->GetMesh()->SetPhysicsAsset(Asset, false);
			FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
			if (!Save(Blueprint)) { return false; }
		}
		for (const FString& Path : MontagePaths)
		{
			UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, *Path);
			if (!TestNotNull(TEXT("Get-up montage ") + Path, Montage)) { return false; }
			Montage->Modify();
			// ALS starts the get-up clip immediately beneath the snapshot, not blended from standing idle.
			Montage->BlendIn.SetBlendTime(0.0f);
			if (!Save(Montage)) { return false; }
		}
	}
	if (!TestNotNull(TEXT("Dedicated enemy Physics Asset (run explicit setup once if absent)"), Asset)) { return false; }
	const int32 RootIndex = Asset->FindBodyIndex(TEXT("root"));
	if (!TestTrue(TEXT("Root body exists"), RootIndex != INDEX_NONE)) { return false; }
	const USkeletalBodySetup* Root = Asset->SkeletalBodySetups[RootIndex];
	TestEqual(TEXT("Root cannot simulate"), Root->PhysicsType.GetValue(), PhysType_Kinematic);
	TestEqual(TEXT("Anchor has no collision"), Root->CollisionReponse.GetValue(), EBodyCollisionResponse::BodyCollision_Disabled);
	TestTrue(TEXT("Pelvis retained"), Asset->FindBodyIndex(TEXT("pelvis")) != INDEX_NONE);
	for (const FString& Path : EnemyPaths)
	{
		const UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *Path);
		if (!TestNotNull(TEXT("Configured enemy ") + Path, Blueprint)) { return false; }
		const ACharacter* Default = CastChecked<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject());
		TestEqual(TEXT("Enemy overrides shared mannequin Physics Asset: ") + Path, Default->GetMesh()->GetPhysicsAsset(), Asset);
	}
	for (const FString& Path : MontagePaths)
	{
		const UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, *Path);
		if (!TestNotNull(TEXT("Configured get-up montage ") + Path, Montage)) { return false; }
		TestEqual(TEXT("No standing-idle blend-in: ") + Path, Montage->BlendIn.GetBlendTime(), 0.0f);
	}
	return true;
}

#endif
