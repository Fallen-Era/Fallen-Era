#include "Combat/Damage/FEHitZoneMappingData.h"
#include "Engine/SkeletalMesh.h"
#include "ReferenceSkeleton.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

void UFE_HitZoneMappingData::PrepareForMesh(USkeletalMesh* Mesh)
{
	if (!Mesh || ResolvedBoneZonesByMesh.Contains(Mesh)) { return; }
	for (auto It = ResolvedBoneZonesByMesh.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid()) { It.RemoveCurrent(); }
	}
	const FReferenceSkeleton& Skeleton = Mesh->GetRefSkeleton();
	TMap<FName, EFE_HitZone>& ResolvedZones = ResolvedBoneZonesByMesh.Add(Mesh);
	ResolvedZones.Reserve(Skeleton.GetNum());
	// Reference skeleton parents precede children: one linear pass propagates the nearest rule.
	for (int32 BoneIndex = 0; BoneIndex < Skeleton.GetNum(); ++BoneIndex)
	{
		const FName BoneName = Skeleton.GetBoneName(BoneIndex);
		const EFE_HitZone* Zone = BoneZones.Find(BoneName);
		if (!Zone)
		{
			const int32 ParentIndex = Skeleton.GetParentIndex(BoneIndex);
			Zone = ParentIndex != INDEX_NONE ? ResolvedZones.Find(Skeleton.GetBoneName(ParentIndex)) : nullptr;
		}
		if (Zone)
		{
			const EFE_HitZone ResolvedZone = *Zone;
			ResolvedZones.Add(BoneName, ResolvedZone);
		}
	}
}

bool UFE_HitZoneMappingData::IsPreparedForMesh(USkeletalMesh* Mesh) const
{
	return !Mesh || ResolvedBoneZonesByMesh.Contains(Mesh);
}

const EFE_HitZone* UFE_HitZoneMappingData::FindHitZone(FName BoneName, USkeletalMesh* Mesh) const
{
	if (BoneName.IsNone()) { return nullptr; }
	if (const EFE_HitZone* ExactZone = BoneZones.Find(BoneName)) { return ExactZone; }
	const TMap<FName, EFE_HitZone>* ResolvedZones = Mesh ? ResolvedBoneZonesByMesh.Find(Mesh) : nullptr;
	return ResolvedZones ? ResolvedZones->Find(BoneName) : nullptr;
}

FName UFE_HitZoneMappingData::GetZoneName(EFE_HitZone Zone)
{
	switch (Zone)
	{
	case EFE_HitZone::Head: return TEXT("Head");
	case EFE_HitZone::Torso: return TEXT("Torso");
	case EFE_HitZone::Arms: return TEXT("Arms");
	case EFE_HitZone::Legs: return TEXT("Legs");
	default: return TEXT("Default");
	}
}

#if WITH_EDITOR
void UFE_HitZoneMappingData::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	ResolvedBoneZonesByMesh.Reset();
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UFE_HitZoneMappingData::PostEditUndo()
{
	ResolvedBoneZonesByMesh.Reset();
	Super::PostEditUndo();
}

EDataValidationResult UFE_HitZoneMappingData::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	for (const auto& Rule : BoneZones)
	{
		if (Rule.Key.IsNone() || !StaticEnum<EFE_HitZone>()->IsValidEnumValue(static_cast<int64>(Rule.Value)))
		{
			Context.AddError(FText::Format(NSLOCTEXT("FEHitZones", "InvalidBoneZone",
				"Bone rule '{0}' requires a bone name and a valid hit zone."), FText::FromName(Rule.Key)));
			Result = EDataValidationResult::Invalid;
		}
	}
	return Result == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : Result;
}
#endif
