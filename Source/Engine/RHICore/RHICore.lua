RHICore = Project:CreateProject("RHICore", ETargetType.None)

function RHICore:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("SculptorLib")
end

RHICore:SetupProject()