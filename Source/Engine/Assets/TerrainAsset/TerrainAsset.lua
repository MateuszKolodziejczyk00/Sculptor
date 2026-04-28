TerrainAsset = Project:CreateProject("TerrainAsset", EngineLibrary)

function TerrainAsset:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("AssetsSystem")
    self:AddPublicDependency("MaterialAsset")
    self:AddPublicDependency("RenderScene")

    self:AddPrivateDependency("RendererCore")
    self:AddPrivateDependency("Graphics")
end

TerrainAsset:SetupProject()
