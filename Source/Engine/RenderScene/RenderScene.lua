RenderScene = Project:CreateProject("RenderScene", ETargetType.SharedLibrary)

function RenderScene:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("Graphics")
    self:AddPublicDependency("SculptorECS")

    self:AddPrivateDependency("TinyGLTF")
    self:AddPrivateDependency("MeshOptimizer")
    
    self:AddPrivateDependency("JobSystem")
end

RenderScene:SetupProject()