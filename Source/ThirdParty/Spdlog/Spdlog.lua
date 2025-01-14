Spdlog = Project:CreateProject("Spdlog", ETargetType.StaticLibrary)

function Spdlog:SetupConfiguration(configuration, platform)
    self:AddPublicDefine("SPDLOG_COMPILED_LIB")
    self:AddPublicDefine("SPDLOG_NO_EXCEPTIONS")

    self:AddPublicRelativeIncludePath("/include")

	self:AddPublicDefine("_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING=1")

	self:AddPublicDefine("FMT_UNICODE=0")
end

function Spdlog:GetProjectFiles()
    return
    {
        "Spdlog/src/**",
        "Spdlog/include/**"
    }
end

Spdlog:SetupProject()