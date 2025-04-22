AssetsSystem = Project:CreateProject("AssetsSystem", ETargetType.SharedLibrary)

function AssetsSystem:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("DDC")
    self:AddPublicDependency("Blackboard")
    self:AddPublicDependency("SculptorLib")
end

AssetsSystem:SetupProject()