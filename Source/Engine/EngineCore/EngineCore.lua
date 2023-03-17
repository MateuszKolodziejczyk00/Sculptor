EngineCore = Project:CreateProject("EngineCore", ETargetType.SharedLibrary)

function EngineCore:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("SculptorLib")
    self:AddPublicDependency("Serialization")
    self:AddPublicDependency("JobSystem")
end

EngineCore:SetupProject()