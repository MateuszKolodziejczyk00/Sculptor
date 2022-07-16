Spdlog = Project:CreateProject("Spdlog", ETargetType.StaticLibrary, EProjectType.ThirdParty)

function Spdlog:SetupConfiguration(configuration, platform)
    self:AddPublicDefine("SPDLOG_COMPILED_LIB")
    self:AddPublicDefine("SPDLOG_NO_EXCEPTIONS")
end

function Spdlog:GetIncludesRelativeLocation()
    return "/include"
end

function Spdlog:GetProjectFiles()
    return
    {
        "Spdlog/src/**",
        "Spdlog/include/**"
    }
end

Spdlog:SetupProject()