ShaderMetaData = Project:CreateProject("ShaderMetaData", ETargetType.None)

function ShaderMetaData:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("SculptorLib")
    self:AddPublicDependency("Serialization")
end

ShaderMetaData:SetupProject()
