ScUI = Project:CreateProject("ScUI", EngineLibrary)

function ScUI:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("UICore")
end

ScUI:SetupProject()