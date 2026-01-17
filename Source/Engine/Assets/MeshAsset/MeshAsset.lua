MeshAsset = Project:CreateProject("MeshAsset", EngineLibrary)

function MeshAsset:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("AssetsSystem")
    self:AddPublicDependency("RendererCore")

    self:AddPrivateDependency("Graphics")
    self:AddPrivateDependency("RenderScene")
end

MeshAsset:SetupProject()
