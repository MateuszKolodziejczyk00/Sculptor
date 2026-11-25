SceneAsset = Project:CreateProject("SceneAsset", EngineLibrary)

function SceneAsset:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("AssetsSystem")
    self:AddPublicDependency("PrefabAsset")
    self:AddPublicDependency("TerrainAsset")
    self:AddPublicDependency("CloudscapeAsset")
    self:AddPublicDependency("RenderScene")
end

SceneAsset:SetupProject()
