SerializationTests = Project:CreateProject("SerializationTests", ETargetType.Application)

function SerializationTests:SetupConfiguration(configuration, platform)
    self:AddPrivateDependency("Serialization")
    self:AddPrivateDependency("GoogleTest")
end

SerializationTests:SetupProject()