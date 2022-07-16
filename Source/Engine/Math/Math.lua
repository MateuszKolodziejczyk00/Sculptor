Math = Project:CreateProject("Math", ETargetType.None, EProjectType.Engine)

function Math:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("Eigen")
end

Math:SetupProject()