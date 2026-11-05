RTTI = Project:CreateProject("RTTI", ETargetType.SharedLibrary)

function RTTI:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("SculptorLib")
end

RTTI:SetupProject()
