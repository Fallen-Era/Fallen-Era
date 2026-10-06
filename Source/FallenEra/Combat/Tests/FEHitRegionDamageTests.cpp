#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/FallenEraAttributeSet.h"
#include "Combat/Component/FECombatComponent.h"
#include "Combat/Damage/FEHitZoneMappingData.h"
#include "Combat/Damage/FEHitZoneMultiplierData.h"
#include "Combat/GameplayEffect/FEDamageGameplayEffect.h"
#include "Combat/GameplayEffect/FEDamageExecutionCalculation.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "ReferenceSkeleton.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#include "UObject/UnrealType.h"
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFE_HitRegionDamageTest, "FallenEra.Combat.Damage.HitRegions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFE_HitRegionDamageTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Test world"), World)) { return false; }
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ACharacter* Target = World->SpawnActor<ACharacter>();
	ACharacter* Source = World->SpawnActor<ACharacter>(FVector(1000.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
	auto CreateASC = [](ACharacter* Character)
	{
		UAbilitySystemComponent* ASC = NewObject<UAbilitySystemComponent>(Character);
		ASC->RegisterComponent();
		ASC->InitializeComponent();
		ASC->AddAttributeSetSubobject(NewObject<UFallenEraAttributeSet>(Character));
		ASC->InitAbilityActorInfo(Character, Character);
		return ASC;
	};
	UAbilitySystemComponent* SourceASC = CreateASC(Source);
	UAbilitySystemComponent* TargetASC = CreateASC(Target);
	SourceASC->SetNumericAttributeBase(UFallenEraAttributeSet::GetAttackPowerAttribute(), 20.0f);
	TargetASC->SetNumericAttributeBase(UFallenEraAttributeSet::GetDefensePowerAttribute(), 5.0f);
	UFE_CombatComponent* Combat = NewObject<UFE_CombatComponent>(Target);
	Combat->RegisterComponent();
	UFE_HitZoneMappingData* Mapping = NewObject<UFE_HitZoneMappingData>();
	Mapping->BoneZones.Add(TEXT("head"), EFE_HitZone::Head);
	Mapping->BoneZones.Add(TEXT("neck_01"), EFE_HitZone::Head);
	Mapping->BoneZones.Add(TEXT("neck_02"), EFE_HitZone::Head);
	Mapping->BoneZones.Add(TEXT("spine_01"), EFE_HitZone::Torso);
	Mapping->BoneZones.Add(TEXT("upperarm_l"), EFE_HitZone::Arms);
	Mapping->BoneZones.Add(TEXT("thigh_l"), EFE_HitZone::Legs);
	Mapping->BoneZones.Add(TEXT("thigh_r"), EFE_HitZone::Legs);
	UFE_HitZoneMultiplierData* Multipliers = NewObject<UFE_HitZoneMultiplierData>();
	Multipliers->ZoneMultipliers.Add(EFE_HitZone::Head, 2.0f);
	Multipliers->ZoneMultipliers.Add(EFE_HitZone::Torso, 1.0f);
	Multipliers->ZoneMultipliers.Add(EFE_HitZone::Arms, 0.8f);
	Multipliers->ZoneMultipliers.Add(EFE_HitZone::Legs, 0.5f);
	Multipliers->ZoneMultipliers.Add(EFE_HitZone::Default, 99.0f);
	Combat->HitZoneMappingData = Mapping;
	Combat->HitZoneMultiplierData = Multipliers;
	Combat->RefreshHitZoneData();
	TestTrue(TEXT("Loaded hit-zone asset is ready"), Combat->IsHitZoneDataReady());
	TestTrue(TEXT("Rules cached by reference, not copied"), Combat->GetLoadedHitZoneMappingData() == Mapping);
	TestNull(TEXT("Unknown bone has no rule"), Mapping->FindHitZone(TEXT("unknown_bone")));
	UFE_CombatComponent* OtherCombat = NewObject<UFE_CombatComponent>(Source);
	OtherCombat->RegisterComponent();
	OtherCombat->HitZoneMappingData = Mapping;
	OtherCombat->HitZoneMultiplierData = Multipliers;
	OtherCombat->RefreshHitZoneData();
	TestTrue(TEXT("Multiple receivers share exactly one mapping"), OtherCombat->GetLoadedHitZoneMappingData() == Combat->GetLoadedHitZoneMappingData());
	TArray<FSoftObjectPath> PreloadPaths;
	Combat->GatherHitZoneAssetPaths(PreloadPaths);
	TestTrue(TEXT("Spawner initial preparation includes mapping"), PreloadPaths.Contains(Mapping));
	TestTrue(TEXT("Spawner initial preparation includes multipliers"), PreloadPaths.Contains(Multipliers));
	PreloadPaths.Reset();
	Combat->GatherHitZoneAssetPaths(PreloadPaths);
	TestEqual(TEXT("Spawner gathers exactly the two configured profiles"), PreloadPaths.Num(), 2);
#if WITH_EDITOR
	FDataValidationContext ValidContext;
	TestTrue(TEXT("Valid hit-zone rules pass asset validation"), Mapping->IsDataValid(ValidContext) == EDataValidationResult::Valid);
#endif
	auto Apply = [&](FName Bone, EFE_DamageHitType HitType = EFE_DamageHitType::Direct, bool bHasHit = true)
	{
		TargetASC->SetNumericAttributeBase(UFallenEraAttributeSet::GetHealthAttribute(), 100.0f);
		FFE_CombatDamageRequest Request;
		Request.SourceActor = Source;
		Request.TargetActor = Target;
		Request.DamageEffectClass = UFE_DamageGameplayEffect::StaticClass();
		Request.HitType = HitType;
		Request.bHasHitResult = bHasHit;
		Request.HitResult.BoneName = Bone;
		return Combat->ApplyGameplayEffectDamage(Request);
	};
	const UFE_DamageExecutionCalculation* Calculation = GetDefault<UFE_DamageExecutionCalculation>();
	auto CheckDamage = [&](const TCHAR* Label, const FFE_CombatDamageResult& DamageResult, float Multiplier)
	{
		TestTrue(FString(Label) + TEXT(" accepted"), DamageResult.bDamageApplied);
		TestEqual(FString(Label) + TEXT(" region multiplier"), DamageResult.HitRegionMultiplier, Multiplier);
		const float Minimum = Calculation->CalculateDamage(20.0f, 5.0f, Multiplier, -1.0f);
		const float Maximum = Calculation->CalculateDamage(20.0f, 5.0f, Multiplier, 1.0f);
		TestTrue(FString(Label) + TEXT(" within variance range"),
			DamageResult.AppliedDamage >= Minimum - KINDA_SMALL_NUMBER && DamageResult.AppliedDamage <= Maximum + KINDA_SMALL_NUMBER);
	};
	FFE_CombatDamageResult Result = Apply(TEXT("head"));
	TestTrue(TEXT("Direct hit accepted"), Result.bDamageApplied);
	TestEqual(TEXT("Head region selected"), Result.HitRegion, FName(TEXT("Head")));
	CheckDamage(TEXT("Head damage"), Result, 2.0f);
	TestEqual(TEXT("HP reflects head multiplier"), TargetASC->GetNumericAttribute(UFallenEraAttributeSet::GetHealthAttribute()), 100.0f - Result.AppliedDamage);
	CheckDamage(TEXT("Multiple bones can share one region"), Apply(TEXT("neck_01")), 2.0f);
	CheckDamage(TEXT("Leg multiplier halves post-defense damage"), Apply(TEXT("thigh_l")), 0.5f);
	CheckDamage(TEXT("Unconfigured bone defaults to x1"), Apply(TEXT("spine_02")), 1.0f);
	CheckDamage(TEXT("No bone defaults to x1"), Apply(NAME_None), 1.0f);
	CheckDamage(TEXT("No actual hit result ignores stale bone"), Apply(TEXT("head"), EFE_DamageHitType::Direct, false), 1.0f);
	Result = Apply(TEXT("head"), EFE_DamageHitType::Area);
	CheckDamage(TEXT("Area ignores even a valid head bone"), Result, 1.0f);
	TestEqual(TEXT("Area debug classification"), Result.HitRegion, FName(TEXT("Area")));
	TestEqual(TEXT("Area multiplier is always one"), Result.HitRegionMultiplier, 1.0f);
	TSet<float> ObservedDamage;
	for (int32 Index = 0; Index < 64; ++Index)
	{
		Result = Apply(TEXT("head"));
		CheckDamage(TEXT("Repeated GE head hit"), Result, 2.0f);
		ObservedDamage.Add(Result.AppliedDamage);
	}
	TestTrue(TEXT("Actual GE execution produces varied Health loss"), ObservedDamage.Num() > 11);
	// Direct GE callers also receive variance, with x1 when no region is supplied.
	TargetASC->SetNumericAttributeBase(UFallenEraAttributeSet::GetHealthAttribute(), 100.0f);
	const FGameplayEffectSpecHandle BaseDamageSpec = SourceASC->MakeOutgoingSpec(
		UFE_DamageGameplayEffect::StaticClass(), 1.0f, SourceASC->MakeEffectContext());
	TargetASC->ApplyGameplayEffectSpecToSelf(*BaseDamageSpec.Data.Get());
	const float RawHealth = TargetASC->GetNumericAttribute(UFallenEraAttributeSet::GetHealthAttribute());
	TestTrue(TEXT("Missing SetByCaller multiplier defaults to one"),
		RawHealth >= 100.0f - Calculation->CalculateDamage(20.0f, 5.0f, 1.0f, 1.0f) - KINDA_SMALL_NUMBER &&
		RawHealth <= 100.0f - Calculation->CalculateDamage(20.0f, 5.0f, 1.0f, -1.0f) + KINDA_SMALL_NUMBER);
	Multipliers->ZoneMultipliers[EFE_HitZone::Head] = 0.0f;
	TestEqual(TEXT("Zero multiplier produces no HP damage"), Apply(TEXT("head")).AppliedDamage, 0.0f);
	Multipliers->ZoneMultipliers[EFE_HitZone::Head] = -2.0f;
	TestEqual(TEXT("Negative multiplier never heals"), Apply(TEXT("head")).AppliedDamage, 0.0f);
#if WITH_EDITOR
	FDataValidationContext InvalidContext;
	TestTrue(TEXT("Negative multiplier fails editor asset validation"), Multipliers->IsDataValid(InvalidContext) == EDataValidationResult::Invalid);
#endif
	FName ResolvedRegion;
	TestEqual(TEXT("Empty bones are never inferred from impact position"),
		Combat->ResolveHitRegionMultiplier(NAME_None, EFE_DamageHitType::Direct, ResolvedRegion), 1.0f);
	Multipliers->ZoneMultipliers[EFE_HitZone::Head] = 2.0f;
	Combat->HitZoneMappingData = nullptr;
	Combat->HitZoneMultiplierData = nullptr;
	Combat->RefreshHitZoneData();
	TestTrue(TEXT("No profile remains backward compatible"), Combat->IsHitZoneDataReady());
	CheckDamage(TEXT("No profile uses normal damage"), Apply(TEXT("head")), 1.0f);
	// Simulate a configured but not-yet-loaded asset without issuing an IO request for a fake file.
	Combat->HitZoneMappingData = TSoftObjectPtr<UFE_HitZoneMappingData>(FSoftObjectPath(TEXT("/Game/Tests/UnloadedHitZones.UnloadedHitZones")));
	Combat->HitZoneMultiplierData = Multipliers;
	TestFalse(TEXT("Configured unloaded profile is not ready"), Combat->IsHitZoneDataReady());
	TestFalse(TEXT("No incorrect x1 damage while zone data is loading"), Apply(TEXT("head")).bDamageApplied);
	CheckDamage(TEXT("Area damage is independent of zone readiness"), Apply(TEXT("head"), EFE_DamageHitType::Area), 1.0f);
	Combat->HitZoneMappingData = Mapping;
	Combat->RefreshHitZoneData();
	CheckDamage(TEXT("Profile restoration resumes head damage"), Apply(TEXT("head")), 2.0f);

	TestTrue(TEXT("Split profiles ready"), Combat->IsHitZoneDataReady());
	CheckDamage(TEXT("Split mapping resolves head"), Apply(TEXT("head")), 2.0f);
	CheckDamage(TEXT("Neck01 uses Head zone"), Apply(TEXT("neck_01")), 2.0f);
	CheckDamage(TEXT("Neck02 uses Head zone"), Apply(TEXT("neck_02")), 2.0f);
	Multipliers->ZoneMultipliers[EFE_HitZone::Head] = 3.0f;
	CheckDamage(TEXT("One multiplier change affects every Head bone"), Apply(TEXT("neck_02")), 3.0f);
	CheckDamage(TEXT("Arm zone damage"), Apply(TEXT("upperarm_l")), 0.8f);
	CheckDamage(TEXT("Unmapped bones ignore even a configured Default multiplier"), Apply(TEXT("unknown_bone")), 1.0f);
	Multipliers->ZoneMultipliers.Remove(EFE_HitZone::Arms);
	CheckDamage(TEXT("Missing zone multiplier defaults to one"), Apply(TEXT("upperarm_l")), 1.0f);
	CheckDamage(TEXT("Split rules do not affect grenades"), Apply(TEXT("head"), EFE_DamageHitType::Area), 1.0f);
	UFE_HitZoneMultiplierData* AlternateMultipliers = NewObject<UFE_HitZoneMultiplierData>();
	AlternateMultipliers->ZoneMultipliers.Add(EFE_HitZone::Head, 4.0f);
	OtherCombat->HitZoneMappingData = Mapping;
	OtherCombat->HitZoneMultiplierData = AlternateMultipliers;
	OtherCombat->RefreshHitZoneData();
	TestTrue(TEXT("Different monsters share a bone mapping without copying"), OtherCombat->GetLoadedHitZoneMappingData() == Mapping);
	TestEqual(TEXT("Shared skeleton can use independent damage tuning"),
		OtherCombat->ResolveHitRegionMultiplier(TEXT("neck_02"), EFE_DamageHitType::Direct, ResolvedRegion), 4.0f);
	CheckDamage(TEXT("Changing another monster profile does not alter this profile"), Apply(TEXT("head")), 3.0f);
	PreloadPaths.Reset();
	Combat->GatherHitZoneAssetPaths(PreloadPaths);
	TestTrue(TEXT("Mapping is preloaded before spawn"), PreloadPaths.Contains(Mapping));
	TestTrue(TEXT("Multiplier profile is preloaded before spawn"), PreloadPaths.Contains(Multipliers));
	TestEqual(TEXT("Only mapping and multipliers are gathered"), PreloadPaths.Num(), 2);
	Combat->HitZoneMultiplierData = TSoftObjectPtr<UFE_HitZoneMultiplierData>(FSoftObjectPath(TEXT("/Game/Tests/UnloadedMultipliers.UnloadedMultipliers")));
	TestFalse(TEXT("Both split assets must be ready when configured"), Combat->IsHitZoneDataReady());
	TestFalse(TEXT("Loading multiplier cannot produce unintended x1 direct damage"), Apply(TEXT("head")).bDamageApplied);
	CheckDamage(TEXT("Area damage bypasses multiplier asset loading"), Apply(TEXT("head"), EFE_DamageHitType::Area), 1.0f);
	Combat->HitZoneMultiplierData = nullptr;
	Combat->RefreshHitZoneData();
	CheckDamage(TEXT("No multiplier asset means one"), Apply(TEXT("head")), 1.0f);
	Combat->HitZoneMultiplierData = Multipliers;
	Combat->RefreshHitZoneData();
#if WITH_EDITOR
	FDataValidationContext MappingContext;
	FDataValidationContext MultiplierContext;
	TestTrue(TEXT("Valid mappings pass validation"), Mapping->IsDataValid(MappingContext) == EDataValidationResult::Valid);
	TestTrue(TEXT("Valid zone multipliers pass validation"), Multipliers->IsDataValid(MultiplierContext) == EDataValidationResult::Valid);
	Multipliers->ZoneMultipliers[EFE_HitZone::Head] = -1.0f;
	FDataValidationContext InvalidMultiplierContext;
	TestTrue(TEXT("Negative split multiplier is rejected by validation"), Multipliers->IsDataValid(InvalidMultiplierContext) == EDataValidationResult::Invalid);
	TestEqual(TEXT("Invalid negative split multiplier never heals"), Apply(TEXT("head")).AppliedDamage, 0.0f);
	Multipliers->ZoneMultipliers[EFE_HitZone::Head] = 3.0f;
#endif
	// Reference-only meshes keep these hierarchy tests independent of rendering and project assets.
	auto CreateHierarchyMesh = [](bool bHeadUnderArm)
	{
		USkeletalMesh* Mesh = NewObject<USkeletalMesh>();
		Mesh->SetSkeleton(NewObject<USkeleton>(Mesh));
		FReferenceSkeletonModifier Modifier(Mesh->GetRefSkeleton(), nullptr);
		Modifier.Add(FMeshBoneInfo(TEXT("root"), TEXT("root"), INDEX_NONE), FTransform::Identity);
		Modifier.Add(FMeshBoneInfo(TEXT("spine_01"), TEXT("spine_01"), 0), FTransform::Identity);
		Modifier.Add(FMeshBoneInfo(TEXT("neck_01"), TEXT("neck_01"), 1), FTransform::Identity);
		Modifier.Add(FMeshBoneInfo(TEXT("neck_02"), TEXT("neck_02"), 2), FTransform::Identity);
		Modifier.Add(FMeshBoneInfo(TEXT("upperarm_l"), TEXT("upperarm_l"), 1), FTransform::Identity);
		Modifier.Add(FMeshBoneInfo(TEXT("head"), TEXT("head"), bHeadUnderArm ? 4 : 3), FTransform::Identity);
		Modifier.Add(FMeshBoneInfo(TEXT("jaw"), TEXT("jaw"), 5), FTransform::Identity);
		Modifier.Add(FMeshBoneInfo(TEXT("finger_l"), TEXT("finger_l"), 4), FTransform::Identity);
		Modifier.Add(FMeshBoneInfo(TEXT("thigh_l"), TEXT("thigh_l"), 0), FTransform::Identity);
		Modifier.Add(FMeshBoneInfo(TEXT("calf_l"), TEXT("calf_l"), 8), FTransform::Identity);
		return Mesh;
	};
	USkeletalMesh* HierarchyMesh = CreateHierarchyMesh(false);
	Target->GetMesh()->UnregisterComponent();
	Source->GetMesh()->UnregisterComponent();
	Target->GetMesh()->SetSkeletalMeshAsset(HierarchyMesh);
	Source->GetMesh()->SetSkeletalMeshAsset(HierarchyMesh);
	UFE_HitZoneMappingData* HierarchyMapping = NewObject<UFE_HitZoneMappingData>();
	HierarchyMapping->BoneZones.Add(TEXT("neck_01"), EFE_HitZone::Head);
	HierarchyMapping->BoneZones.Add(TEXT("spine_01"), EFE_HitZone::Torso);
	HierarchyMapping->BoneZones.Add(TEXT("upperarm_l"), EFE_HitZone::Arms);
	HierarchyMapping->BoneZones.Add(TEXT("thigh_l"), EFE_HitZone::Legs);
	TestFalse(TEXT("Hierarchy cache is not built before preparation"), HierarchyMapping->IsPreparedForMesh(HierarchyMesh));
	Combat->HitZoneMappingData = HierarchyMapping;
	Combat->RefreshHitZoneData();
	TestTrue(TEXT("Refresh prepares hierarchy before damage"), Combat->IsHitZoneDataReady());
	CheckDamage(TEXT("Head inherits the closest registered neck ancestor"), Apply(TEXT("head")), 3.0f);
	CheckDamage(TEXT("Intermediate neck inherits Head"), Apply(TEXT("neck_02")), 3.0f);
	CheckDamage(TEXT("Deep descendant inherits through multiple bones"), Apply(TEXT("jaw")), 3.0f);
	CheckDamage(TEXT("Leg descendants inherit Legs"), Apply(TEXT("calf_l")), 0.5f);
	CheckDamage(TEXT("Unmapped root remains x1"), Apply(TEXT("root")), 1.0f);
	CheckDamage(TEXT("Nonexistent bones remain x1"), Apply(TEXT("not_in_skeleton")), 1.0f);
	CheckDamage(TEXT("Area bypasses inherited head rules"), Apply(TEXT("jaw"), EFE_DamageHitType::Area), 1.0f);
	const EFE_HitZone* SharedHeadRule = HierarchyMapping->FindHitZone(TEXT("head"), HierarchyMesh);
	OtherCombat->HitZoneMappingData = HierarchyMapping;
	OtherCombat->RefreshHitZoneData();
	TestTrue(TEXT("Another receiver reuses the same cached rule without rebuilding"),
		HierarchyMapping->FindHitZone(TEXT("head"), HierarchyMesh) == SharedHeadRule);
	TestEqual(TEXT("Shared inherited mapping still supports independent multipliers"),
		OtherCombat->ResolveHitRegionMultiplier(TEXT("head"), EFE_DamageHitType::Direct, ResolvedRegion), 4.0f);
	UFE_HitZoneMappingData* OverrideMapping = NewObject<UFE_HitZoneMappingData>();
	OverrideMapping->BoneZones = HierarchyMapping->BoneZones;
	OverrideMapping->BoneZones.Add(TEXT("head"), EFE_HitZone::Legs);
	Combat->HitZoneMappingData = OverrideMapping;
	Combat->RefreshHitZoneData();
	CheckDamage(TEXT("Explicit child overrides ancestor"), Apply(TEXT("head")), 0.5f);
	CheckDamage(TEXT("Child override propagates to its own descendants"), Apply(TEXT("jaw")), 0.5f);
	CheckDamage(TEXT("Child override does not alter its parent"), Apply(TEXT("neck_02")), 3.0f);
	Combat->HitZoneMappingData = HierarchyMapping;
	Combat->RefreshHitZoneData();
	USkeletalMesh* AlternateHierarchyMesh = CreateHierarchyMesh(true);
	Target->GetMesh()->SetSkeletalMeshAsset(AlternateHierarchyMesh);
	TestFalse(TEXT("A different unprepared mesh is not ready"), Combat->IsHitZoneDataReady());
	TestFalse(TEXT("Stale hierarchy is never used after mesh replacement"), Apply(TEXT("head")).bDamageApplied);
	CheckDamage(TEXT("Area damage is independent of mesh cache readiness"), Apply(TEXT("head"), EFE_DamageHitType::Area), 1.0f);
	Combat->RefreshHitZoneData();
	TestTrue(TEXT("Refresh prepares the replacement mesh"), Combat->IsHitZoneDataReady());
	TestEqual(TEXT("Same bone names on another mesh use its own parent hierarchy"), Apply(TEXT("head")).HitRegion, FName(TEXT("Arms")));
	TestEqual(TEXT("Preparing another mesh does not change the original mesh cache"),
		OtherCombat->ResolveHitRegionMultiplier(TEXT("head"), EFE_DamageHitType::Direct, ResolvedRegion), 4.0f);
	Target->GetMesh()->SetSkeletalMeshAsset(HierarchyMesh);
	Combat->RefreshHitZoneData();
#if WITH_EDITOR
	FPropertyChangedEvent MappingEdited(nullptr);
	HierarchyMapping->PostEditChangeProperty(MappingEdited);
	TestFalse(TEXT("Editing mappings invalidates the old hierarchy cache"), Combat->IsHitZoneDataReady());
	Combat->RefreshHitZoneData();
	TestTrue(TEXT("Refresh rebuilds the invalidated hierarchy cache"), Combat->IsHitZoneDataReady());
	CheckDamage(TEXT("Inherited damage resumes after rebuilding"), Apply(TEXT("jaw")), 3.0f);
#endif
	TargetASC->SetNumericAttributeBase(UFallenEraAttributeSet::GetDefensePowerAttribute(), 25.0f);
	TestEqual(TEXT("Fully blocked attack stays zero even on head"), Apply(TEXT("head")).AppliedDamage, 0.0f);
	TargetASC->SetNumericAttributeBase(UFallenEraAttributeSet::GetDefensePowerAttribute(), 5.0f);
	TargetASC->SetNumericAttributeBase(UFallenEraAttributeSet::GetHealthAttribute(), 10.0f);
	FFE_CombatDamageRequest LethalRequest;
	LethalRequest.SourceActor = Source;
	LethalRequest.TargetActor = Target;
	LethalRequest.DamageEffectClass = UFE_DamageGameplayEffect::StaticClass();
	LethalRequest.bHasHitResult = true;
	LethalRequest.HitResult.BoneName = TEXT("head");
	TestEqual(TEXT("Debug damage reports actual HP lost, not overkill"), Combat->ApplyGameplayEffectDamage(LethalRequest).AppliedDamage, 10.0f);
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

#endif
