CloudscapeAsset = Project:CreateProject("CloudscapeAsset", EngineLibrary)

function CloudscapeAsset:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("AssetsSystem")
    self:AddPublicDependency("RenderScene")

    self:AddPrivateDependency("RendererCore")
    self:AddPrivateDependency("Graphics")
end

CloudscapeAsset:SetupProject()
