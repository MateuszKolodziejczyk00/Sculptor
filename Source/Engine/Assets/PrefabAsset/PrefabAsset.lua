PrefabAsset = Project:CreateProject("PrefabAsset", EngineLibrary)

function PrefabAsset:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("AssetsSystem")
    self:AddPublicDependency("RendererCore")

    self:AddPublicDependency("MeshAsset")
    self:AddPublicDependency("MaterialAsset")

    self:AddPrivateDependency("RenderScene")
end

PrefabAsset:SetupProject()
