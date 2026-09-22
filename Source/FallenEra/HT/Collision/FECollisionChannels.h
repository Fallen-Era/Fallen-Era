#pragma once

#include "Engine/EngineTypes.h"

/**
 * C++ aliases for the project collision channels declared in DefaultEngine.ini.
 * Keep these values synchronized with [/Script/Engine.CollisionProfile].
 */
namespace FECollisionChannels
{
	inline constexpr ECollisionChannel PlayerHitscanTrace = ECC_GameTraceChannel2;
	inline constexpr ECollisionChannel Enemy = ECC_GameTraceChannel4;
}
