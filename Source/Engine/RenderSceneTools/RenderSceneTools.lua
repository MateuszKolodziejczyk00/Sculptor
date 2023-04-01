RenderSceneTools = Project:CreateProject("RenderSceneTools", ETargetType.SharedLibrary)

function RenderSceneTools:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("RenderScene")
    self:AddPublicDependency("ScUI")
end

RenderSceneTools:SetupProject()