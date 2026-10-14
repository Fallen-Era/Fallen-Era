#include "Combat/Weapon/Attachment/FEWeaponAttachmentData.h"

float FFE_WeaponAttachmentInstance::ResolveAccuracyBonus() const
{
	const UFE_WeaponAttachmentData* Data = Definition.Get();
	const float Value = bOverrideAccuracyBonus ? AccuracyBonus : (Data ? Data->AccuracyBonus : 0.0f);
	return FMath::IsFinite(Value) ? FMath::Clamp(Value, -1.0f, 1.0f) : 0.0f;
}
