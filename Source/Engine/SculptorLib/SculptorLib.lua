SculptorLib = Project:CreateProject("SculptorLib", ETargetType.SharedLibrary)

function SculptorLib:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("SculptorCore")
    self:AddPublicDependency("Math")
    self:AddPublicDependency("Spdlog")
    self:AddPublicDependency("ProfilerCore")
    self:AddPublicDependency("Platform")

    self:AddPublicDefine("WITH_LOGGER=1")

    self:AddPublicDefine("DO_CHECKS=1")
end

SculptorLib:SetupProject()