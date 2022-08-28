EngineCore = Project:CreateProject("EngineCore", ETargetType.SharedLibrary)

function EngineCore:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("SculptorLib")
    self:AddPublicDependency("Serialization")
end

EngineCore:SetupProject()