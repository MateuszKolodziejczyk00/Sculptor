EngineCore = Project:CreateProject("EngineCore", EngineLibrary)

function EngineCore:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("SculptorLib")
    self:AddPublicDependency("Serialization")
    self:AddPublicDependency("JobSystem")
    self:AddPublicDependency("AssetsSystem")

    if EngineLibrary == ETargetType.SharedLibrary then
        self:AddPublicDefine("SPT_BUILD_LIBS_AS_DLL=1");
    else
        self:AddPublicDefine("SPT_BUILD_LIBS_AS_DLL=0");
    end
end

EngineCore:SetupProject()