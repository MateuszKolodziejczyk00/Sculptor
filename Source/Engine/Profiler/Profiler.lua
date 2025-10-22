Profiler = Project:CreateProject("Profiler", EngineLibrary)

function Profiler:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("SculptorLib")
    self:AddPublicDependency("ScUI")
    
    self:AddPublicDependency("RendererCore")

	self:AddPrivateDependency("EngineCore")
end

Profiler:SetupProject()