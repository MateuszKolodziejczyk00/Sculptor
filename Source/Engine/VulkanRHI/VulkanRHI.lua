VulkanRHI = Project:CreateProject("VulkanRHI", ETargetType.SharedLibrary)

function VulkanRHI:SetupConfiguration(configuration, platform)

    self:AddPublicDependency("SculptorLib")
    self:AddPublicDependency("RHICore")

    self:AddPublicDependency("Volk")

    self:AddPrivateDependency("VMA")

    self:AddPublicDefine("VULKAN_RHI=1")

    self:AddPublicDefine("VULKAN_VALIDATION=1")
    self:AddPublicDefine("VULKAN_VALIDATION_STRICT=1")
    self:AddPublicDefine("VULKAN_VALIDATION_STRICT_GPU_ASSISTED=1")
    self:AddPublicDefine("VULKAN_VALIDATION_STRICT_BEST_PRACTICES=1")
    self:AddPublicDefine("VULKAN_VALIDATION_STRICT_DEBUG_PRINTF=0")
    self:AddPublicDefine("VULKAN_VALIDATION_STRICT_SYNCHRONIZATION=1")

    self:AddPublicAbsoluteIncludePath("$(VULKAN_SDK)/Include")
end

VulkanRHI:SetupProject()