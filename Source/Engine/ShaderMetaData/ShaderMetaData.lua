ShaderMetaData = Project:CreateProject("ShaderMetaData", ETargetType.None)

function ShaderMetaData:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("SculptorLib")
end

ShaderMetaData:SetupProject()