ScUI = Project:CreateProject("ScUI", ETargetType.SharedLibrary)

function ScUI:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("UICore")
end

ScUI:SetupProject()