Spdlog = Project:CreateProject("Spdlog", ETargetType.StaticLibrary)

function Spdlog:SetupConfiguration(configuration, platform)
    self:AddPublicDefine("SPDLOG_COMPILED_LIB")
    self:AddPublicDefine("SPDLOG_NO_EXCEPTIONS")

    self:AddPublicRelativeIncludePath("/include")
end

function Spdlog:GetProjectFiles()
    return
    {
        "Spdlog/src/**",
        "Spdlog/include/**"
    }
end

Spdlog:SetupProject()