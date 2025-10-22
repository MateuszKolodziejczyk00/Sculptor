RenderScene = Project:CreateProject("RenderScene", EngineLibrary)

function RenderScene:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("Graphics")
    self:AddPublicDependency("SculptorECS")

    self:AddPublicDependency("Materials")

    self:AddPrivateDependency("TinyGLTF")
    self:AddPrivateDependency("MeshOptimizer")
    
    self:AddPrivateDependency("JobSystem")

    self:AddPublicDependency("Blackboard")

	self:AddPublicDefine("SPT_ENABLE_SCENE_RENDERER_STATS=1")
end

RenderScene:SetupProject()