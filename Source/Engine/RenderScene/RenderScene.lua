RenderScene = Project:CreateProject("RenderScene", ETargetType.SharedLibrary)

function RenderScene:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("Graphics")
    self:AddPublicDependency("SculptorECS")

    self:AddPrivateDependency("TinyGLTFLoader")
end

RenderScene:SetupProject()