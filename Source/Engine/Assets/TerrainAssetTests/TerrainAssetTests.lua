TerrainAssetTests = Project:CreateProject("TerrainAssetTests", ETargetType.Application)

function TerrainAssetTests:SetupConfiguration(configuration, platform)
    self:AddPrivateDependency("TerrainAsset")
    self:AddPrivateDependency("GoogleTest")
    self:AddPrivateDependency("EngineCore")
    self:AddPrivateDependency("RenderScene")
end

TerrainAssetTests:SetupProject()
