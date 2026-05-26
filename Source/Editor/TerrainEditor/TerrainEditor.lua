TerrainEditor = Project:CreateProject("TerrainEditor", EngineLibrary)

function TerrainEditor:SetupConfiguration(configuration, platform)
	self:AddPublicDependency("EditorCommon")
	self:AddPublicDependency("ScUI")
	self:AddPublicDependency("TerrainAsset")
	self:AddPrivateDependency("Graphics")
end

TerrainEditor:SetupProject()
