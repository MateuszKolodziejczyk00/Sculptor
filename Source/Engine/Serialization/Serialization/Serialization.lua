Serialization = Project:CreateProject("Serialization", EngineLibrary)

function Serialization:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("SculptorLib")
    self:AddPublicDependency("JSON")
end

Serialization:SetupProject()