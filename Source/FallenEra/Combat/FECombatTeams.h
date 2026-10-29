#pragma once

#include "GenericTeamAgentInterface.h"

/** Shared team identifiers used by perception and the authoritative damage gate. */
namespace FECombatTeams
{
	inline const FGenericTeamId Player(1);
	inline const FGenericTeamId Enemy(2);

	inline bool AreSameTeam(const AActor* SourceActor, const AActor* TargetActor)
	{
		if (!SourceActor || !TargetActor)
		{
			return false;
		}

		const FGenericTeamId SourceTeam = FGenericTeamId::GetTeamIdentifier(SourceActor);
		const FGenericTeamId TargetTeam = FGenericTeamId::GetTeamIdentifier(TargetActor);
		return SourceTeam.GetId() != FGenericTeamId::NoTeam.GetId()
			&& SourceTeam.GetId() == TargetTeam.GetId();
	}
}
