#include "Combat/Item/FEEquipmentInstanceData.h"

bool FFE_WeaponInstanceData::IsValid() const
{
	const FFE_WeaponStat& Stats = ResolveStats();
	if (!FMath::IsFinite(Stats.Offense) || Stats.Offense < 0.0f ||
		!FMath::IsFinite(Stats.Accuracy) || Stats.Accuracy < 0.0f || Stats.Accuracy > 1.0f ||
		!FMath::IsFinite(Stats.RecoilControl) || Stats.RecoilControl < 0.0f || Stats.RecoilControl > 1.0f) { return false; }
	if (bOverrideAttachments)
	{
		for (const auto& Attachment : Attachments)
		{
			if (Attachment.Definition.IsNull() ||
				(Attachment.bOverrideAccuracyBonus && !FMath::IsFinite(Attachment.AccuracyBonus))) { return false; }
		}
	}
	return true;
}

bool FFE_ArmorInstanceData::IsValid() const
{
	const FFE_ArmorStat& Stats = ResolveStats();
	return FMath::IsFinite(Stats.Defense) && Stats.Defense >= 0.0f &&
		FMath::IsFinite(Stats.KnockbackResistance) && Stats.KnockbackResistance >= 0.0f;
}
