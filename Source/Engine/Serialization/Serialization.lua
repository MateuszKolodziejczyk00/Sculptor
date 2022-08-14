Serialization = Project:CreateProject("Serialization", ETargetType.None)

function Serialization:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("SculptorLib")
    self:AddPublicDependency("YAML")
end

Serialization:SetupProject()