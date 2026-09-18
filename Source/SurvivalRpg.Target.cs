// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;
using System.Collections.Generic;
using System;
using System.IO;
using System.Security.Cryptography;
using EpicGames.Core;

public class SurvivalRpgTarget : TargetRules
{
	public SurvivalRpgTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;

		ExtraModuleNames.AddRange( new string[] { "SurvivalRpg" } );
		RpgNetworkPredictionPatch.RequirePrepared(ProjectFile?.Directory.FullName, Version);
	}
}

// The launcher Engine supplies ignored plugin sources. A fresh checkout must prepare
// the versioned correction before building, rather than silently use the affected DLL.
internal static class RpgNetworkPredictionPatch
{
	internal static void RequirePrepared(string ProjectDirectory, ReadOnlyBuildVersion EngineVersion)
	{
		if (EngineVersion.MajorVersion != 5 || EngineVersion.MinorVersion != 8 || EngineVersion.PatchVersion != 2)
		{
			throw new BuildException("Review and update the pinned NetworkPrediction correction before changing from UE 5.8.2.");
		}
		if (String.IsNullOrEmpty(ProjectDirectory))
		{
			throw new BuildException("SurvivalRpg requires an explicit project path to verify its NetworkPrediction patch.");
		}
		string PatchDirectory = Path.Combine(ProjectDirectory, "Build", "Patches", "NetworkPrediction");
		string ManifestPath = Path.Combine(PatchDirectory, "manifest.json");
		try
		{
			byte[] ManifestBytes = File.ReadAllBytes(ManifestPath);
			string ManifestHash = Convert.ToHexString(SHA256.HashData(ManifestBytes));
			JsonObject Manifest = JsonObject.Read(new FileReference(ManifestPath));
			JsonObject Plugins = Manifest.GetObjectField("plugins");
			foreach (string PluginName in Plugins.KeyNames)
			{
				string Root = Path.Combine(ProjectDirectory, "Plugins", PluginName);
				JsonObject Marker = JsonObject.Read(new FileReference(Path.Combine(Root, ".survival-rpg-network-prediction-patch.json")));
				if (!String.Equals(Marker.GetStringField("manifest_sha256"), ManifestHash, StringComparison.OrdinalIgnoreCase))
				{
					throw new InvalidOperationException("Stale plugin staging marker: " + PluginName);
				}
				string Descriptor = PluginName + ".uplugin";
				RequireHash(Path.Combine(Root, Descriptor), Plugins.GetObjectField(PluginName).GetObjectField("baseline").GetStringField(Descriptor));
			}
			foreach (string Section in new[] { "patched", "overlay" })
			{
				JsonObject Files = Manifest.GetObjectField(Section);
				foreach (string Filename in Files.KeyNames)
				{
					RequireHash(Path.Combine(ProjectDirectory, "Plugins", "NetworkPrediction", Filename), Files.GetStringField(Filename));
				}
			}
		}
		catch (Exception Error)
		{
			throw new BuildException("NetworkPrediction patch is not prepared: " + Error.Message +
				" Run python Build/Patches/NetworkPrediction/prepare.py stage --engine <UE_5.8.2>/Engine from the project root. " +
				"For an existing override, run prepare.py verify and follow Build/Patches/NetworkPrediction/README.md.");
		}
	}

	private static void RequireHash(string Filename, string Expected)
	{
		string Actual = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(Filename)));
		if (!String.Equals(Actual, Expected, StringComparison.OrdinalIgnoreCase))
		{
			throw new InvalidOperationException("Patched source or plugin descriptor differs: " + Filename);
		}
	}
}
