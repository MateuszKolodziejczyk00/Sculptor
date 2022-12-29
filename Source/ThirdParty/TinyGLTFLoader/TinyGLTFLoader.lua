TinyGLTFLoader = Project:CreateProject("TinyGLTFLoader", ETargetType.None)

function TinyGLTFLoader:GetProjectFiles(configuration, platform)
    return
    {
        "TinyGLTFLoader/stb_image.h",
        "TinyGLTFLoader/tiny_gltf_loader.h"
    }
end

TinyGLTFLoader:SetupProject()