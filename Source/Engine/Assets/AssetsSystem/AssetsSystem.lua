AssetsSystem = Project:CreateProject("AssetsSystem", EngineLibrary)

function AssetsSystem:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("DDC")
    self:AddPublicDependency("Blackboard")
    self:AddPublicDependency("SculptorLib")
end

AssetsSystem:SetupProject()