SculptorECS = Project:CreateProject("SculptorECS", ETargetType.SharedLibrary)

function SculptorECS:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("SculptorCore")
    self:AddPublicDependency("Math")
    self:AddPublicDependency("Entt")

    self:AddPublicDefine("ENTT_USE_ATOMIC=1")
end

SculptorECS:SetupProject()