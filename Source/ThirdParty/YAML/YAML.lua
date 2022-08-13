YAML = Project:CreateProject("YAML", ETargetType.SharedLibrary)

function YAML:SetupConfiguration(configuration, platform)
    self:AddPublicRelativeIncludePath("/include")

    self:AddPrivateDefine("yaml_cpp_EXPORTS")
    
    self:DisableWarnings()
end

function YAML:GetProjectFiles()
    return
    {
        "YAML/src/**",
        "YAML/include/**"
    }
end

YAML:SetupProject()