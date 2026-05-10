TinyEXR = Project:CreateProject("TinyEXR", ETargetType.None)


function TinyEXR:SetupConfiguration(configuration, platform)
    self:AddPublicRelativeIncludePath("deps/miniz")
end


function TinyEXR:GetProjectFiles(configuration, platform)
    return
    {
        "TinyEXR/tinyexr.h",
    }
end

TinyEXR:SetupProject()
