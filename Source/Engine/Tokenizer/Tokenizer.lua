Tokenizer = Project:CreateProject("Tokenizer", ETargetType.None)

function Tokenizer:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("SculptorLib")
end

Tokenizer:SetupProject()