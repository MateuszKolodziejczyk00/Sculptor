MeshOptimizer = Project:CreateProject("MeshOptimizer", ETargetType.StaticLibrary)

function MeshOptimizer:SetupConfiguration(configuration, platform)
    self:AddPublicRelativeIncludePath("/src")
end

function MeshOptimizer:GetProjectFiles()
    return
    {
        "MeshOptimizer/src/**",
    }
end

MeshOptimizer:SetupProject()