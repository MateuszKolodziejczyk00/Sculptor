Math = Project:CreateProject("Math", ETargetType.None)

function Math:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("Eigen")
end

Math:SetupProject()