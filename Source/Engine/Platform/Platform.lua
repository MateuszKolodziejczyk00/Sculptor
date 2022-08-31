Platform = Project:CreateProject("Platform", ETargetType.SharedLibrary)

function Platform:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("SculptorCore")
    self:AddPublicDependency("Math")
    self:AddPublicDependency("Profiler")
end

function GLFW:GetProjectFiles(configuration, platform)
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