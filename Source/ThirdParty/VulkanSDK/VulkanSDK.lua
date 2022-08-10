VulkanSDK = Project:CreateProject("VulkanSDK", ETargetType.None)

function VulkanSDK:GetProjectFiles(configuration, platform)
    -- use variable because it seems that premakes file doesn't look for environment variables?
    vulkanPath = os.getenv("VULKAN_SDK")
    return
    {
        vulkanPath .. "/Include/**.h",
        vulkanPath .. "/Include/**.hpp",
    }
end

VulkanSDK:SetupProject()