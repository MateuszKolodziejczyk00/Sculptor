SculptorDLSSVulkan = Project:CreateProjectInDirectory("DLSS/SculptorDLSSVulkan", "SculptorDLSSVulkan", ETargetType.SharedLibrary)

function SculptorDLSSVulkan:SetupConfiguration(configuration, platform)
    self:AddPrivateDependency("DLSS")
    self:AddPublicDependency("RenderGraph")

    self:AddPublicDefine("SPT_DLSS_VULKAN_BACKEND=1")
end

SculptorDLSSVulkan:SetupProject()