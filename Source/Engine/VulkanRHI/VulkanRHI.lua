VulkanRHI = Project:CreateProject("VulkanRHI", ETargetType.SharedLibrary)

function VulkanRHI:SetupConfiguration(configuration, platform)

    self:AddPublicDependency("SculptorLib")

    self:AddPrivateDependency("Volk")
    self:AddPrivateDependency("VMA")

    self:AddPublicDefine("VULKAN_RHI")
end

function VulkanRHI:GetIncludePathsToThisProject()
    return
    {
        self.referenceProjectLocation,
        "%{VULKAN_SDK}/Include"
    }
end

function VulkanRHI:GetPrivateIncludePaths()
    return
    {
        "%{VULKAN_SDK}/Include"
    }
end

VulkanRHI:SetupProject()