// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class FallenEra : ModuleRules
{
	public FallenEra(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
			"AIModule",
			"NavigationSystem",
			"DeveloperSettings",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"Niagara",
			"UMG",

			"Voxel",
			"VoxelGraph",
			"VoxelCore",
			"ModelViewViewModel",
			"Slate",
			"SlateCore",
			"PhysicsCore"
			"DeveloperSettings"
		});

		PrivateDependencyModuleNames.AddRange(new string[] 
		{
			"GameplayMessageRuntime",
		});

		PublicIncludePaths.AddRange(new string[] {
			"FallenEra",
			"FallenEra/AbilitySystem",
			"FallenEra/AbilitySystem/Abilities",
			"FallenEra/AbilitySystem/Attributes",
			"FallenEra/Building",
			"FallenEra/Interaction",
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
