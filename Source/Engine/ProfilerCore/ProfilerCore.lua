ProfilerCore = Project:CreateProject("ProfilerCore", ETargetType.SharedLibrary)

function ProfilerCore:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("SculptorCore")

    self:AddPrivateDependency("PerformanceAPI")
    self:AddPrivateDependency("PIX")

    self:AddPublicDefine("SPT_ENABLE_PROFILER=1")
    self:AddPrivateDefine("SPT_USE_PERFORMANCE_API=1")

    self:AddPrivateDefine("USE_PIX=1")
end

ProfilerCore:SetupProject()