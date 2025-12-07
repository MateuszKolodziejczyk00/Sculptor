IESProfileAssetTests = Project:CreateProject("IESProfileAssetTests", ETargetType.Application)

function IESProfileAssetTests:SetupConfiguration(configuration, platform)
    self:AddPrivateDependency("IESProfileAsset")
    self:AddPrivateDependency("GoogleTest")
    self:AddPrivateDependency("EngineCore")
    self:AddPrivateDependency("Graphics")
end

IESProfileAssetTests:SetupProject()