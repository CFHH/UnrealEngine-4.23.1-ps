// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class WebRTCProxy : ModuleRules
{
	public WebRTCProxy(ReadOnlyTargetRules Target) : base(Target)
	{
		var EngineDir = Path.GetFullPath(Target.RelativeEnginePath);
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core"
			}
		);
		
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PrivateIncludePaths.Add(Path.Combine(EngineDir, "Source/ThirdParty/WebRTC/rev.23789/include/Win64/VS2017"));
			PrivateIncludePaths.Add(Path.Combine(EngineDir, "Source/ThirdParty/WebRTC/rev.23789/include/Win64/VS2017/third_party/jsoncpp/source/include"));
			
			PublicLibraryPaths.Add(Path.Combine(EngineDir, "Source/ThirdParty/WebRTC/rev.23789/lib/Win64/VS2017/release"));
			
			PublicAdditionalLibraries.Add("json.lib");
			PublicAdditionalLibraries.Add("webrtc.lib");
			PublicAdditionalLibraries.Add("webrtc_opus.lib");
			PublicAdditionalLibraries.Add("audio_decoder_opus.lib");
			PublicAdditionalLibraries.Add("Msdmo.lib");
			PublicAdditionalLibraries.Add("Dmoguids.lib");
			PublicAdditionalLibraries.Add("wmcodecdspuuid.lib");
			PublicAdditionalLibraries.Add("Secur32.lib");
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix) && Target.Architecture.StartsWith("x86_64"))
		{
			PrivateIncludePaths.Add(Path.Combine(EngineDir, "Source/ThirdParty/WebRTC/sdk_trunk_linux/include"));
			PrivateIncludePaths.Add(Path.Combine(EngineDir, "Source/ThirdParty/WebRTC/sdk_trunk_linux/include/third_party"));
			
			PublicDefinitions.Add("ABSL_ALLOCATOR_NOTHROW=0");
			PublicDefinitions.Add("_Printf_format_string_=");
			PublicDefinitions.Add("__forceinline=");
			
			AddEngineThirdPartyPrivateStaticDependencies(Target, "WebRTC");
		}
		
		bEnableExceptions = true;
	}
}
