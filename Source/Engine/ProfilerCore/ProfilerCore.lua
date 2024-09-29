ProfilerCore = Project:CreateProject("ProfilerCore", ETargetType.SharedLibrary)

function ProfilerCore:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("SculptorCore")
    self:AddPublicDefine("SPT_ENABLE_PROFILER=1")

    self:AddPrivateDependency("PerformanceAPI")
    self:AddPrivateDefine("SPT_USE_PERFORMANCE_API=1")
end

ProfilerCore:SetupProject()