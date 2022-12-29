EditorSandbox = Project:CreateProject("EditorSandbox", ETargetType.SharedLibrary)

function EditorSandbox:SetupConfiguration(configuration, platform)
	self:AddPublicDependency("ScUI")
	
	self:AddPrivateDependency("JobSystem")
	self:AddPrivateDependency("EngineCore")
	
	self:AddPrivateDependency("RenderScene")
end

EditorSandbox:SetupProject()