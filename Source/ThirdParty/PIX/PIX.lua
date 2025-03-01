PIX = Project:CreateProject("PIX", ETargetType.None)

function PIX:SetupConfiguration(configuration, platform)
	self:AddPublicRelativeIncludePath("/include")

	self:CopyProjectLibToOutputDir("/bin/x64/WinPixEventRuntime.dll")

    self:SetPrecompiledLibsPath("/bin/x64")
    self:AddPublicDependency("WinPixEventRuntime")
end

function PIX:GetProjectFiles(configuration, platform)
	return
	{
		"PIX/include/**"
	}
end

PIX:SetupProject()