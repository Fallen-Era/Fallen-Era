#include "HT/Impact/ImpactEffectData.h"

const FFE_ImpactEffectEntry* UFE_ImpactEffectData::FindEffectForSurface(EPhysicalSurface SurfaceType) const
{
	for (const FFE_ImpactEffectEntry& Entry : SurfaceEffects)
	{
		if (Entry.SurfaceType == SurfaceType)
		{
			return &Entry;
		}
	}

	return nullptr;
}
