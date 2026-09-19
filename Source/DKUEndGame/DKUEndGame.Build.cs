// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DKUEndGame : ModuleRules
{
	public DKUEndGame(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"DKUEndGame",
			"DKUEndGame/Variant_Platforming",
			"DKUEndGame/Variant_Platforming/Animation",
			"DKUEndGame/Variant_Combat",
			"DKUEndGame/Variant_Combat/AI",
			"DKUEndGame/Variant_Combat/Animation",
			"DKUEndGame/Variant_Combat/Gameplay",
			"DKUEndGame/Variant_Combat/Interfaces",
			"DKUEndGame/Variant_Combat/UI",
			"DKUEndGame/Variant_SideScrolling",
			"DKUEndGame/Variant_SideScrolling/AI",
			"DKUEndGame/Variant_SideScrolling/Gameplay",
			"DKUEndGame/Variant_SideScrolling/Interfaces",
			"DKUEndGame/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
