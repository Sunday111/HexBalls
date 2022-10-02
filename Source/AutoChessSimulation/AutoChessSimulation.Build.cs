// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AutoChessSimulation : ModuleRules
{
	public AutoChessSimulation(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "HeadMountedDisplay" });
	}
}
