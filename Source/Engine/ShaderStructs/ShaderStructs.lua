ShaderStructs = Project:CreateProject("ShaderStructs", EngineLibrary)

function ShaderStructs:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("SculptorLib")

    if configuration ~= EConfiguration.Release then
        self:AddPublicDefine("SHADER_STRUCTS_RTTI=1")
    else
        self:AddPublicDefine("SHADER_STRUCTS_RTTI=0")
    end

end

ShaderStructs:SetupProject()