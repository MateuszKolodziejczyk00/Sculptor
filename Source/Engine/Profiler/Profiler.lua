Profiler = Project:CreateProject("Profiler", ETargetType.SharedLibrary)

function Profiler:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("SculptorLib")

	self:AddPrivateDependency("EngineCore")
end

Profiler:SetupProject()