PlatformWindow = Project:CreateProject("PlatformWindow", ETargetType.SharedLibrary)

function PlatformWindow:SetupConfiguration(configuration, platform)

    self:AddPublicDependency("RHI")
    self:AddPublicDependency("UICore")

    self:AddPrivateDependency("ImGuiGLFWBackend")

    if platform == EPlatform.Windows then
        self:AddPrivateDependency("GLFW")

        self:AddPublicDefine("USE_GLFW=1")
    end
end

PlatformWindow:SetupProject()