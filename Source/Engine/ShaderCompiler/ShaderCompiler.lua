ShaderCompiler= Project:CreateProject("ShaderCompiler", ETargetType.SharedLibrary)

include "Shaderc/Shaderc"

function ShaderCompiler:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("SculptorLib")
    self:AddPublicDependency("ShaderMetaData")

    self:AddPrivateDependency("EngineCore")
    self:AddPrivateDependency("Tokenizer")
    self:AddPrivateDependency("Serialization")

    self:AddLinkOption("-IGNORE:4099")

    if configuration == EConfiguration.Debug
    then
        self:AddPrivateDependency("spirv-cross-cored")
        self:AddPrivateDependency("spirv-cross-glsld")
        -- debug libs from VulkanSDK doesn't have associated pdbs which causes link warning in debug configuration
        self:AddLinkOption("-IGNORE:4099")
    else
        self:AddPrivateDependency("spirv-cross-cored")
        self:AddPrivateDependency("spirv-cross-glsld")
    end

    self:AddPrivateAbsoluteIncludePath("$(Vulkan_SDK)/Include")

    self:SetupPlatformConfiguration(configuration, platform)
end

function ShaderCompiler:GetAdditionalLibPaths()
    return 
    {
        self:GetPlatformAdditionalLibPath(),
        "$(VULKAN_SDK)/Lib" -- for spirv-cross
    }
end

function ShaderCompiler:GetProjectFiles(configuration, platform)
    return
    {
        "Common/**.h",
        "Common/**.cpp",
        "Bridge/**.h",
        "Bridge/**.cpp",
        self:GetPlatformDirectoryName() .. "/**.h",
        self:GetPlatformDirectoryName() .. "/**.cpp",
        self:GetPlatformDirectoryName() .. "/" .. self:GetPlatformDirectoryName() .. ".lua",
        "ShaderCompiler.lua"
    }
end

ShaderCompiler:SetupProject()