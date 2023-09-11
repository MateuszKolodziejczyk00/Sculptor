Materials = Project:CreateProject("Materials", ETargetType.SharedLibrary)

function Materials:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("Graphics")
    self:AddPublicDependency("SculptorECS")

    self:AddPrivateDependency("JobSystem")
end

Materials:SetupProject()