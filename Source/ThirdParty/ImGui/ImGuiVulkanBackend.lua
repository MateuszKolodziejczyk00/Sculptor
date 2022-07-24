ImGuiVulkanBackend = Project:CreateProjectInDirectory("ImGui", "ImGuiVulkanBackend", ETargetType.SharedLibrary)

function ImGuiVulkanBackend:SetupConfiguration(configuration, platform)
    self:AddPublicRelativeIncludePath("/backends")    

    self:AddPublicDependency("ImGui")
    self:AddPublicDependency("Volk")

    self:AddPublicAbsoluteIncludePath("$(VULKAN_SDK)/Include")

    self:AddPrivateDefine("IMGUI_BACKEND_BUILD_DLL=1")

    self:AddPrivateDefine("IMGUI_VULKAN=1")
    self:AddPrivateDefine("VK_NO_PROTOTYPES")

    if platform == EPlatform.Windows then
        self:AddPrivateDefine("IMGUI_PLATFORM_WINDOWS")
    end
end

function ImGuiVulkanBackend:GetProjectFiles(configuration, platform)
    return
    {
        "ImGui/backends/imgui_impl_vulkan.h",
        "ImGui/backends/imgui_impl_vulkan.cpp"
    }
end

ImGuiVulkanBackend:SetupProject()