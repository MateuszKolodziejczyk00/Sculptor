ShaderCompiler= Project:CreateProject("ShaderCompiler", ETargetType.SharedLibrary)

if GetSelectedRHI() == ERHI.Vulkan then
    if GetSelectedShaderCompiler() == EShaderCompiler.Shaderc then
        include "Shaderc/Shaderc"
    elseif GetSelectedShaderCompiler() == EShaderCompiler.DXC then
        include "DXC/DXC"
    end
end

function ShaderCompiler:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("SculptorLib")
    self:AddPublicDependency("ShaderMetaData")
	self:AddPublicDependency("ShaderStructs")

    self:AddPrivateDependency("EngineCore")
    self:AddPrivateDependency("Tokenizer")
    self:AddPrivateDependency("Serialization")
    self:AddPrivateDependency("RHI") -- this is currently necessary for some types in meta-data

    self:AddLinkOption("-IGNORE:4099")

    if configuration == EConfiguration.Debug
    then
        self:AddPrivateDependency("spirv-cross-cored")
        self:AddPrivateDependency("spirv-cross-glsld")
        -- debug libs from VulkanSDK doesn't have associated pdbs which causes link warning in debug configuration
        self:AddLinkOption("-IGNORE:4099")
    else
        self:AddPrivateDependency("spirv-cross-core")
        self:AddPrivateDependency("spirv-cross-glsl")
    end

    self:AddPrivateAbsoluteIncludePath("$(Vulkan_SDK)/Include")

    self:SetupPlatformConfiguration(configuration, platform)

    self:AddPublicDefine("SPT_SHADERS_DEBUG_FEATURES=1")
    self:AddPublicDefine("WITH_SHADERS_HOT_RELOAD=1")
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