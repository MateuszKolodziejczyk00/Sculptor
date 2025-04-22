DDC = Project:CreateProject("DDC", ETargetType.SharedLibrary)

function DDC:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("SculptorLib")
    self:AddPublicDependency("Serialization")
end

DDC:SetupProject()