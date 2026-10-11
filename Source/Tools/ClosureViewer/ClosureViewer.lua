ClosureViewer = Project:CreateProject("ClosureViewer", ETargetType.Application)

function ClosureViewer:SetupConfiguration(configuration, platform)
	self:AddPrivateDependency("ToolsFramework")
end

ClosureViewer:SetupProject()
