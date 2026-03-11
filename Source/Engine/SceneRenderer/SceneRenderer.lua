SceneRenderer = Project:CreateProject("SceneRenderer", ETargetType.SharedLibrary)

function SceneRenderer:SetupConfiguration(configuration, platform)
	self:AddPrivateDependency("JobSystem")
	self:AddPublicDependency("RenderScene")

	self:AddPublicDefine("SPT_ENABLE_SCENE_RENDERER_STATS=1")
end

SceneRenderer:SetupProject()
