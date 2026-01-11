MaterialAsset = Project:CreateProject("MaterialAsset", EngineLibrary)

function MaterialAsset:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("AssetsSystem")
    self:AddPublicDependency("RendererCore")
    self:AddPublicDependency("Materials")

    self:AddPrivateDependency("Graphics")
    self:AddPrivateDependency("RenderScene")
end

MaterialAsset:SetupProject()
