function RHI:SetupRHIConfiguration(configuration, platform)

    self:AddPublicDependency("SculptorLib")
    self:AddPublicDependency("Volk")
    self:AddPublicDependency("VMA")

    self:AddPublicDependency("UICore")

    self:AddPrivateDependency("ImGuiVulkanBackend")

    self:AddPublicAbsoluteIncludePath("$(VULKAN_SDK)/Include")

    self:AddPublicDefine("SPT_VULKAN_RHI=1")

    -- Vulkan
    self:AddPrivateDefine("SPT_VULKAN_PREFER_DIFFERENT_QUEUE_FAMILIES=1")
end

function RHI:GetRHIDirectoryName()
    return "Vulkan"
end