
function ShaderCompiler:GetPlatformDirectoryName()
    return "Shaderc"
end

function ShaderCompiler:GetPlatformAdditionalLibPaths()
    return
    {
        "$(VULKAN_SDK)/Lib"
    }
end

function ShaderCompiler:SetupPlatformConfiguration(configuration, platform)
    if configuration == EConfiguration.Debug
    then
        self:AddPrivateDependency("shaderc_combinedd")
    else
        self:AddPrivateDependency("shaderc_combined")
    end

    self:AddPrivateAbsoluteIncludePath("$(Vulkan_SDK)/Include")
end