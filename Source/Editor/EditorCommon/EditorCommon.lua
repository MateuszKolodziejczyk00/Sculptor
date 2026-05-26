EditorCommon = Project:CreateProject("EditorCommon", EngineLibrary)

function EditorCommon:SetupConfiguration(configuration, platform)
	self:AddPublicDependency("EngineCore")

	self:AddPrivateDependency("RendererCore")
	self:AddPrivateDependency("UICore")
end

EditorCommon:SetupProject()
