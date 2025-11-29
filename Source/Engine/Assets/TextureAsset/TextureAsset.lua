TextureAsset = Project:CreateProject("TextureAsset", EngineLibrary)

function TextureAsset:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("AssetsSystem")
    self:AddPublicDependency("Graphics")
end

TextureAsset:SetupProject()