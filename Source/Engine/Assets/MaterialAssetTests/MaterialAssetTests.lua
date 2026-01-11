MaterialAssetTests = Project:CreateProject("MaterialAssetTests", ETargetType.Application)

function MaterialAssetTests:SetupConfiguration(configuration, platform)
    self:AddPrivateDependency("MaterialAsset")
    self:AddPrivateDependency("GoogleTest")
    self:AddPrivateDependency("EngineCore")
    self:AddPrivateDependency("RenderScene")
end

MaterialAssetTests:SetupProject()