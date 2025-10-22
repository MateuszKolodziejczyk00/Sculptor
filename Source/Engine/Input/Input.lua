Input = Project:CreateProject("Input", EngineLibrary)

function Input:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("SculptorLib")
end

Input:SetupProject()