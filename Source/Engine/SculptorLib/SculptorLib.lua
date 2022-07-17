SculptorLib = Project:CreateProject("SculptorLib", ETargetType.SharedLibrary)

function SculptorLib:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("SculptorCore")
    self:AddPublicDependency("Math")
    self:AddPublicDependency("Spdlog")
end

SculptorLib:SetupProject()