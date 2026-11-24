ImGuiWin32Backend = Project:CreateProject("ImGuiWin32Backend", ETargetType.StaticLibrary)

function ImGuiWin32Backend:SetupConfiguration(configuration, platform)
    self:AddPublicRelativeIncludePath("backends")    

    self:AddPublicDependency("ImGui")

    self:AddPublicDefine("IMGUI_BACKEND_BUILD_NO_DLL")

    if platform == EPlatform.Windows then
        self:AddPrivateDefine("IMGUI_PLATFORM_WINDOWS")
    end
end

function ImGuiWin32Backend:GetProjectFiles(configuration, platform)
    return
    {
        "ImGui/backends/imgui_impl_win32.h",
        "ImGui/backends/imgui_impl_win32.cpp"
    }
end

ImGuiWin32Backend:SetupProject()
