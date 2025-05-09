Graphics = Project:CreateProject("Graphics", ETargetType.SharedLibrary)

function Graphics:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("RendererCore")
    self:AddPublicDependency("RenderGraph")
    self:AddPublicDependency("JobSystem")
    self:AddPublicDependency("SculptorDLSS")
	
    self:AddPrivateDependency("STB")
    self:AddPrivateDependency("DDS")
end

Graphics:SetupProject()