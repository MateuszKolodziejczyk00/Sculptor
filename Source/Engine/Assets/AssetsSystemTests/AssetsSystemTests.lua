AssetsSystemTests = Project:CreateProject("AssetsSystemTests", ETargetType.Application)

function AssetsSystemTests:SetupConfiguration(configuration, platform)
    self:AddPrivateDependency("AssetsSystem")
    self:AddPrivateDependency("GoogleTest")
    self:AddPrivateDependency("EngineCore")
end

AssetsSystemTests:SetupProject()