
function ShaderCompiler:GetPlatformDirectoryName()
    return "Shaderc"
end

function ShaderCompiler:GetPlatformAdditionalLibPath()
    return "$(VULKAN_SDK)/Lib"
end

function ShaderCompiler:SetupPlatformConfiguration(configuration, platform)
    if configuration == EConfiguration.Debug
    then
        self:AddPrivateDependency("shaderc_combinedd")
    else
        self:AddPrivateDependency("shaderc_combined")
    end
end