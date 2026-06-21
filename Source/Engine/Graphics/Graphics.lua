Graphics = Project:CreateProject("Graphics", EngineLibrary)

function Graphics:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("RendererCore")
    self:AddPublicDependency("RenderGraph")
    self:AddPublicDependency("JobSystem")
    self:AddPublicDependency("SculptorDLSS")

    self:AddPrivateDependency("STB")
    self:AddPrivateDependency("DDS")
    self:AddPrivateDependency("TinyEXR")
    self:AddPrivateDependency("TinyTIFF")
end

Graphics:SetupProject()
