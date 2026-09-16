// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class FallenEra : ModuleRules
{
	public FallenEra(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"AIModule",
			"Core",
			"CoreUObject",
			"Engine",
			"EnhancedInput",
			"InputCore",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
			"GameplayStateTreeModule",
			"Niagara",
			"StateTreeModule",
			"Slate",
			"UMG"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"FallenEra",
			"FallenEra/AbilitySystem",
			"FallenEra/AbilitySystem/Abilities",
			"FallenEra/AbilitySystem/Attributes",
			"FallenEra/GameplayTag",
			"FallenEra/ItemDatas",
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
