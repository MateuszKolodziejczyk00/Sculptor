SculptorECS = Project:CreateProject("SculptorECS", ETargetType.None)

function SculptorECS:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("Entt")
end

SculptorECS:SetupProject()