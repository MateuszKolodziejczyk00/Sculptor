Profiler = Project:CreateProject("Profiler", ETargetType.SharedLibrary)

function Profiler:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("SculptorLib")
    self:AddPublicDependency("ScUI")
    
    self:AddPublicDependency("RendererCore")

	self:AddPrivateDependency("EngineCore")
end

Profiler:SetupProject()