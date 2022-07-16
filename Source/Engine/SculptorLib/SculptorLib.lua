SculptorLib = Project:CreateProject("SculptorLib", ETargetType.SharedLibrary, EProjectType.Engine)

function SculptorLib:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("Math")
end

SculptorLib:SetupProject()