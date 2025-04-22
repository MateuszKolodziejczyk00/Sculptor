TextureAssetTests = Project:CreateProject("TextureAssetTests", ETargetType.Application)

function TextureAssetTests:SetupConfiguration(configuration, platform)
    self:AddPrivateDependency("TextureAsset")
    self:AddPrivateDependency("GoogleTest")
    self:AddPrivateDependency("EngineCore")
end

TextureAssetTests:SetupProject()