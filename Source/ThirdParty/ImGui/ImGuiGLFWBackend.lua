ImGuiGLFWBackend = Project:CreateProject("ImGuiGLFWBackend", ETargetType.StaticLibrary)

function ImGuiGLFWBackend:SetupConfiguration(configuration, platform)
    self:AddPublicRelativeIncludePath("backends")    

    self:AddPublicDependency("ImGui")
    self:AddPublicDependency("GLFW")

    self:AddPublicDefine("IMGUI_BACKEND_BUILD_NO_DLL")

    if platform == EPlatform.Windows then
        self:AddPrivateDefine("IMGUI_PLATFORM_WINDOWS")
    end
end

function ImGuiGLFWBackend:GetProjectFiles(configuration, platform)
    return
    {
        "ImGui/backends/imgui_impl_glfw.h",
        "ImGui/backends/imgui_impl_glfw.cpp"
    }
end

ImGuiGLFWBackend:SetupProject()