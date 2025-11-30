JSON = Project:CreateProject("JSON", ETargetType.None)

function JSON:SetupConfiguration(configuration, platform)
    self:AddPublicRelativeIncludePath("/include")
end

function JSON:GetProjectFiles(configuration, platform)
    return
    {
        "JSON/include/**.hpp"
    }
end

JSON:SetupProject()