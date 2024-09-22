JobSystemTests = Project:CreateProject("JobSystemTests", ETargetType.Application)

function JobSystemTests:SetupConfiguration(configuration, platform)
    self:AddPrivateDependency("JobSystem")
    self:AddPrivateDependency("GoogleTest")
end

JobSystemTests:SetupProject()