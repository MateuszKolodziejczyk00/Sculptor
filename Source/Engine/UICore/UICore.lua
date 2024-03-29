UICore = Project:CreateProject("UICore", ETargetType.SharedLibrary)

function UICore:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("SculptorLib")
    self:AddPublicDependency("ImGui")
    self:AddPublicDependency("Input")
end

UICore:SetupProject()