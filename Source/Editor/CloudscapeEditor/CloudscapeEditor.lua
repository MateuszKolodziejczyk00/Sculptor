CloudscapeEditor = Project:CreateProject("CloudscapeEditor", EngineLibrary)

function CloudscapeEditor:SetupConfiguration(configuration, platform)
	self:AddPublicDependency("EditorCommon")
	self:AddPublicDependency("ScUI")
	self:AddPublicDependency("CloudscapeAsset")
	self:AddPrivateDependency("Graphics")
end

CloudscapeEditor:SetupProject()
