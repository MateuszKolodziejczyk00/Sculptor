Assets = Project:CreateProject("Assets", ETargetType.None)

function Assets:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("IESProfileAsset")
    self:AddPublicDependency("TextureAsset")
    self:AddPublicDependency("MaterialAsset")
    self:AddPublicDependency("TerrainAsset")
    self:AddPublicDependency("CloudscapeAsset")
    self:AddPublicDependency("MeshAsset")
    self:AddPublicDependency("PrefabAsset")
    self:AddPublicDependency("SceneAsset")
end

Assets:SetupProject()
