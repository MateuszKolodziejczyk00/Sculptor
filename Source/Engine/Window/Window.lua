Window = Project:CreateProject("Window", ETargetType.SharedLibrary)

function Window:SetupConfiguration(configuration, platform)

    self:AddPublicDependency("RendererTypes")

    self:AddPrivateDependency("ImGuiGLFWBackend")

    if platform == EPlatform.Windows then
        self:AddPrivateDependency("GLFW")

        self:AddPublicDefine("USE_GLFW=1")
    end
end

Window:SetupProject()