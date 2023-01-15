Input = Project:CreateProject("Input", ETargetType.SharedLibrary)

function Input:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("SculptorLib")
end

Input:SetupProject()