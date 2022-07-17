VulkanRHI = Project:CreateProject("VulkanRHI", ETargetType.SharedLibrary)

function VulkanRHI:SetupConfiguration(configuration, platform)

    self:AddPublicDependency("SculptorLib")

    self:AddPrivateDependency("Volk")
    self:AddPrivateDependency("VMA")

    self:AddPublicDefine("VULKAN_RHI")

    self:AddPublicAbsoluteIncludePath("$(VULKAN_SDK)/Include")
end

VulkanRHI:SetupProject()