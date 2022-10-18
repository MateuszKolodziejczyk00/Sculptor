JobSystem = Project:CreateProject("JobSystem", ETargetType.SharedLibrary)

function JobSystem:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("ProfilerCore")
    self:AddPublicDependency("SculptorCore")
end

JobSystem:SetupProject()