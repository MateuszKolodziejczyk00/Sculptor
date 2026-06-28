EditorSandbox = Project:CreateProject("EditorSandbox", EngineLibrary)

function EditorSandbox:SetupConfiguration(configuration, platform)
	self:AddPublicDependency("EditorCommon")
	self:AddPublicDependency("ScUI")
	self:AddPublicDependency("TerrainEditor")
	
	self:AddPrivateDependency("JobSystem")
	self:AddPrivateDependency("EngineCore")
	
	self:AddPrivateDependency("Profiler")
	self:AddPrivateDependency("RenderGraphCapturer")
	
	self:AddPublicDependency("RenderSceneTools")

	self:AddPublicDependency("Assets")

	self:AddPublicDependency("GameFramework")
end

EditorSandbox:SetupProject()
