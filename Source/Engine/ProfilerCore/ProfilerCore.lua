ProfilerCore = Project:CreateProject("ProfilerCore", ETargetType.None)

function ProfilerCore:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("Optick")

    self:AddPublicDefine("ENABLE_PROFILER=1")
    self:AddPublicDefine("USE_OPTICK=1")
end

ProfilerCore:SetupProject()