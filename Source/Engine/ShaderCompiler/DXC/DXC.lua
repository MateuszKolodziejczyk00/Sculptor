
function ShaderCompiler:GetPlatformDirectoryName()
    return "DXC"
end

function ShaderCompiler:GetPlatformAdditionalLibPath()
    return "$(VULKAN_SDK)/Lib"
end

function ShaderCompiler:SetupPlatformConfiguration(configuration, platform)
    self:AddPrivateDependency("dxcompiler")

    if configuration == EConfiguration.Debug
    then
        self:AddPrivateDependency("SPIRV-Toolsd")
        self:AddPrivateDependency("SPIRV-Tools-optd")
    else
        self:AddPrivateDependency("SPIRV-Tools")
        self:AddPrivateDependency("SPIRV-Tools-opt")
    end

    self:CopyLibToOutputDir("$(Vulkan_SDK)/Bin/dxcompiler.dll")
end