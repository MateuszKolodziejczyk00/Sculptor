ShaderCompiler= Project:CreateProject("ShaderCompiler", ETargetType.SharedLibrary)

include "Shaderc/Shaderc"

function ShaderCompiler:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("SculptorLib")

    self:AddPrivateDependency("Tokenizer")
    self:AddPrivateDependency("Serialization")

    self:SetupPlatformConfiguration(configuration, platform)
end

function ShaderCompiler:GetAdditionalLibPaths()
    return self:GetPlatformAdditionalLibPaths()
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