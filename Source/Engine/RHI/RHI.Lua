RHI = Project:CreateProject("RHI", ETargetType.SharedLibrary)

include "Vulkan/Vulkan"

function RHI:SetupConfiguration(configuration, platform)
    self:SetupRHIConfiguration(configuration, platform)

    self:AddPublicDependency("SculptorLib")
    self:AddPublicDependency("UICore")
end

function RHI:GetProjectFiles(configuration, platform)
    return
    {
        "RHIBidge/**.h",
        "RHIBidge/**.cpp",
        "RHICore/**.h",
        "RHICore/**.cpp",
        self:GetRHIDirectoryName() .. "/**.h",
        self:GetRHIDirectoryName() .. "/**.cpp",
        self:GetRHIDirectoryName() .. "/" .. self:GetRHIDirectoryName() .. ".lua",
        "RHI.lua"
    }
end

RHI:SetupProject()