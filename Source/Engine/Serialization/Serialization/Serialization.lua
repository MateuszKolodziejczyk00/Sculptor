Serialization = Project:CreateProject("Serialization", EngineLibrary)

function Serialization:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("SculptorLib")
    self:AddPublicDependency("YAML")
    self:AddPublicDependency("JSON")
end

Serialization:SetupProject()