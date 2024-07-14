// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LiHouOng_BGS_TASK : ModuleRules
{
	public LiHouOng_BGS_TASK(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput" });
	}
}
