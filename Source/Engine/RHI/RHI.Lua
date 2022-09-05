RHI = Project:CreateProject("RHI", ETargetType.SharedLibrary)

if GetSelectedRHI() == ERHI.Vulkan then
    include "Vulkan/Vulkan"
end

function RHI:SetupConfiguration(configuration, platform)
    self:SetupRHIConfiguration(configuration, platform)

    self:AddPublicDependency("SculptorLib")
    self:AddPublicDependency("UICore")
end

function RHI:GetProjectFiles(configuration, platform)
    return
    {
        "RHIBridge/**.h",
        "RHIBridge/**.cpp",
        "RHICore/**.h",
        "RHICore/**.cpp",
        self:GetRHIDirectoryName() .. "/**.h",
        self:GetRHIDirectoryName() .. "/**.cpp",
        self:GetRHIDirectoryName() .. "/" .. self:GetRHIDirectoryName() .. ".lua",
        "RHI.lua"
    }
end

RHI:SetupProject()