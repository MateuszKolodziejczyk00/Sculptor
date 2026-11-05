SDDTests = Project:CreateProject("SDDTests", ETargetType.Application)

function SDDTests:SetupConfiguration(configuration, platform)
    self:AddPrivateDependency("Serialization")
    self:AddPrivateDependency("RendererCore")
    self:AddPrivateDependency("GoogleTest")
    self:AddPrivateDependency("RTTI")
end

SDDTests:SetupProject()
