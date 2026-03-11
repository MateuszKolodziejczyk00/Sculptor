SculptorECS = Project:CreateProject("SculptorECS", EngineLibrary)

function SculptorECS:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("SculptorLib")
    self:AddPublicDependency("Entt")
    self:AddPublicDependency("EngineCore")

    self:AddPublicDefine("ENTT_USE_ATOMIC=1")
end

SculptorECS:SetupProject()
