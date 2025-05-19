
TileableVolumeNoise = Project:CreateProject("TileableVolumeNoise", ETargetType.None)

function TileableVolumeNoise:SetupConfiguration(configuration, platform)
	self:SetPrecompiledLibsPath("/Lib/release")

    self:AddPublicDependency("TileableVolumeNoise_MD")
end

function TileableVolumeNoise:GetProjectFiles(configuration, platform)
	return
	{
		"TileableVolumeNoise/TileableVolumeNoise.h",
		"TileableVolumeNoise/TileableVolumeNoise.cpp",
	}
end

TileableVolumeNoise:SetupProject()