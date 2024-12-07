#pragma once

#include <resources/kraft_resource_types.h>

namespace kraft {
struct MaterialData
{
    bool                              AutoRelease;
    String                            Name;
    String                            FilePath;
    String                            ShaderAsset;
    HashMap<String, MaterialProperty> Properties;

    MaterialData()
    {
        Properties = HashMap<String, MaterialProperty>();
    }
};
}