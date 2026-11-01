RenderTests = Project:CreateProject("RenderTests", ETargetType.Application)

function RenderTests:SetupConfiguration(configuration, platform)
	self:AddPrivateDependency("ToolsFramework")
	self:AddPrivateDependency("GameFramework")
	self:AddPrivateDependency("SceneRenderer")
end

RenderTests:SetupProject()
