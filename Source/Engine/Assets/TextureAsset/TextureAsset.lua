
TextureAsset = Project:CreateProject("TextureAsset", ETargetType.SharedLibrary)

function TextureAsset:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("AssetsSystem")
    self:AddPublicDependency("RendererCore")

    self:AddPrivateDependency("Graphics")
    self:AddPrivateDependency("RenderGraph")
end

TextureAsset:SetupProject()