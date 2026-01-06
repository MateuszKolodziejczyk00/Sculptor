MeshAssetTests = Project:CreateProject("MeshAssetTests", ETargetType.Application)

function MeshAssetTests:SetupConfiguration(configuration, platform)
    self:AddPrivateDependency("MeshAsset")
    self:AddPrivateDependency("GoogleTest")
    self:AddPrivateDependency("EngineCore")
    self:AddPrivateDependency("RenderScene")
end

MeshAssetTests:SetupProject()