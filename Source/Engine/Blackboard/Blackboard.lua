Blackboard = Project:CreateProject("Blackboard", EngineLibrary)

function Blackboard:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("SculptorLib")
    self:AddPublicDependency("Serialization")
end

Blackboard:SetupProject()