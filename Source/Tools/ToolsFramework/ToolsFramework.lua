ToolsFramework = Project:CreateProject("ToolsFramework", ETargetType.StaticLibrary)

function ToolsFramework:SetupConfiguration(configuration, platform)
	self:AddPublicDependency("SculptorLib")
	self:AddPublicDependency("EngineCore")
	self:AddPublicDependency("JobSystem")
	self:AddPublicDependency("RendererCore")
	self:AddPublicDependency("RenderGraph")
	self:AddPublicDependency("Graphics")
	self:AddPublicDependency("UICore")
	self:AddPublicDependency("ScUI")
	self:AddPublicDependency("RenderGraphCapturer")
end

ToolsFramework:SetupProject()
