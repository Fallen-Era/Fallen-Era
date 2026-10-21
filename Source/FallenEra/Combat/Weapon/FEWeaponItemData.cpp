#include "Combat/Weapon/FEWeaponItemData.h"
#include "Serialization/Archive.h"

void UFE_WeaponItemData::Serialize(FArchive& Ar)
{
	if (Ar.IsSaving()) { ItemDataVersion = 1; }
	Super::Serialize(Ar);
}

void UFE_WeaponItemData::PostLoad()
{
	Super::PostLoad();
	if (!NeedsTemplateMigration()) { return; }
	Template.WeaponStat = WeaponStat;
	Template.ItemTags = ItemTags;
	Template.WeaponTags = WeaponTags;
	Template.Mesh = Mesh;
	Template.SkeletalMesh = SkeletalMesh;
	Template.WeaponAnimLayerClass = WeaponAnimLayerClass;
	Template.CrosshairWidgetClass = CrosshairWidgetClass;
	Template.WeaponMeshAnimInstanceClass = WeaponMeshAnimInstanceClass;
	Template.AttackSound = AttackSound;
	Template.AttackSoundVolume = AttackSoundVolume;
	Template.AttackSoundPitch = AttackSoundPitch;
	Template.AttackNoiseLoudness = AttackNoiseLoudness;
	Template.AttackNoiseMaxRange = AttackNoiseMaxRange;
	Template.ImpactEffectData = ImpactEffectData;
	Template.AttachSocketName = AttachSocketName;
	Template.SocketOffset = SocketOffset;
	Template.FirstPersonMeshTransform = FirstPersonMeshTransform;
	Template.DefaultDamageEffect = DefaultDamageEffect;
	Template.AttackActions = AttackActions;
	Instance.ItemData = WeaponStat;
	ItemDataVersion = 1;
}

void UFE_MeleeWeaponItemData::PostLoad()
{
	const bool bMigrate = NeedsTemplateMigration();
	Super::PostLoad();
	if (bMigrate) { Template.AttackSequenceMode = AttackSequenceMode; }
}

void UFE_RangedWeaponItemData::PostLoad()
{
	const bool bMigrate = NeedsTemplateMigration();
	Super::PostLoad();
	if (!bMigrate) { return; }
	Template.AimSwaySettings = AimSwaySettings;
	Template.bSupportsAiming = bSupportsAiming;
	Template.DefaultAttachments = DefaultAttachments;
	Template.DefaultSight = DefaultSight;
	Template.MuzzleNiagaraSystem = MuzzleNiagaraSystem;
	Template.MuzzleParticleSystem = MuzzleParticleSystem;
	Template.MuzzleNiagaraScale = MuzzleNiagaraScale;
}

const UFE_WeaponAttackData* UFE_WeaponItemData::FindAttackForInputTag(FGameplayTag InInputTag) const
{
	if (!InInputTag.IsValid())
	{
		return nullptr;
	}

	for (const UFE_WeaponAttackData* AttackAction : Template.AttackActions)
	{
		if (AttackAction && AttackAction->InputTag.MatchesTagExact(InInputTag))
		{
			return AttackAction;
		}
	}

	return nullptr;
}

bool UFE_WeaponItemData::HasItemTag(FGameplayTag InItemTag) const
{
	return InItemTag.IsValid() && Template.ItemTags.HasTagExact(InItemTag);
}
