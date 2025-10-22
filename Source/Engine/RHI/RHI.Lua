RHI = Project:CreateProject("RHI", EngineLibrary)

forceRHIValidation = false
useNsightAftermath = true

if GetSelectedRHI() == ERHI.Vulkan then
    include "Vulkan/Vulkan"
end

function RHI:SetupConfiguration(configuration, platform)
    self:SetupRHIConfiguration(configuration, platform)

    self:AddPublicDependency("SculptorLib")
    self:AddPublicDependency("UICore")
    self:AddPublicDependency("EngineCore")

    if configuration ~= EConfiguration.Release or forceRHIValidation then
        self:AddPublicDefine("SPT_RHI_DEBUG=1")
    else
        self:AddPublicDefine("SPT_RHI_DEBUG=0")
    end
     
    if useNsightAftermath then
        self:AddPublicDefine("SPT_ENABLE_GPU_CRASH_DUMPS=1")
        self:AddPrivateDefine("SPT_ENABLE_NSIGHT_AFTERMATH=1")
        self:AddPrivateDependency("NsightAftermath")
    else
        self:AddPublicDefine("SPT_ENABLE_GPU_CRASH_DUMPS=0")
        self:AddPrivateDefine("SPT_ENABLE_NSIGHT_AFTERMATH=0")
    end
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