Slang = Project:CreateProject("Slang", ETargetType.None)

function Slang:SetupConfiguration(configuration, platform)
	self:AddPublicRelativeIncludePath("/include")

	--self:CopyProjectLibToOutputDir("/bin/slang.dll")
	self:CopyProjectLibToOutputDir("/bin/slang-compiler.dll")

	self:SetPrecompiledLibsPath("/lib")
	self:AddPublicDependency("slang-compiler")
end

function Slang:GetProjectFiles(configuration, platform)
	return
	{
		"Slang/include/**"
	}
end

Slang:SetupProject()
