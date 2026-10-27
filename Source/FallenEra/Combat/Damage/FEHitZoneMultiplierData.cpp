#include "Combat/Damage/FEHitZoneMultiplierData.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

float UFE_HitZoneMultiplierData::GetDamageMultiplier(EFE_HitZone Zone) const
{
	if (const float* Multiplier = ZoneMultipliers.Find(Zone))
	{
		return FMath::IsFinite(*Multiplier) ? FMath::Max(0.0f, *Multiplier) : 1.0f;
	}
	return 1.0f;
}

#if WITH_EDITOR
EDataValidationResult UFE_HitZoneMultiplierData::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	for (const auto& Rule : ZoneMultipliers)
	{
		if (!StaticEnum<EFE_HitZone>()->IsValidEnumValue(static_cast<int64>(Rule.Key)) ||
			!FMath::IsFinite(Rule.Value) || Rule.Value < 0.0f)
		{
			Context.AddError(NSLOCTEXT("FEHitZones", "InvalidZoneMultiplier",
				"Zone multipliers require valid hit zones and finite, non-negative values."));
			Result = EDataValidationResult::Invalid;
		}
	}
	return Result == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : Result;
}
#endif
