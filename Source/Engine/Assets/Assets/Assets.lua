Assets = Project:CreateProject("Assets", ETargetType.None)

function Assets:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("IESProfileAsset")
    self:AddPublicDependency("TextureAsset")
    self:AddPublicDependency("MaterialAsset")
    self:AddPublicDependency("MeshAsset")
    self:AddPublicDependency("PrefabAsset")
end

Assets:SetupProject()
