#pragma once

#include <resources/kraft_resource_types.h>

namespace kraft {

// This format is used just for loading materials and not the actual format
struct MaterialDataIntermediateFormat
{
    bool                                  AutoRelease;
    String                                Name;
    String                                FilePath;
    String                                ShaderAsset;
    FlatHashMap<String, MaterialProperty> Properties;

    MaterialDataIntermediateFormat()
    {
        Properties = FlatHashMap<String, MaterialProperty>();
    }
};

} // namespace kraft