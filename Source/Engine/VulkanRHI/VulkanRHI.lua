VulkanRHI = Project:CreateProject("VulkanRHI", ETargetType.SharedLibrary)

function VulkanRHI:SetupConfiguration(configuration, platform)

    self:AddPublicDependency("SculptorLib")
    self:AddPublicDependency("RHICore")
    self:AddPublicDependency("Volk")
    self:AddPublicDependency("VMA")

    self:AddPublicDependency("UICore")

    self:AddPrivateDependency("ImGuiVulkanBackend")

    self:AddPublicAbsoluteIncludePath("$(VULKAN_SDK)/Include")

    -- Validation
    self:AddPublicDefine("VULKAN_VALIDATION=1")
    self:AddPrivateDefine("VULKAN_VALIDATION_STRICT=1")
    self:AddPrivateDefine("VULKAN_VALIDATION_STRICT_GPU_ASSISTED=1")
    self:AddPrivateDefine("VULKAN_VALIDATION_STRICT_BEST_PRACTICES=1")
    self:AddPrivateDefine("VULKAN_VALIDATION_STRICT_DEBUG_PRINTF=0")
    self:AddPrivateDefine("VULKAN_VALIDATION_STRICT_SYNCHRONIZATION=1")

    -- Vulkan
    self:AddPrivateDefine("VULKAN_PREFER_DIFFERENT_QUEUE_FAMILIES=1")
end

VulkanRHI:SetupProject()