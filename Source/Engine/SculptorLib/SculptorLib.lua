SculptorLib = Project:CreateProject("SculptorLib", EngineLibrary)

function SculptorLib:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("SculptorCore")
    self:AddPublicDependency("Math")
    self:AddPublicDependency("Spdlog")
    self:AddPublicDependency("ProfilerCore")
    self:AddPublicDependency("Platform")

    self:AddPrivateDependency("TileableVolumeNoise")
    self:AddPrivateDependency("Ws2_32.lib")

    self:AddPublicDefine("WITH_LOGGER=1")

    self:AddPublicDefine("DO_CHECKS=1")
end

SculptorLib:SetupProject()