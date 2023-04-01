EditorSandbox = Project:CreateProject("EditorSandbox", ETargetType.SharedLibrary)

function EditorSandbox:SetupConfiguration(configuration, platform)
	self:AddPublicDependency("ScUI")
	
	self:AddPrivateDependency("JobSystem")
	self:AddPrivateDependency("EngineCore")
	
	self:AddPrivateDependency("Profiler")
	
	self:AddPublicDependency("RenderScene")
	self:AddPublicDependency("RenderSceneTools")
end

EditorSandbox:SetupProject()