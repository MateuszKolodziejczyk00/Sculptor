RenderGraph = Project:CreateProject("RenderGraph", ETargetType.SharedLibrary)

function RenderGraph:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("RendererCore")

	self:AddPrivateDependency("EngineCore")

    if configuration ~= EConfiguration.Release then
        self:AddPublicDefine("DEBUG_RENDER_GRAPH=1")
    end
end

RenderGraph:SetupProject()