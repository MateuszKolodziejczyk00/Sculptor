PropertiesPanel = Project:CreateProject("PropertiesPanel", ETargetType.SharedLibrary)

function PropertiesPanel:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("SculptorLib")
    self:AddPublicDependency("RTTI")
    self:AddPublicDependency("UICore")
end

PropertiesPanel:SetupProject()
