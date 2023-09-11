SculptorECS = Project:CreateProject("SculptorECS", ETargetType.SharedLibrary)

function SculptorECS:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("SculptorCore")
    self:AddPublicDependency("Math")
    self:AddPublicDependency("Entt")
end

SculptorECS:SetupProject()