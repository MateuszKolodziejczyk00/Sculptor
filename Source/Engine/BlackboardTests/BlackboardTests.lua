BlackboardTests = Project:CreateProject("BlackboardTests", ETargetType.Application)

function BlackboardTests:SetupConfiguration(configuration, platform)
    self:AddPrivateDependency("Blackboard")
    self:AddPrivateDependency("GoogleTest")
end

BlackboardTests:SetupProject()