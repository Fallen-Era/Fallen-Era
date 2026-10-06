#pragma once

#include "Engine/EngineTypes.h"

/**
 * C++ aliases for the project collision channels declared in DefaultEngine.ini.
 * Keep these values synchronized with [/Script/Engine.CollisionProfile].
 */
namespace FECollisionChannels
{
	inline constexpr ECollisionChannel PlayerHitscanTrace = ECC_GameTraceChannel6;
	inline constexpr ECollisionChannel Player = ECC_GameTraceChannel3;
	inline constexpr ECollisionChannel Enemy = ECC_GameTraceChannel4;
	inline constexpr ECollisionChannel EnemyTrace = ECC_GameTraceChannel5;
}
