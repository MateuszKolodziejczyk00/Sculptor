IESProfileAsset = Project:CreateProject("IESProfileAsset", EngineLibrary)

function IESProfileAsset:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("AssetsSystem")
    self:AddPublicDependency("RendererCore")

    self:AddPrivateDependency("Graphics")
    self:AddPrivateDependency("RenderGraph")
end

IESProfileAsset:SetupProject()