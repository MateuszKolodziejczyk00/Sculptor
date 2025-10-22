Platform = Project:CreateProject("Platform", EngineLibrary)

function Platform:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("SculptorCore")
    self:AddPublicDependency("Math")
    self:AddPublicDependency("ProfilerCore")
end

function Platform:GetProjectFiles(configuration, platform)
    if platform == EPlatform.Windows then
        return
        {
            "Platform.lua",
            "*.h",
            "*.cpp",
            "Windows/**.h",
            "Windows/**.cpp"
        }
    end
end

Platform:SetupProject()