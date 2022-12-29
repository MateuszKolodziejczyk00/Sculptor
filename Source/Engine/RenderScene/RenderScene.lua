RenderScene = Project:CreateProject("RenderScene", ETargetType.SharedLibrary)

function RenderScene:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("Graphics")
    self:AddPublicDependency("SculptorECS")
end

RenderScene:SetupProject()