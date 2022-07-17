Profiler = Project:CreateProject("Profiler", ETargetType.None)

function Profiler:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("Optick")

    self:AddPublicDefine("ENABLE_PROFILER=1")
end

Profiler:SetupProject()