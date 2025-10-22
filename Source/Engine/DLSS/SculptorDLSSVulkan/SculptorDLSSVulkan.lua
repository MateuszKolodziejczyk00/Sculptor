SculptorDLSSVulkan = Project:CreateProject("SculptorDLSSVulkan", EngineLibrary)

function SculptorDLSSVulkan:SetupConfiguration(configuration, platform)
    self:AddPrivateDependency("DLSS")
    self:AddPublicDependency("RenderGraph")

    self:AddPublicDefine("SPT_DLSS_VULKAN_BACKEND=1")

    self:AddPublicDefine("NGX_ENABLE_DEPRECATED_SHUTDOWN=1")
end

SculptorDLSSVulkan:SetupProject()