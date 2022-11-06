RenderGraph = Project:CreateProject("RenderGraph", ETargetType.SharedLibrary)

function RenderGraph:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("RendererCore")

	self:AddPrivateDependency("EngineCore")

    self:AddPublicDefine("DEBUG_RENDER_GRAPH=1")
end

RenderGraph:SetupProject()