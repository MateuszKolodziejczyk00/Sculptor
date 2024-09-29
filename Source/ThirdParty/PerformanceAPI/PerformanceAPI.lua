PerformanceAPI = Project:CreateProject("PerformanceAPI", ETargetType.None)

function PerformanceAPI:SetupConfiguration(configuration, platform)
	self:AddPublicRelativeIncludePath("/include")

	self:CopyProjectLibToOutputDir("/dll/x64/PerformanceAPI.dll")
end

function PerformanceAPI:GetProjectFiles(configuration, platform)
	return
	{
		"PerformanceAPI/include/**"
	}
end

PerformanceAPI:SetupProject()