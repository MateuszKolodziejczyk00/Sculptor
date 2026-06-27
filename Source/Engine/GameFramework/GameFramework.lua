GameFramework = Project:CreateProject("GameFramework", EngineLibrary)

function GameFramework:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("RenderScene")
    self:AddPublicDependency("Assets")
end

GameFramework:SetupProject()
