Graphics = Project:CreateProject("Graphics", EngineLibrary)

function Graphics:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("RendererCore")
    self:AddPublicDependency("RenderGraph")
    self:AddPublicDependency("JobSystem")
    self:AddPublicDependency("SculptorDLSS")
	
    self:AddPrivateDependency("STB")
    self:AddPrivateDependency("DDS")
end

Graphics:SetupProject()