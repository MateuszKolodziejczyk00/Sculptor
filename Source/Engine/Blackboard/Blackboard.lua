Blackboard = Project:CreateProject("Blackboard", ETargetType.SharedLibrary)

function Blackboard:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("SculptorLib")
    self:AddPublicDependency("Serialization")
end

Blackboard:SetupProject()