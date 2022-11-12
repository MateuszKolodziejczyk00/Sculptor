NsightAftermath = Project:CreateProject("NsightAftermath", ETargetType.None)

function NsightAftermath:SetupConfiguration(configuration, platform)
	self:AddPublicRelativeIncludePath("/include")
 
	self:CopyLibToOutputDir("/lib/x64/GFSDK_Aftermath_Lib.x64.dll")
	self:CopyLibToOutputDir("/lib/x64/GFSDK_Aftermath_Lib.x64.lib")

	self:SetPrecompiledLibsPath("/lib/x64/")
	
	self:SetLinkLibNames("GFSDK_Aftermath_Lib.x64")
end

function NsightAftermath:GetProjectFiles(configuration, platform)
	return
	{
		"NsightAftermath/include/**"
	}
end

NsightAftermath:SetupProject()