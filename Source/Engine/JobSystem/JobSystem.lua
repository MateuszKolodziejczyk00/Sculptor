JobSystem = Project:CreateProject("JobSystem", EngineLibrary)

function JobSystem:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("ProfilerCore")
    self:AddPublicDependency("SculptorLib")

    if configuration == EConfiguration.Debug then
        self:AddPublicDefine("DEBUG_JOB_SYSTEM=1")
    else
        self:AddPublicDefine("DEBUG_JOB_SYSTEM=0")
    end
end

JobSystem:SetupProject()