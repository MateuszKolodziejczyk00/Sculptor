Graphics = Project:CreateProject("Graphics", ETargetType.SharedLibrary)

function Graphics:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("RendererCore")
    self:AddPublicDependency("RenderGraph")
	
    self:AddPrivateDependency("JobSystem")
end

Graphics:SetupProject()