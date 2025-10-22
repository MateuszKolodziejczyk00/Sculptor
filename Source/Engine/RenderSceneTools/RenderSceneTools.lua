RenderSceneTools = Project:CreateProject("RenderSceneTools", EngineLibrary)

function RenderSceneTools:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("RenderScene")
    self:AddPublicDependency("ScUI")
end

RenderSceneTools:SetupProject()