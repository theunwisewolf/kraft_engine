#pragma once

#include <resources/kraft_resource_types.h>

namespace kraft {

// This format is used just for loading materials and not the actual format
struct MaterialDataIntermediateFormat
{
    bool                               AutoRelease;
    String8                            name;
    String8                            filepath;
    String8                            shader_asset;
    FlatHashMap<u64, MaterialProperty> properties;

    MaterialDataIntermediateFormat()
    {
        properties = FlatHashMap<u64, MaterialProperty>();
    }
};

} // namespace kraft