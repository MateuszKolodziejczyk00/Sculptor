PlatformWindow = Project:CreateProject("PlatformWindow", EngineLibrary)

function PlatformWindow:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("RHI")
    self:AddPublicDependency("UICore")
    self:AddPublicDependency("Input")

    self:AddPrivateDependency("ImGuiGLFWBackend")

    if platform == EPlatform.Windows then
        self:AddPrivateDependency("GLFW")

        self:AddPublicDefine("USE_GLFW=1")
    end
end

PlatformWindow:SetupProject()