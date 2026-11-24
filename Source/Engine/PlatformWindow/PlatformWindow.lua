PlatformWindow = Project:CreateProject("PlatformWindow", EngineLibrary)

function PlatformWindow:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("RHI")
    self:AddPublicDependency("UICore")
    self:AddPublicDependency("Input")

    self:AddPrivateDependency("ImGuiWin32Backend")

    if platform == EPlatform.Windows then
        self:AddPublicDefine("USE_WIN32=1")
    end
end

PlatformWindow:SetupProject()