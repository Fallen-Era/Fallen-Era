#include "HT/Weapon/WeaponItemData.h"

const UFE_WeaponAttackData* UFE_WeaponItemData::FindAttackForInputTag(FGameplayTag InInputTag) const
{
	if (!InInputTag.IsValid())
	{
		return nullptr;
	}

	for (const UFE_WeaponAttackData* AttackAction : AttackActions)
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
	return InItemTag.IsValid() && ItemTags.HasTagExact(InItemTag);
}
