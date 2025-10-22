DDC = Project:CreateProject("DDC", EngineLibrary)

function DDC:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("SculptorLib")
    self:AddPublicDependency("Serialization")
end

DDC:SetupProject()